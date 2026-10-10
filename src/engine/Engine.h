// Thin engine layer over SDL2 (window, 2D renderer, images, audio, input) + asset index.
// Design space: 1280x720, Y axis points UP (same convention as the original cocos2d code).
#pragma once
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/BMFont.h"
#include "core/Save.h"

namespace ogd {

constexpr int kW = 1280;
constexpr int kH = 720;

struct Color {
    Uint8 r = 255, g = 255, b = 255;
};

// A drawable image: either a frame of a sprite sheet or a whole texture.
struct Sprite {
    SDL_Texture* tex = nullptr;
    SDL_Rect src{};            // area in the texture (footprint, i.e. swapped w/h if rotated)
    bool rotated = false;
    float w = 0, h = 0;        // trimmed size in design units
    float srcW = 0, srcH = 0;  // untrimmed size in design units
    float offX = 0, offY = 0;  // trim offset in design units (y-up)
    explicit operator bool() const { return tex != nullptr; }
};

struct Font {
    BMFont bm;
    SDL_Texture* tex = nullptr;
    float scale = 1.f;  // 1 for -hd assets, 2 for SD assets
};

enum class Align { Left, Center, Right };

class Engine {
public:
    bool init(const std::string& resourceRoot, const std::string& savePath);
    void shutdown();

    // ---- assets -------------------------------------------------------
    size_t assetCount() const { return index_.size(); }
    bool hasAsset(const std::string& name) const { return index_.count(name) > 0; }
    std::vector<std::string> sheetNames() const;       // base names ("GJ_GameSheet.plist")
    bool loadSheet(const std::string& plistName);      // false: not a sprite sheet / missing
    Sprite sprite(const std::string& name);            // sheet frame or standalone image
    Font* font(const std::string& fntName);
    Mix_Music* music(const std::string& name);
    void playMusic(const std::string& name, int loops, int volume = MIX_MAX_VOLUME);   // restarts from the beginning; no-op if audio is off
    void stopMusic();
    void setMusicPosition(double seconds);      // seek the running track (practice mode respawn)
    void pauseMusic();
    void resumeMusic();
    void setMusicOn(bool on);   // Options / pause toggle: mutes or unmutes the running track
    void setFxOn(bool on);
    void playSfx(const std::string& name, int volume = MIX_MAX_VOLUME);
    bool readText(const std::string& name, std::string& out) const;

    // ---- drawing (y-up design coordinates, centre-anchored) -------------
    void clear(Color c);
    void drawSprite(const Sprite& s, float cx, float cy, float sx = 1, float sy = 1, float rotDeg = 0,
                    Color col = {}, Uint8 alpha = 255, bool flipX = false, bool flipY = false, bool additive = false);
    // Repeats the sprite horizontally to cover `width` px starting at leftX (the last tile is cropped). Left-anchored.
    void drawSpriteTiledX(const Sprite& s, float leftX, float cy, float width, Color col = {});
    void drawSpriteCropped(const Sprite& s, float leftX, float cy, float fraction, Color col = {}, float scale = 1.f); // left-anchored
    void drawText(Font* f, const std::string& text, float x, float y, float scale = 1, Align a = Align::Center,
                  Color col = {}, Uint8 alpha = 255);
    void fillRect(float x, float y, float w, float h, Color c, Uint8 alpha); // x,y = bottom-left
    void drawPanel(const Sprite& s, float cx, float cy, float w, float h, Color col = {}, Uint8 alpha = 255);
    // Restrict drawing to a rectangle (x,y = bottom-left, y-up). clearClip() removes it.
    void setClip(float x, float y, float w, float h);
    void clearClip();
    void present() { SDL_RenderPresent(ren); }

    void persist(); // write save file (+ flush IndexedDB on web)

    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SaveData save;
    bool audioOk = false;
    // World zoom: 1 for UI; the play scene sets 1.125 so that 320 GD units fill the screen height (the original's view).
    // Positions, sizes and text of drawSprite / fillRect / drawText are multiplied by it.
    void setZoom(float z) { zoom_ = z; }
    float zoom() const { return zoom_; }
    float zoom_ = 1.f;
    int musicBase_ = MIX_MAX_VOLUME;   // volume of the current track while Music is on

private:
    std::string resolve(const std::string& name, bool* isHd = nullptr) const;
    SDL_Texture* loadTexture(const std::string& name, float* scale);

    std::string savePath_;
    std::unordered_map<std::string, std::string> index_;  // basename -> full path
    std::unordered_map<std::string, SDL_Texture*> textures_;
    std::unordered_map<std::string, Sprite> sprites_;
    std::unordered_map<std::string, Font> fonts_;
    std::unordered_map<std::string, Mix_Music*> music_;
    std::unordered_map<std::string, Mix_Chunk*> sfx_;
};

Engine& E();

} // namespace ogd
