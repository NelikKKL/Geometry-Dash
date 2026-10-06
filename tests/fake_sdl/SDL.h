// Minimal software "SDL2" used ONLY by tests/render_harness to run the real game code natively and dump frames.
// Implements exactly the subset of SDL2 / SDL2_image / SDL2_mixer that src/ uses. Not part of the product.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

typedef uint8_t Uint8; typedef uint32_t Uint32; typedef uint64_t Uint64; typedef int32_t Sint32;
struct SDL_Window;
struct SDL_Texture { int w = 0, h = 0; std::vector<uint8_t> rgba; uint8_t cr = 255, cg = 255, cb = 255, ca = 255; };
struct SDL_Renderer { int w = 1280, h = 720; std::vector<uint8_t> fb; uint8_t r = 0, g = 0, b = 0, a = 255; bool clip = false; int cx = 0, cy = 0, cw = 0, ch = 0; };
struct SDL_Rect { int x, y, w, h; };
struct SDL_FRect { float x, y, w, h; };
typedef enum { SDL_FLIP_NONE = 0, SDL_FLIP_HORIZONTAL = 1, SDL_FLIP_VERTICAL = 2 } SDL_RendererFlip;
typedef int32_t SDL_Keycode;
enum { SDLK_SPACE = 32, SDLK_UP = 1, SDLK_w = 'w', SDLK_ESCAPE = 27, SDLK_LEFT = 2, SDLK_RIGHT = 3, SDLK_a = 'a', SDLK_d = 'd', SDLK_RETURN = 13, SDLK_BACKSPACE = 8 };
struct SDL_Keysym { SDL_Keycode sym; };
struct SDL_KeyboardEvent { Uint32 type; Uint8 repeat; SDL_Keysym keysym; };
struct SDL_MouseButtonEvent { Uint32 type; Uint8 button; Sint32 x, y; };
struct SDL_TextInputEvent { Uint32 type; char text[32]; };
struct SDL_MouseMotionEvent { Uint32 type; Uint32 state; Sint32 x, y; };
struct SDL_MouseWheelEvent { Uint32 type; Sint32 x, y; };
union SDL_Event { Uint32 type; SDL_KeyboardEvent key; SDL_MouseButtonEvent button; SDL_TextInputEvent text; SDL_MouseMotionEvent motion; SDL_MouseWheelEvent wheel; };
enum { SDL_QUIT = 0x100, SDL_KEYDOWN = 0x300, SDL_KEYUP, SDL_TEXTINPUT = 0x303, SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN = 0x401, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL = 0x403 };
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_LMASK 1
#define SDL_INIT_VIDEO 1
#define SDL_INIT_AUDIO 2
#define SDL_INIT_EVENTS 4
#define SDL_HINT_RENDER_SCALE_QUALITY "x"
#define SDL_WINDOWPOS_CENTERED 0
#define SDL_WINDOW_SHOWN 1
#define SDL_WINDOW_RESIZABLE 2
#define SDL_RENDERER_ACCELERATED 1
#define SDL_RENDERER_SOFTWARE 2
#define SDL_RENDERER_PRESENTVSYNC 4
enum { SDL_BLENDMODE_BLEND = 1 };

int SDL_Init(Uint32); void SDL_Quit(); void SDL_Log(const char*, ...); const char* SDL_GetError();
void SDL_SetHint(const char*, const char*);
SDL_Window* SDL_CreateWindow(const char*, int, int, int, int, Uint32);
SDL_Renderer* SDL_CreateRenderer(SDL_Window*, int, Uint32);
void SDL_DestroyRenderer(SDL_Renderer*); void SDL_DestroyWindow(SDL_Window*); void SDL_DestroyTexture(SDL_Texture*);
int SDL_RenderSetLogicalSize(SDL_Renderer*, int, int); int SDL_SetRenderDrawBlendMode(SDL_Renderer*, int);
int SDL_SetRenderDrawColor(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8); int SDL_RenderClear(SDL_Renderer*);
int SDL_RenderFillRectF(SDL_Renderer*, const SDL_FRect*); void SDL_RenderPresent(SDL_Renderer*);
int SDL_SetTextureBlendMode(SDL_Texture*, int); int SDL_SetTextureColorMod(SDL_Texture*, Uint8, Uint8, Uint8);
int SDL_SetTextureAlphaMod(SDL_Texture*, Uint8); int SDL_QueryTexture(SDL_Texture*, Uint32*, int*, int*, int*);
int SDL_RenderCopyExF(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_FRect*, double, const void*, SDL_RendererFlip);
int SDL_RenderCopyF(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_FRect*);
void SDL_RenderWindowToLogical(SDL_Renderer*, int, int, float*, float*);
int SDL_PollEvent(SDL_Event*); Uint64 SDL_GetPerformanceCounter(); Uint64 SDL_GetPerformanceFrequency();
int SDL_RenderSetClipRect(SDL_Renderer*, const SDL_Rect*);
Uint32 SDL_GetTicks(); int SDL_OpenURL(const char*);
void SDL_StartTextInput(); void SDL_StopTextInput();

// --- test control surface
void fake_push_event(const SDL_Event& e);
void fake_advance(double seconds);
bool fake_dump_ppm(SDL_Renderer* r, const char* path);
SDL_Renderer* fake_renderer();
