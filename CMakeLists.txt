cmake_minimum_required(VERSION 3.16)
project(OpenGD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(OGD_BUILD_TESTS "Build unit tests" ON)

# --- core: pure C++, no SDL (parsers, physics, save data) -------------------
add_library(ogd_core STATIC
  src/core/Plist.cpp src/core/Atlas.cpp src/core/BMFont.cpp src/core/Save.cpp
  src/core/Inflate.cpp src/core/Level.cpp src/core/ObjectTable.cpp src/core/Sim.cpp src/core/Unlocks.cpp)
target_include_directories(ogd_core PUBLIC src libs)

# --- game -------------------------------------------------------------------
add_executable(OpenGD
  src/main.cpp
  src/engine/Engine.cpp
  src/game/App.cpp src/game/Ui.cpp src/game/Popup.cpp src/game/World.cpp src/game/Scenes.cpp
  src/game/LevelSelect.cpp src/game/PlayScene.cpp src/game/PauseLayer.cpp src/game/Garage.cpp src/game/Boards.cpp)
target_link_libraries(OpenGD PRIVATE ogd_core)

if(EMSCRIPTEN)
  # SDL2 / SDL2_image / SDL2_mixer come from Emscripten ports (downloaded on first use).
  set(OGD_PORTS -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS=png -sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS=ogg,mp3)
  target_compile_options(OpenGD PRIVATE ${OGD_PORTS})
  target_link_options(OpenGD PRIVATE ${OGD_PORTS}
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=268435456
    -sINVOKE_RUN=0
    -sFORCE_FILESYSTEM=1
    -sEXPORTED_RUNTIME_METHODS=callMain,FS
    -lidbfs.js
    --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
  set_target_properties(OpenGD PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  # the page unpacks .akrile asset bundles with this library
  add_custom_command(TARGET OpenGD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/web/akrile.js $<TARGET_FILE_DIR:OpenGD>/akrile.js)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_image SDL2_mixer)
  target_link_libraries(OpenGD PRIVATE PkgConfig::SDL2)
  # SDL headers are included as <SDL.h>; pkg-config already adds .../SDL2 to the include path.
endif()

# --- tools (native only) -----------------------------------------------------
if(NOT EMSCRIPTEN)
  add_executable(leveldump tools/leveldump.cpp)
  target_link_libraries(leveldump PRIVATE ogd_core)
endif()

# --- tests (native only) -----------------------------------------------------
if(OGD_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_executable(core_tests tests/core_tests.cpp)
  target_link_libraries(core_tests PRIVATE ogd_core)
  target_compile_definitions(core_tests PRIVATE OGD_TEST_DATA="${CMAKE_SOURCE_DIR}/tests/data")
  add_test(NAME core_tests COMMAND core_tests)

  add_executable(sim_tests tests/sim_tests.cpp)
  target_link_libraries(sim_tests PRIVATE ogd_core)
  add_test(NAME sim_tests COMMAND sim_tests)

  # runs the real scenes on a software fake of SDL and dumps frames (manual, needs assets):
  #   render_harness Resources "run 2.5; click 722 313; run 1; shot select.ppm"
  add_executable(render_harness
    tests/render_harness.cpp tests/fake_sdl/fake_sdl.cpp src/engine/Engine.cpp
    src/game/App.cpp src/game/Ui.cpp src/game/Popup.cpp src/game/World.cpp src/game/Scenes.cpp
    src/game/LevelSelect.cpp src/game/PlayScene.cpp src/game/PauseLayer.cpp src/game/Garage.cpp src/game/Boards.cpp)
  target_include_directories(render_harness PRIVATE tests/fake_sdl)   # shadows the real SDL headers
  target_link_libraries(render_harness PRIVATE ogd_core)

  # look-ahead bot that tries to complete level files (manual: sim_bot Resources/level_0.txt)
  add_executable(sim_bot tests/sim_bot.cpp)
  target_link_libraries(sim_bot PRIVATE ogd_core)

  # exact reachability search ("can this level be finished?"): sim_solver Resources/level_4.txt -20 0 8000
  add_executable(sim_solver tests/sim_solver.cpp)
  target_link_libraries(sim_solver PRIVATE ogd_core)
endif()
