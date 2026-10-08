// "Hanging board" screens of the main menu (Stats, More Games): a green frame on two chains that drops in from the top.
// The board is an overlay owned by the menu scene; the menu keeps animating behind it.
#pragma once
#include <memory>
#include <string>

#include "game/Ui.h"

namespace ogd {

class Board {
public:
    explicit Board(const std::string& title);
    virtual ~Board() = default;

    void update(float dt);
    void draw();
    void onDown(float x, float y);
    void onUp(float x, float y);
    void onMove(float x, float y);
    void onWheel(float dy);
    void onKey(SDL_Keycode k, bool down);

    bool closed = false;
    std::unique_ptr<Board> next;       // set together with `closed`: the menu opens this board in its place (Options -> Support ...)

    // The green frame (chains, sides, top and bottom bars). Shared with the level-complete screen.
    static void drawFrame(float topBarY, float bottomBarY, float inTop, float inBottom, float dy, bool chains = true);

    // interior (the area below the title bar), design px, y-up
    static constexpr float kInL = 262.f, kInR = 1018.f, kInTop = 595.f, kInBottom = 112.f;

protected:
    virtual void updateInterior(float dt) { (void)dt; }
    virtual void drawInterior() = 0;                       // clip rect is set; all y are already shifted by the slide
    virtual void interiorDown(float x, float y) { (void)x; (void)y; }
    virtual void interiorUp(float x, float y) { (void)x; (void)y; }
    virtual void interiorMove(float x, float y) { (void)x; (void)y; }
    virtual void interiorWheel(float dy) { (void)dy; }
    // A modal layer on top of the whole board (tutorial, song card). While it is active it gets all input.
    virtual bool overlayActive() const { return false; }
    virtual void updateOverlay(float dt) { (void)dt; }
    virtual void drawOverlay() {}
    virtual void overlayDown(float x, float y) { (void)x; (void)y; }
    virtual void overlayUp(float x, float y) { (void)x; (void)y; }
    virtual void overlayBack() {}

    float slide() const { return slideY_; }                // add to a y to place it on the (moving) board
    static bool inInterior(float x, float y, float slideY);
    bool settled() const { return t_ >= 1.f; }

private:
    std::string title_;
    Button back_;
    float t_ = 0.f, slideY_ = 760.f;
};

std::unique_ptr<Board> makeStatsBoard();
std::unique_ptr<Board> makeMoreGamesBoard();
std::unique_ptr<Board> makeSongsBoard();     // SongsLayer (also opened from the level-select screen)
std::unique_ptr<Board> makeOptionsBoard();   // OptionsLayer; leads to the Support / Soundtracks boards and the tutorial

} // namespace ogd
