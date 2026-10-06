#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <string>

#include "core/Inflate.h"

static std::deque<SDL_Event> g_events;
static double g_time = 0;
static SDL_Renderer* g_ren = nullptr;
static std::string g_err;

int SDL_Init(Uint32) { return 0; }
void SDL_Quit() {}
void SDL_Log(const char* f, ...) { va_list a; va_start(a, f); std::vfprintf(stderr, f, a); std::fputc('\n', stderr); va_end(a); }
const char* SDL_GetError() { return g_err.c_str(); }
void SDL_SetHint(const char*, const char*) {}
SDL_Window* SDL_CreateWindow(const char*, int, int, int, int, Uint32) { return (SDL_Window*)1; }
SDL_Renderer* SDL_CreateRenderer(SDL_Window*, int, Uint32) { g_ren = new SDL_Renderer(); g_ren->fb.assign(1280 * 720 * 4, 255); return g_ren; }
void SDL_DestroyRenderer(SDL_Renderer* r) { delete r; if (r == g_ren) g_ren = nullptr; }
void SDL_DestroyWindow(SDL_Window*) {}
void SDL_DestroyTexture(SDL_Texture* t) { delete t; }
int SDL_RenderSetLogicalSize(SDL_Renderer*, int, int) { return 0; }
int SDL_SetRenderDrawBlendMode(SDL_Renderer*, int) { return 0; }
int SDL_SetRenderDrawColor(SDL_Renderer* r, Uint8 R, Uint8 G, Uint8 B, Uint8 A) { r->r = R; r->g = G; r->b = B; r->a = A; return 0; }
int SDL_RenderClear(SDL_Renderer* r) { for (size_t i = 0; i < r->fb.size(); i += 4) { r->fb[i] = r->r; r->fb[i + 1] = r->g; r->fb[i + 2] = r->b; r->fb[i + 3] = 255; } return 0; }
void SDL_RenderPresent(SDL_Renderer*) {}
int SDL_RenderSetClipRect(SDL_Renderer* r, const SDL_Rect* c) {
    r->clip = c != nullptr;
    if (c) { r->cx = c->x; r->cy = c->y; r->cw = c->w; r->ch = c->h; }
    return 0;
}
int SDL_SetTextureBlendMode(SDL_Texture*, int) { return 0; }
int SDL_SetTextureColorMod(SDL_Texture* t, Uint8 r, Uint8 g, Uint8 b) { t->cr = r; t->cg = g; t->cb = b; return 0; }
int SDL_SetTextureAlphaMod(SDL_Texture* t, Uint8 a) { t->ca = a; return 0; }
int SDL_QueryTexture(SDL_Texture* t, Uint32*, int*, int* w, int* h) { *w = t->w; *h = t->h; return 0; }
void SDL_RenderWindowToLogical(SDL_Renderer*, int x, int y, float* lx, float* ly) { *lx = (float)x; *ly = (float)y; }
int SDL_PollEvent(SDL_Event* e) { if (g_events.empty()) return 0; *e = g_events.front(); g_events.pop_front(); return 1; }
Uint64 SDL_GetPerformanceCounter() { return (Uint64)(g_time * 1e9); }
Uint64 SDL_GetPerformanceFrequency() { return 1000000000ull; }
Uint32 SDL_GetTicks() { return (Uint32)(g_time * 1000.0); }
int SDL_OpenURL(const char*) { return 0; }
void SDL_StartTextInput() {}
void SDL_StopTextInput() {}

void fake_push_event(const SDL_Event& e) { g_events.push_back(e); }
void fake_advance(double s) { g_time += s; }
SDL_Renderer* fake_renderer() { return g_ren; }
bool fake_dump_ppm(SDL_Renderer* r, const char* path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << r->w << " " << r->h << "\n255\n";
    for (int i = 0; i < r->w * r->h; ++i) f.write((const char*)&r->fb[i * 4], 3);
    return true;
}

// ------------------------------------------------------------------ drawing
static inline void blendPixel(SDL_Renderer* r, int x, int y, float R, float G, float B, float A) {
    if (x < 0 || y < 0 || x >= r->w || y >= r->h || A <= 0) return;
    if (r->clip && (x < r->cx || y < r->cy || x >= r->cx + r->cw || y >= r->cy + r->ch)) return;
    uint8_t* p = &r->fb[((size_t)y * r->w + x) * 4];
    p[0] = (uint8_t)(R * A + p[0] * (1 - A) + 0.5f);
    p[1] = (uint8_t)(G * A + p[1] * (1 - A) + 0.5f);
    p[2] = (uint8_t)(B * A + p[2] * (1 - A) + 0.5f);
}

int SDL_RenderFillRectF(SDL_Renderer* r, const SDL_FRect* rc) {
    const int x0 = (int)std::floor(rc->x + 0.5f), x1 = (int)std::floor(rc->x + rc->w + 0.5f);
    const int y0 = (int)std::floor(rc->y + 0.5f), y1 = (int)std::floor(rc->y + rc->h + 0.5f);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) blendPixel(r, x, y, r->r, r->g, r->b, r->a / 255.f);
    return 0;
}

static void sample(const SDL_Texture* t, const SDL_Rect& s, float u, float v, float out[4]) {
    // bilinear inside the source rectangle (never bleeds into neighbouring atlas cells)
    float fx = s.x + u * s.w - 0.5f, fy = s.y + v * s.h - 0.5f;
    fx = std::max((float)s.x, std::min((float)(s.x + s.w - 1), fx));
    fy = std::max((float)s.y, std::min((float)(s.y + s.h - 1), fy));
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = std::min(x0 + 1, s.x + s.w - 1), y1 = std::min(y0 + 1, s.y + s.h - 1);
    float ax = fx - x0, ay = fy - y0;
    auto px = [&](int x, int y, int c) { return (float)t->rgba[((size_t)y * t->w + x) * 4 + c]; };
    for (int c = 0; c < 4; ++c) {
        float top = px(x0, y0, c) * (1 - ax) + px(x1, y0, c) * ax;
        float bot = px(x0, y1, c) * (1 - ax) + px(x1, y1, c) * ax;
        out[c] = top * (1 - ay) + bot * ay;
    }
}

int SDL_RenderCopyExF(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_FRect* dst, double angle, const void*, SDL_RendererFlip flip) {
    SDL_Rect s = src ? *src : SDL_Rect{0, 0, t->w, t->h};
    if (s.w <= 0 || s.h <= 0 || dst->w <= 0 || dst->h <= 0) return 0;
    const float cx = dst->x + dst->w / 2, cy = dst->y + dst->h / 2;
    const float rad = (float)(angle * 3.14159265358979 / 180.0);
    const float ca = std::cos(rad), sa = std::sin(rad);
    // bounding box of the rotated rect
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < 4; ++i) {
        float lx = (i & 1 ? 0.5f : -0.5f) * dst->w, ly = (i & 2 ? 0.5f : -0.5f) * dst->h;
        float X = cx + lx * ca - ly * sa, Y = cy + lx * sa + ly * ca;
        minx = std::min(minx, X); maxx = std::max(maxx, X); miny = std::min(miny, Y); maxy = std::max(maxy, Y);
    }
    const int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(r->w - 1, (int)std::ceil(maxx));
    const int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(r->h - 1, (int)std::ceil(maxy));
    const float mr = t->cr / 255.f, mg = t->cg / 255.f, mb = t->cb / 255.f, ma = t->ca / 255.f;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float px = x + 0.5f - cx, py = y + 0.5f - cy;
            const float lx = px * ca + py * sa, ly = -px * sa + py * ca;      // inverse rotation (clockwise screen angle)
            float u = lx / dst->w + 0.5f, v = ly / dst->h + 0.5f;
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            if (flip & SDL_FLIP_HORIZONTAL) u = 1 - u;
            if (flip & SDL_FLIP_VERTICAL) v = 1 - v;
            float c[4];
            sample(t, s, u, v, c);
            blendPixel(r, x, y, c[0] * mr, c[1] * mg, c[2] * mb, c[3] / 255.f * ma);
        }
    return 0;
}

int SDL_RenderCopyF(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_FRect* dst) {
    return SDL_RenderCopyExF(r, t, src, dst, 0, nullptr, SDL_FLIP_NONE);
}

// ---------------------------------------------------------------- PNG loading (8-bit RGB/RGBA, non-interlaced)
int IMG_Init(int) { return 0; }
void IMG_Quit() {}
const char* IMG_GetError() { return g_err.c_str(); }

SDL_Texture* IMG_LoadTexture(SDL_Renderer*, const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    auto be32 = [&](size_t p) { return (uint32_t)d[p] << 24 | d[p + 1] << 16 | d[p + 2] << 8 | d[p + 3]; };
    if (d.size() < 33 || std::memcmp(d.data(), "\x89PNG", 4) != 0) { g_err = "not a png"; return nullptr; }
    uint32_t w = 0, h = 0; int bd = 0, ct = 0; std::vector<uint8_t> idat;
    for (size_t p = 8; p + 12 <= d.size();) {
        uint32_t len = be32(p); std::string type((const char*)&d[p + 4], 4);
        if (type == "IHDR") { w = be32(p + 8); h = be32(p + 12); bd = d[p + 16]; ct = d[p + 17]; if (d[p + 20]) { g_err = "interlaced"; return nullptr; } }
        else if (type == "IDAT") idat.insert(idat.end(), d.begin() + p + 8, d.begin() + p + 8 + len);
        p += 12 + len;
    }
    const int bpp = ct == 6 ? 4 : ct == 2 ? 3 : 0;
    if (bd != 8 || !bpp) { g_err = "unsupported png format"; return nullptr; }
    std::vector<uint8_t> raw;
    if (!ogd::zlibDecompress(idat.data(), idat.size(), raw, 512u << 20)) { g_err = "bad zlib"; return nullptr; }
    SDL_Texture* t = new SDL_Texture(); t->w = (int)w; t->h = (int)h; t->rgba.assign((size_t)w * h * 4, 255);
    const size_t stride = (size_t)w * bpp;
    std::vector<uint8_t> prev(stride, 0), cur(stride);
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* line = &raw[y * (stride + 1)]; int ft = line[0];
        for (size_t i = 0; i < stride; ++i) {
            int a = i >= (size_t)bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= (size_t)bpp ? prev[i - bpp] : 0, x = line[1 + i], v;
            switch (ft) {
            case 0: v = x; break; case 1: v = x + a; break; case 2: v = x + b; break; case 3: v = x + ((a + b) >> 1); break;
            default: { int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c); v = x + ((pa <= pb && pa <= pc) ? a : pb <= pc ? b : c); }
            }
            cur[i] = (uint8_t)v;
        }
        for (uint32_t x = 0; x < w; ++x) { uint8_t* o = &t->rgba[((size_t)y * w + x) * 4]; for (int c = 0; c < bpp; ++c) o[c] = cur[x * bpp + c]; }
        prev = cur;
    }
    return t;
}

// ---------------------------------------------------------------- audio: disabled
int Mix_OpenAudio(int, Uint32, int, int) { g_err = "fake audio"; return -1; }
void Mix_CloseAudio() {} int Mix_AllocateChannels(int) { return 0; }
Mix_Music* Mix_LoadMUS(const char*) { return nullptr; } void Mix_FreeMusic(Mix_Music*) {} const char* Mix_GetError() { return g_err.c_str(); }
int Mix_PlayMusic(Mix_Music*, int) { return 0; } int Mix_VolumeMusic(int) { return 0; } int Mix_HaltMusic() { return 0; } int Mix_PlayingMusic() { return 0; }
Mix_Chunk* Mix_LoadWAV(const char*) { return nullptr; } void Mix_FreeChunk(Mix_Chunk*) {} int Mix_VolumeChunk(Mix_Chunk*, int) { return 0; } int Mix_PlayChannel(int, Mix_Chunk*, int) { return 0; }
