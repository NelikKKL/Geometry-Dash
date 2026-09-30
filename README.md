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
| Level parsing / objects / collisions / death | TODO |
| Particles, motion streak, ship mode, sound effects | TODO |

## Assets

The game looks for files by **basename** anywhere under the resource directory, so both the flat APK layout
and the old `reallocate.sh` layout work. `-hd` variants are preferred.

* **Web:** open the page, press *Choose Geometry Dash APK / ZIP*. The archive is unpacked in the browser and
  cached in IndexedDB; nothing leaves your machine.
* **Native:** copy the contents of the APK's `assets/` folder into `./Resources/`.

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

Controls: mouse / touch / <kbd>Space</kbd> / <kbd>↑</kbd> to jump, <kbd>Esc</kbd> to go back.

## Layout

```
src/core/     pure C++: plist + sprite-sheet + BMFont parsers, cube physics, save data (unit-tested)
src/engine/   SDL2 wrapper: window, renderer, asset index/cache, audio, text, 9-slice
src/game/     scenes (loading, menu, play), UI button, popup
web/shell.html  Emscripten page: asset import, IndexedDB persistence, start button
tests/        dependency-free unit tests
```

## CI

`.github/workflows/ci.yml` builds and tests natively, builds the WebAssembly bundle, and on `main`
deploys it to GitHub Pages (enable *Settings → Pages → Source: GitHub Actions*).

## License

GPL-3.0, see `LICENSE`. Geometry Dash is a trademark of RobTop Games; this project is not affiliated with it.
