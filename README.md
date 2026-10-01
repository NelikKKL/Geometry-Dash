# OpenGD

Open-source C++17 reimplementation of **Geometry Dash 1.0**, built directly on SDL2 (no game engine).
Runs natively (Linux/Windows/macOS) and in the browser via Emscripten/WebAssembly.

> Game assets (sprites, fonts, music) are **not** part of this repository and are not distributed here.
> You need your own copy of the Geometry Dash 1.x APK.

## Status

| Feature | State |
|---|---|
| Loading screen, sprite sheets (.plist), bitmap fonts (.fnt) | done |
| Main menu (buttons, bounce animation, scrolling background, running cube, music) | done |
| Play scene: ground, background, cube jump physics (ported from the original code) | done |
| Garage, creator, options, stats, achievements | placeholder popups |
| Level parser (plain text and base64+gzip/zlib), 7 official levels load with 0 warnings | done |
| Level playback: object-id -> sprite table, collisions, death | TODO (currently a coloured-box debug view of level 0) |
| Particles, motion streak, ship mode, sound effects | TODO |

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

Controls: mouse / touch / <kbd>Space</kbd> / <kbd>↑</kbd> to jump, <kbd>Esc</kbd> to go back.

## Layout

```
src/core/     pure C++: plist/sprite-sheet/BMFont/level parsers, inflate+gzip, cube physics, save data (unit-tested)
src/engine/   SDL2 wrapper: window, renderer, asset index/cache, audio, text, 9-slice
src/game/     scenes (loading, menu, play), UI button, popup
web/shell.html  Emscripten page: asset import, IndexedDB persistence, start button
web/akrile.js   .akrile archive library (WASM core embedded)
tools/        akrile-tool.mjs (pack/unpack/list assets + extract levels), leveldump.cpp
tests/        dependency-free unit tests
```

## CI

`.github/workflows/ci.yml` builds and tests natively, builds the WebAssembly bundle, and on `main`
deploys it to GitHub Pages (enable *Settings → Pages → Source: GitHub Actions*).

## License

GPL-3.0, see `LICENSE`. Geometry Dash is a trademark of RobTop Games; this project is not affiliated with it.
