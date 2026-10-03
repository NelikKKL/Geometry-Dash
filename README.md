# OpenGD

Open-source C++17 reimplementation of **Geometry Dash 1.0**, built directly on SDL2 (no game engine).
Runs natively (Linux/Windows/macOS) and in the browser via Emscripten/WebAssembly.

> Game assets (sprites, fonts, music) are **not** part of this repository and are not distributed here.
> You need your own copy of the Geometry Dash 1.x APK.

## Status

| Feature | State |
|---|---|
| Loading screen, sprite sheets, bitmap fonts, main menu, popups | done |
| Level select (7 official levels, best progress saved), layout measured from a device screenshot | done |
| Garage / icon kit: 13 cube icons, 12 colours (main + secondary), name editing, locked entries with unlock hints | done |
| Level parser (plain text and base64+gzip/zlib) | done, 7 official levels load with 0 warnings |
| Gameplay: cube, ship, gravity portals, pads, orbs, blocks/slabs, spikes, colour triggers | done (see "Accuracy") |
| Death burst + restart, level complete screen, progress bar, music, sound effects | done |
| Creator, options, stats, achievements, practice mode | placeholder popups / TODO |
| Decoration with no confident sprite match (ids 15, 22-24, 26, 27, 32, 33, 41) | not drawn |

## Accuracy: what is verified and what is approximate

* **Verified by data:** the level format; the id -> sprite mapping for blocks, spikes, portals, pads/orbs and the
  white/black ground decoration (derived from the data, see `src/core/ObjectTable.h`); cube jump physics are the
  constants of the original OpenGD code (jump height ~2.2 blocks).
* **Approximate (tuned, not copied from the original):** hitbox sizes, the 1.5-unit tolerance when squeezing
  through 1-block gaps, ship physics, pad/orb strength. `id 9` (black ground thorns) is treated as decoration:
  in several levels they cover the whole floor of stretches that have to be run on.
* **Checked by the look-ahead bot** (`sim_bot`, horizon 250 frames): Stereo Madness is completable in the simulation,
  including both ship sections. The other levels were not proven completable (the greedy bot gets stuck in dead
  ends, which says nothing about the physics); play-test them.
* **Ship** uses the documented ship gravity of 25 blocks/s^2 (GeometryPhysics project) for both thrust and fall, and a
  speed cap that limits the climb angle to 45 degrees (the first version was 3x too quick). The same document gives
  a cube launch speed that matches the simulation to 0.1%, but its cube gravity is inconsistent (79 b/s^2 in the
  formula, 72 in the text, the simulation uses 93), so the cube is unchanged.
* **Unlocks** are this project's own design (the original uses achievements, which do not exist here): icons 1-4 and
  colours 1-4 are free; completing level N unlocks icon N+4 and colour N+4; icon 12 needs 4 completed levels,
  icon 13 and colour 12 need all 7. The rules are one small table in `src/core/Unlocks.cpp`. Hidden debug key in the
  garage: <kbd>U</kbd> toggles "unlock everything" (not saved).
* Not implemented: practice mode, checkpoints, the level-end wall, dual mode, speed portals, other cube/ship icons.

## Assets

The game looks for files by **basename** anywhere under the resource directory, so both the flat APK layout
and any sub-folder layout work. `-hd` variants are preferred.

Pack your APK once into a single `.akrile` bundle (needs Node >= 18, no npm packages):

```bash
node tools/akrile-tool.mjs pack Geometry_Dash.apk -o Resources.akrile            # everything (~14 MB)
node tools/akrile-tool.mjs pack Geometry_Dash.apk -o Resources.akrile --lean     # ~2.5 MB: drops SD twins, promo art, level music
node tools/akrile-tool.mjs list   Resources.akrile
node tools/akrile-tool.mjs unpack Resources.akrile -o Resources                  # for the native build
```

The official levels are not files in the APK; they are text strings inside `lib/*/libgame.so`. `pack` extracts
them as `level_<track>.txt` (add `--no-levels` to skip). Note: audio and PNGs are already compressed, so a
bundle is a convenience (one file) rather than a size win; `--lean` is what actually shrinks it.

* **Web:** open the page and choose the `.akrile` (an APK/ZIP also works). It is unpacked in the browser and
  cached in IndexedDB; nothing leaves your machine. If a `Resources.akrile` is served next to `index.html`
  it is picked up automatically (use this only for private deployments).
* **Native:** unpack the bundle (or copy the APK's `assets/`) into `./Resources/`.

Bundles contain copyrighted material: `*.akrile`, `*.apk` and `Resources/` are git-ignored. Do not publish them.

## Level data

`src/core/Level.h` parses level text (`header;object;object;...`) and base64(gzip/zlib) strings. Coordinates are
in GD units (block = 30); on screen: `x_px = 2*x`, `y_px = 206 + 2*y`. Inspect a level with
`./build/leveldump Resources/level_0.txt` (`--objects` dumps every object). To run the test against real
levels: `OGD_LEVELS_DIR=Resources ctest --test-dir build --output-on-failure`.

## Build

```bash
# native (needs SDL2, SDL2_image, SDL2_mixer, pkg-config)
cmake -S . -B build && cmake --build build
ctest --test-dir build
./build/OpenGD [resources_dir] [save_file]

# web (needs the Emscripten SDK)
emcmake cmake -S . -B build-web && cmake --build build-web
python3 -m http.server -d build-web 8000     # open http://localhost:8000
```

Browser smoke test (needs your own APK, so it is not run in CI):

```bash
pip install playwright pillow && playwright install chromium
python3 tests/web_smoke.py build-web /path/to/Geometry_Dash_1_0.apk
```

Controls: mouse / touch / <kbd>Space</kbd> / <kbd>↑</kbd> to jump or fly, <kbd>Esc</kbd> to go back; level select: <kbd>←</kbd>/<kbd>→</kbd>, <kbd>Enter</kbd>.

## Layout

```
src/core/     pure C++: plist/sprite-sheet/BMFont/level parsers, inflate+gzip, cube physics, save data (unit-tested)
src/engine/   SDL2 wrapper: window, renderer, asset index/cache, audio, text, 9-slice
src/game/     scenes (loading, menu, level select, play), UI button, popup
web/shell.html  Emscripten page: asset import, IndexedDB persistence, start button
web/akrile.js   .akrile archive library (WASM core embedded)
tools/        akrile-tool.mjs (pack/unpack/list assets + extract levels), leveldump.cpp
tests/        unit tests (core, sim), sim_bot (plays levels), render_harness + fake_sdl (runs real scenes natively, dumps frames)
```

## CI

`.github/workflows/ci.yml` builds and tests natively, builds the WebAssembly bundle, and on `main`
deploys it to GitHub Pages (enable *Settings → Pages → Source: GitHub Actions*).

## License

GPL-3.0, see `LICENSE`. Geometry Dash is a trademark of RobTop Games; this project is not affiliated with it.

## Testing without a browser

```bash
ctest --test-dir build --output-on-failure                       # core + simulation unit tests
OGD_LEVELS_DIR=Resources ctest --test-dir build                  # + parse every official level, check every id is known
./build/sim_bot Resources/level_0.txt                            # H=250 ./build/sim_bot ... for a longer look-ahead
./build/render_harness Resources "run 2.5; click 722 313; run 1.2; startx 4300; click 640 428; run 0.5; shot x.ppm"
```

`render_harness` links the real game code against a small software fake of SDL (`tests/fake_sdl`, test-only), so
scenes, sprite placement and input can be checked without Emscripten. Script commands: `run <s>`, `click x y`,
`down x y`, `up x y`, `key <space|esc|left|right|enter|backspace|a-z> <down|up>`, `text <chars>`, `startx <units>`, `ship`, `shot <file.ppm>`.
