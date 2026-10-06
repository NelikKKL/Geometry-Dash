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
| Gameplay: cube, ship, gravity portals, pads, rings, blocks/slabs, spikes, colour triggers (physics ported from the OpenGD fork) | done (see "Physics source and accuracy") |
| Death burst + restart, level complete screen, progress bar, music, sound effects | done |
| Stats board (jumps, attempts, completed levels) and More Games board (scrollable banners from the APK) | done |
| Creator, options, achievements, practice mode | placeholder popups / TODO |
| Invisible fade/trail triggers (ids 22-28, 32, 33) | ignored |

## More Games links

The banners (`promo_boom`, `promo_mu`, `promo_mm`) come from the APK. The original fetched the list and the store links from
`robtopgames.com/checkMoreGamesAndroid.php` / `robtopgames.com/download/<key><platform>`; none of Boomlings, Boomlings
MatchUp or Memory Mastermind is listed on the official site any more, so no verified store link exists. Every banner
therefore opens `https://robtopgames.com`. To point a banner somewhere else edit the `kGames` table at the top of the
More Games section in `src/game/Boards.cpp`. (The official Geometry Dash store links are listed on that site.)

## Physics source and accuracy

The simulation (`src/core/Sim.cpp`, `src/core/ObjectTable.cpp`) is a port of the OpenGD fork's `PlayerObject`,
`PlayLayer::checkCollisions` and its object data (`object.json` for types / sprites / z-order, `LongData.cpp` for hitboxes):

* **Player boxes:** a 30x30 *outer* box (landing, hazards, portals, pads, rings) and a 7.5x7.5 *inner* box (touching a
  block with it is death). So the cube may sink ~11 units into a wall before dying, which is what makes one-block gaps
  passable (the Back On Track window gives ~200 ms of timing instead of ~30 ms).
* **Landing:** a falling cube snaps onto a block whose top is at most 5 units above its feet (ship: 9); shortly after
  leaving a platform the cube still counts as grounded (~2 frames, "coyote time").
* **Ship:** acceleration factors 0.4 (0.5 when holding while falling) x {1.0 hold, 1.2 release while rising, 0.8 release
  while falling}, vertical speed limits +8 / -6.4. `shipScale` in `SimConfig` scales the acceleration if it feels off.
* **Rings:** a tap is *queued* until used; the ring fires if the player touches it while the tap is queued and the button
  is held (so tapping a moment early works, holding from the ground does not). Yellow pad = 16, ring = jump speed.
* **Objects:** solid 1,2,3,4,6,7,40; hazards 8, 39 and **9** (ground thorns); decoration (no collision) 5, 15-21, 41;
  portals 10-13; pad 35; ring 36; colour triggers 29/30; 22-28, 32, 33 ignored.
* **Verified:** all seven official levels are completable in the simulation (`sim_solver`, an exact reachability search).
* **Still approximate:** ship floor/ceiling bounds (3 and 297 units, the original derives them from the portal height),
  rotated hitboxes use their bounding box, only yellow pads/rings and normal speed exist (no mini, dual, speed portals).

* **Unlocks** are this project's own design (the original uses achievements, which do not exist here): icons 1-4 and
  colours 1-4 are free; completing level N unlocks icon N+4 and colour N+4; icon 12 needs 4 completed levels,
  icon 13 and colour 12 need all 7. The rules are one small table in `src/core/Unlocks.cpp`. Hidden debug key in the
  garage: <kbd>U</kbd> toggles "unlock everything" (not saved).
* Not implemented: practice mode, checkpoints, the level-end wall, dual mode, other cube/ship icons.

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
./build/sim_solver Resources/level_4.txt -20 0 8000              # exact: can the level be finished? (~30-90 s per level)
./build/sim_bot Resources/level_0.txt                            # greedy look-ahead bot (H=250 for a longer horizon)
./build/render_harness Resources "run 2.5; click 722 313; run 1.2; startx 4300; click 640 428; run 0.5; shot x.ppm"
```

`render_harness` links the real game code against a small software fake of SDL (`tests/fake_sdl`, test-only), so
scenes, sprite placement and input can be checked without Emscripten. Script commands: `run <s>`, `click x y`,
`down x y`, `up x y`, `key <space|esc|left|right|enter|backspace|a-z> <down|up>`, `text <chars>`, `startx <units>`, `ship`, `shot <file.ppm>`.
