#!/usr/bin/env node
// Pack / unpack / list OpenGD asset bundles (.akrile) using web/akrile.js.
//
//   node tools/akrile-tool.mjs pack   <Geometry_Dash.apk | assets_dir> [-o Resources.akrile] [--lean] [--keep-music] [--no-levels]
//   node tools/akrile-tool.mjs unpack <bundle.akrile> [-o Resources]
//   node tools/akrile-tool.mjs list   <bundle.akrile>
//
// --lean   drops what the game does not use yet: SD twins of existing -hd files, fps images
//          and all level music except menuLoop.mp3 (add --keep-music to keep the music).
// Official levels of GD 1.x are not files: they are plain-text strings inside lib/*/libgame.so. When packing an APK
// they are extracted as level_<track>.txt (level_extra_<n>.txt if the level has no header); --no-levels skips this.
// Needs Node >= 18. No npm dependencies (APKs are read with a tiny built-in zip reader).
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const KEEP_EXT = /\.(png|plist|fnt|mp3|ogg|wav|ttf|json|txt)$/i;
const STORE_EXT = /\.(mp3|ogg|png)$/i; // already compressed: DEFLATE only costs time

async function loadAkrile() {
  globalThis.window = globalThis;
  vm.runInThisContext(fs.readFileSync(path.join(here, '..', 'web', 'akrile.js'), 'utf8'), { filename: 'akrile.js' });
  await globalThis.Akrile.ready;
  return globalThis.Akrile;
}

// ---- minimal ZIP reader (stored + deflate) ---------------------------------
function readZip(buf) {
  let eocd = -1;
  for (let i = buf.length - 22; i >= Math.max(0, buf.length - 65557); i--)
    if (buf.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error('not a zip/apk file (no end-of-central-directory record)');
  const count = buf.readUInt16LE(eocd + 10);
  let p = buf.readUInt32LE(eocd + 16);
  const entries = [];
  for (let n = 0; n < count; n++) {
    if (buf.readUInt32LE(p) !== 0x02014b50) throw new Error('corrupt central directory');
    const method = buf.readUInt16LE(p + 10);
    const csize = buf.readUInt32LE(p + 20);
    const nlen = buf.readUInt16LE(p + 28), elen = buf.readUInt16LE(p + 30), clen = buf.readUInt16LE(p + 32);
    const lho = buf.readUInt32LE(p + 42);
    const name = buf.toString('utf8', p + 46, p + 46 + nlen);
    entries.push({ name, method, csize, lho });
    p += 46 + nlen + elen + clen;
  }
  return entries.map(e => ({
    name: e.name,
    data() {
      const nlen = buf.readUInt16LE(e.lho + 26), elen = buf.readUInt16LE(e.lho + 28);
      const start = e.lho + 30 + nlen + elen;
      const raw = buf.subarray(start, start + e.csize);
      if (e.method === 0) return raw;
      if (e.method === 8) return zlib.inflateRawSync(raw);
      throw new Error(`unsupported zip method ${e.method} for ${e.name}`);
    },
  }));
}

function* walk(dir, base = dir) {
  for (const ent of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, ent.name);
    if (ent.isDirectory()) yield* walk(full, base);
    else yield { name: path.relative(base, full).split(path.sep).join('/'), data: () => fs.readFileSync(full) };
  }
}

function collect(input) {
  const st = fs.statSync(input);
  if (st.isDirectory()) {
    // accept either the assets folder itself or an unpacked APK that contains assets/
    const root = fs.existsSync(path.join(input, 'assets')) ? path.join(input, 'assets') : input;
    return [...walk(root)];
  }
  const entries = readZip(fs.readFileSync(input));
  const inApk = entries.some(e => e.name.startsWith('assets/'));
  collect.lastEntries = inApk ? entries : null;
  return entries
    .filter(e => !e.name.endsWith('/'))
    .filter(e => (inApk ? e.name.startsWith('assets/') : true))
    .map(e => ({ name: inApk ? e.name.slice(7) : e.name.replace(/^Resources\//, ''), data: e.data }));
}


// ---- level extraction from libgame.so --------------------------------------
// A level is a long run of printable ASCII containing many ';'-separated "1,<id>,2,<x>,..." objects.
function extractLevels(so) {
  const out = [];
  let start = -1;
  const flush = end => {
    if (start < 0) return;
    if (end - start >= 500) {
      const str = so.toString('latin1', start, end);
      let semis = 0;
      for (let i = 0; i < str.length; i++) if (str.charCodeAt(i) === 59) semis++;
      if (semis > 20 && /(^|;)1,\d+,2,\d+/.test(str)) out.push(str);
    }
    start = -1;
  };
  for (let i = 0; i < so.length; i++) {
    const c = so[i];
    if (c >= 0x20 && c <= 0x7e) { if (start < 0) start = i; } else flush(i);
  }
  flush(so.length);
  return out;
}

function levelFiles(entries) {
  const used = new Set(), files = [];
  let extra = 0;
  for (const e of entries.filter(e => /^lib\/[^/]+\/libgame\.so$/.test(e.name))) {
    for (const lvl of extractLevels(e.data())) {
      const m = /^k[^;]*?kA1,(\d+)/.exec(lvl.split(';')[0]);
      let name = m ? `level_${m[1]}.txt` : `level_extra_${extra++}.txt`;
      while (used.has(name)) name = name.replace(/(\.txt)$/, '_b$1');
      used.add(name);
      const buf = Buffer.from(lvl, 'latin1');
      files.push({ name, data: () => buf });
    }
    break;  // one ABI is enough: the data is identical in every libgame.so
  }
  return files;
}

function applyLean(files, keepMusic) {
  const names = new Set(files.map(f => f.name));
  return files.filter(f => {
    const n = f.name;
    const hd = n.replace(/(\.[^.]+)$/, '-hd$1');
    if (!/-hd\./.test(n) && names.has(hd)) return false;         // SD twin of an -hd file
    if (/-ipadhd\./.test(n)) return false;
    if (/^fps_images/.test(n)) return false;
    if (!keepMusic && /\.mp3$/i.test(n) && n !== 'menuLoop.mp3') return false;
    return true;
  });
}

const fmt = n => (n / 1048576).toFixed(2) + ' MB';

async function main() {
  const [cmd, input, ...rest] = process.argv.slice(2);
  const flag = f => rest.includes(f);
  const opt = (f, d) => { const i = rest.indexOf(f); return i >= 0 ? rest[i + 1] : d; };
  if (!cmd || !input) {
    console.error('usage: akrile-tool.mjs pack|unpack|list <input> [-o output] [--lean] [--keep-music]');
    process.exit(2);
  }
  const Akrile = await loadAkrile();

  if (cmd === 'pack') {
    let files = collect(input).filter(f => KEEP_EXT.test(f.name));
    if (!files.some(f => /^GJ_LaunchSheet(-hd)?\.plist$/.test(f.name)))
      throw new Error('GJ_LaunchSheet.plist not found - this does not look like a Geometry Dash 1.x APK / assets folder');
    if (flag('--lean')) files = applyLean(files, flag('--keep-music'));
    if (!flag('--no-levels') && collect.lastEntries) {
      const lv = levelFiles(collect.lastEntries);
      files = files.concat(lv);
      console.log(`levels: extracted ${lv.length} from libgame.so (${lv.map(l => l.name).join(', ') || 'none'})`);
    }
    const z = new Akrile();
    let raw = 0;
    for (const f of files.sort((a, b) => a.name.localeCompare(b.name))) {
      const d = f.data();
      raw += d.length;
      z.file(f.name, new Uint8Array(d.buffer, d.byteOffset, d.length), { compression: STORE_EXT.test(f.name) ? 'STORE' : 'DEFLATE' });
    }
    const out = await z.generateAsync({ type: 'uint8array' });
    const dest = opt('-o', 'Resources.akrile');
    fs.writeFileSync(dest, out);
    console.log(`${files.length} files, ${fmt(raw)} -> ${dest} (${fmt(out.length)}, ${(out.length / raw * 100).toFixed(1)}%)`);
  } else if (cmd === 'unpack' || cmd === 'list') {
    const z = await Akrile.loadAsync(new Uint8Array(fs.readFileSync(input)));
    const names = Object.keys(z.files).filter(n => !n.endsWith('/'));
    if (cmd === 'list') { names.forEach(n => console.log(n)); console.log(`${names.length} files`); return; }
    const dest = opt('-o', 'Resources');
    for (const n of names) {
      const target = path.resolve(dest, n);
      if (n.includes('..') || path.isAbsolute(n) || !target.startsWith(path.resolve(dest) + path.sep))
        throw new Error('refusing unsafe path in archive: ' + n);
      fs.mkdirSync(path.dirname(target), { recursive: true });
      fs.writeFileSync(target, await z.file(n).async('uint8array'));
    }
    console.log(`unpacked ${names.length} files into ${dest}/`);
  } else {
    console.error('unknown command: ' + cmd);
    process.exit(2);
  }
}

main().catch(e => { console.error('error: ' + e.message); process.exit(1); });
