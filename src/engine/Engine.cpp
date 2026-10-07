#include "engine/Engine.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/Atlas.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace fs = std::filesystem;

namespace ogd {

Engine& E() {
    static Engine e;
    return e;
}

static std::string withSuffix(const std::string& name, const char* suffix) {
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return name + suffix;
    return name.substr(0, dot) + suffix + name.substr(dot);
}

static bool endsWith(const std::string& s, const std::string& e) {
    return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}

bool Engine::init(const std::string& root, const std::string& savePath) {
    savePath_ = savePath;

    // index every file under the resource root by basename (works for flat and reorganised layouts)
    std::error_code ec;
    if (fs::exists(root, ec)) {
        for (auto& e : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
            if (e.is_regular_file(ec)) index_[e.path().filename().string()] = e.path().string();
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    win = SDL_CreateWindow("OpenGD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kW, kH,
                           SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { SDL_Log("CreateWindow failed: %s", SDL_GetError()); return false; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { SDL_Log("CreateRenderer failed: %s", SDL_GetError()); return false; }
    SDL_RenderSetLogicalSize(ren, kW, kH);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    IMG_Init(IMG_INIT_PNG);
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
        audioOk = true;
        Mix_AllocateChannels(16);
    } else {
        SDL_Log("Audio disabled: %s", Mix_GetError());
    }
    save.load(savePath_);
    return true;
}

void Engine::shutdown() {
    for (auto& kv : music_) if (kv.second) Mix_FreeMusic(kv.second);
    for (auto& kv : sfx_) if (kv.second) Mix_FreeChunk(kv.second);
    for (auto& kv : textures_) if (kv.second) SDL_DestroyTexture(kv.second);
    if (audioOk) Mix_CloseAudio();
    IMG_Quit();
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}

void Engine::persist() {
    save.save(savePath_);
#ifdef __EMSCRIPTEN__
    EM_ASM({ if (Module.FS) Module.FS.syncfs(false, function (e) {}); });
#endif
}

// Resolves "foo.png" to the best available variant: foo-hd.png, then foo.png.
std::string Engine::resolve(const std::string& name, bool* isHd) const {
    auto hd = index_.find(withSuffix(name, "-hd"));
    if (hd != index_.end()) { if (isHd) *isHd = true; return hd->second; }
    auto sd = index_.find(name);
    if (sd != index_.end()) { if (isHd) *isHd = endsWith(name, "-hd" + fs::path(name).extension().string()); return sd->second; }
    return {};
}

bool Engine::readText(const std::string& name, std::string& out) const {
    std::string p = resolve(name);
    if (p.empty()) return false;
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

SDL_Texture* Engine::loadTexture(const std::string& name, float* scale) {
    bool hd = false;
    std::string path = resolve(name, &hd);
    if (scale) *scale = hd ? 1.f : 2.f;
    if (path.empty()) return nullptr;
    auto it = textures_.find(path);
    if (it != textures_.end()) return it->second;
    SDL_Texture* t = IMG_LoadTexture(ren, path.c_str());
    if (!t) SDL_Log("Failed to load %s: %s", path.c_str(), IMG_GetError());
    else SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    textures_[path] = t;
    return t;
}

std::vector<std::string> Engine::sheetNames() const {
    std::vector<std::string> out;
    for (auto& kv : index_) {
        const std::string& n = kv.first;
        if (!endsWith(n, ".plist") || endsWith(n, "-uhd.plist")) continue;
        if (endsWith(n, "-hd.plist")) {
            std::string base = n.substr(0, n.size() - 9) + ".plist";
            if (index_.count(base)) continue; // base name will resolve to -hd anyway
            out.push_back(base);
        } else {
            out.push_back(n);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool Engine::loadSheet(const std::string& plistName) {
    std::string xml;
    if (!readText(plistName, xml)) return false;
    Sheet sh = parseSheet(xml);
    if (!sh.ok || sh.textureFile.empty()) return false;

    float scale = 1.f;
    SDL_Texture* tex = loadTexture(fs::path(sh.textureFile).filename().string(), &scale);
    if (!tex) return false;
    // the plist we resolved may be the SD one while its texture name is exact; trust the texture we found
    for (auto& f : sh.frames) {
        Sprite s;
        s.tex = tex;
        s.rotated = f.rotated;
        s.src = {(int)f.x, (int)f.y, (int)(f.rotated ? f.h : f.w), (int)(f.rotated ? f.w : f.h)};
        s.w = f.w * scale;       s.h = f.h * scale;
        s.srcW = f.srcW * scale; s.srcH = f.srcH * scale;
        s.offX = f.offX * scale; s.offY = f.offY * scale;
        sprites_[f.name] = s;
        auto p = f.name.find("-hd.");
        if (p != std::string::npos) sprites_[f.name.substr(0, p) + f.name.substr(p + 3)] = s;
    }
    return true;
}

Sprite Engine::sprite(const std::string& name) {
    auto it = sprites_.find(name);
    if (it != sprites_.end()) return it->second;
    float scale = 1.f;
    SDL_Texture* t = loadTexture(name, &scale);
    Sprite s;
    if (t) {
        int w = 0, h = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        s.tex = t;
        s.src = {0, 0, w, h};
        s.w = s.srcW = w * scale;
        s.h = s.srcH = h * scale;
    }
    sprites_[name] = s;
    return s;
}

Font* Engine::font(const std::string& fntName) {
    auto it = fonts_.find(fntName);
    if (it != fonts_.end()) return it->second.tex ? &it->second : nullptr;
    Font& f = fonts_[fntName];
    std::string txt;
    if (!readText(fntName, txt)) return nullptr;
    f.bm = parseBMFont(txt);
    if (!f.bm.ok) return nullptr;
    // the .fnt we resolved is -hd if available; its page file usually has no suffix
    bool hd = false;
    resolve(fntName, &hd);
    f.scale = hd ? 1.f : 2.f;
    float ignored;
    f.tex = loadTexture(fs::path(f.bm.pageFile).filename().string(), &ignored);
    return f.tex ? &f : nullptr;
}

Mix_Music* Engine::music(const std::string& name) {
    auto it = music_.find(name);
    if (it != music_.end()) return it->second;
    Mix_Music* m = nullptr;
    std::string p = resolve(name);
    if (!p.empty() && audioOk) {
        m = Mix_LoadMUS(p.c_str());
        if (!m) SDL_Log("Failed to load music %s: %s", p.c_str(), Mix_GetError());
    }
    music_[name] = m;
    return m;
}

void Engine::playMusic(const std::string& name, int loops) {
    if (!audioOk) return;
    // music off = the track still runs, just muted, so switching it back on mid-level is in sync
    Mix_VolumeMusic(save.musicOn ? MIX_MAX_VOLUME : 0);
    if (Mix_Music* m = music(name)) Mix_PlayMusic(m, loops);
}

void Engine::stopMusic() {
    if (audioOk) Mix_HaltMusic();
}

void Engine::pauseMusic() {
    if (audioOk) Mix_PauseMusic();
}

void Engine::resumeMusic() {
    if (audioOk) Mix_ResumeMusic();
}

void Engine::setMusicOn(bool on) {
    save.musicOn = on;
    if (!audioOk) return;
    Mix_VolumeMusic(on ? MIX_MAX_VOLUME : 0);
}

void Engine::setFxOn(bool on) { save.fxOn = on; }

void Engine::playSfx(const std::string& name, int volume) {
    if (!audioOk || !save.fxOn) return;
    auto it = sfx_.find(name);
    if (it == sfx_.end()) {
        Mix_Chunk* c = nullptr;
        std::string p = resolve(name);
        if (!p.empty()) {
            c = Mix_LoadWAV(p.c_str());
            if (!c) SDL_Log("Failed to load sound %s: %s", p.c_str(), Mix_GetError());
        }
        it = sfx_.emplace(name, c).first;
    }
    if (it->second) {
        Mix_VolumeChunk(it->second, volume);
        Mix_PlayChannel(-1, it->second, 0);
    }
}

// ---------------------------------------------------------------- drawing

void Engine::clear(Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
    SDL_RenderClear(ren);
}

void Engine::setClip(float x, float y, float w, float h) {
    SDL_Rect r{(int)std::floor(x), (int)std::floor(kH - y - h), (int)std::ceil(w), (int)std::ceil(h)};
    SDL_RenderSetClipRect(ren, &r);
}

void Engine::clearClip() { SDL_RenderSetClipRect(ren, nullptr); }

void Engine::fillRect(float x, float y, float w, float h, Color c, Uint8 a) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a);
    SDL_FRect r{x, kH - y - h, w, h};
    SDL_RenderFillRectF(ren, &r);
}

void Engine::drawSprite(const Sprite& s, float cx, float cy, float sx, float sy, float rot, Color col, Uint8 alpha,
                        bool flipX, bool flipY) {
    if (!s.tex) return;
    if (sx < 0) { flipX = !flipX; sx = -sx; }
    if (sy < 0) { flipY = !flipY; sy = -sy; }
    // trim offset, mirrored when flipped, scaled and rotated (clockwise rotation, y-up)
    float ox = s.offX * sx * (flipX ? -1.f : 1.f), oy = s.offY * sy * (flipY ? -1.f : 1.f);
    float rad = rot * 3.14159265358979f / 180.f;
    float c = std::cos(rad), sn = std::sin(rad);
    float px = cx + ox * c + oy * sn;
    float py = cy - ox * sn + oy * c;

    float dw = s.w * sx, dh = s.h * sy;

    double angle = rot;
    bool fh = flipX, fv = flipY;
    if (s.rotated) {
        std::swap(dw, dh);   // footprint in the atlas is transposed
        angle -= 90.0;
        std::swap(fh, fv);
    }
    SDL_FRect dst{px - dw / 2, (kH - py) - dh / 2, dw, dh};
    SDL_SetTextureColorMod(s.tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    int flip = SDL_FLIP_NONE;
    if (fh) flip |= SDL_FLIP_HORIZONTAL;
    if (fv) flip |= SDL_FLIP_VERTICAL;
    SDL_RenderCopyExF(ren, s.tex, &s.src, &dst, angle, nullptr, (SDL_RendererFlip)flip);
}

void Engine::drawSpriteCropped(const Sprite& s, float leftX, float cy, float fraction, Color col, float scale) {
    if (!s.tex || s.rotated) return;
    fraction = std::max(0.f, std::min(1.f, fraction));
    int sw = (int)(s.src.w * fraction);
    if (sw <= 0) return;
    SDL_Rect src{s.src.x, s.src.y, sw, s.src.h};
    float k = s.w / s.src.w;
    SDL_FRect dst{leftX, (kH - cy) - s.h * scale / 2, sw * k * scale, s.h * scale};
    SDL_SetTextureColorMod(s.tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(s.tex, 255);
    SDL_RenderCopyF(ren, s.tex, &src, &dst);
}

void Engine::drawSpriteTiledX(const Sprite& s, float leftX, float cy, float width, Color col) {
    if (!s.tex || s.rotated || width <= 0 || s.w <= 0) return;
    SDL_SetTextureColorMod(s.tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(s.tex, 255);
    const float k = s.w / s.src.w;
    for (float x = 0; x < width; x += s.w) {
        const float w = std::min(s.w, width - x);
        SDL_Rect src{s.src.x, s.src.y, std::max(1, (int)(w / k)), s.src.h};
        SDL_FRect dst{leftX + x, (kH - cy) - s.h / 2, w, s.h};
        SDL_RenderCopyF(ren, s.tex, &src, &dst);
    }
}

void Engine::drawText(Font* f, const std::string& text, float x, float y, float scale, Align a, Color col, Uint8 alpha) {
    if (!f || !f->tex) return;
    const float k = scale * f->scale;
    float total = f->bm.measure(text) * k;
    float cursor = (a == Align::Left) ? x : (a == Align::Center ? x - total / 2 : x - total);
    float top = (kH - y) - (f->bm.lineHeight * k) / 2;  // vertically centred on y
    SDL_SetTextureColorMod(f->tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(f->tex, alpha);
    int prev = 0;
    for (unsigned char ch : text) {
        auto it = f->bm.chars.find(ch);
        if (it == f->bm.chars.end()) continue;
        const BMChar& c = it->second;
        if (prev) cursor += f->bm.kern(prev, ch) * k;
        if (c.w > 0 && c.h > 0) {
            SDL_Rect src{c.x, c.y, c.w, c.h};
            SDL_FRect dst{cursor + c.xoff * k, top + c.yoff * k, c.w * k, c.h * k};
            SDL_RenderCopyF(ren, f->tex, &src, &dst);
        }
        cursor += c.xadv * k;
        prev = ch;
    }
}

// 9-slice panel: corners keep their size, edges/centre stretch.
void Engine::drawPanel(const Sprite& s, float cx, float cy, float w, float h, Color col, Uint8 alpha) {
    if (!s.tex || s.rotated) return;
    const int b = std::max(1, s.src.w / 4);            // border in texture pixels
    const float k = s.w / s.src.w;                     // texture px -> design units
    const float bd = b * k;
    const int xs[4] = {s.src.x, s.src.x + b, s.src.x + s.src.w - b, s.src.x + s.src.w};
    const int ys[4] = {s.src.y, s.src.y + b, s.src.y + s.src.h - b, s.src.y + s.src.h};
    const float dx[4] = {cx - w / 2, cx - w / 2 + bd, cx + w / 2 - bd, cx + w / 2};
    const float dyTop = (kH - cy) - h / 2;
    const float dy[4] = {dyTop, dyTop + bd, dyTop + h - bd, dyTop + h};
    SDL_SetTextureColorMod(s.tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i) {
            SDL_Rect src{xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j]};
            SDL_FRect dst{dx[i], dy[j], dx[i + 1] - dx[i], dy[j + 1] - dy[j]};
            SDL_RenderCopyF(ren, s.tex, &src, &dst);
        }
}

} // namespace ogd
