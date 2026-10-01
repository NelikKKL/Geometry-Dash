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
  src/core/Inflate.cpp src/core/Level.cpp)
target_include_directories(ogd_core PUBLIC src libs)

# --- game -------------------------------------------------------------------
add_executable(OpenGD
  src/main.cpp
  src/engine/Engine.cpp
  src/game/App.cpp src/game/Ui.cpp src/game/Popup.cpp src/game/World.cpp src/game/Scenes.cpp)
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
endif()
