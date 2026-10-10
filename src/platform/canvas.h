// canvas.h - the game's screen: persistent 240x320 surface, soft-key rendering, key/pointer routing.
// Port of jme.Canvas + GameCanvas. The window/input backend (SDL or headless) drives it through
// dispatchKey*/dispatchPointer* and reads presented pixels from shown().
#pragma once
#include "jme.h"

namespace cbx {

class GameCanvas : public ICanvas {
public:
    static constexpr int LOGICAL_W = 240, LOGICAL_H = 320;
    static constexpr int UP = 1, LEFT = 2, RIGHT = 5, DOWN = 6, FIRE = 8, GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    static GameCanvas* current;

    GameCanvas();

    // ICanvas
    void setMidlet(GameMIDlet* m) override { midlet_ = m; }
    void removeCommand(Command command) override;
    void setCommand(Command command, Image image) override;
    void setActive(bool active) override;
    int getGameAction(int k) override;
    int getHeight() override { return LOGICAL_H; }
    int getWidth() override { return LOGICAL_W; }
    String getKeyName(int k) override;

    // frame handling (Canvas.repaint / serviceRepaints)
    void repaint() { repaintRequested_ = true; }
    void serviceRepaints();
    const std::vector<uint32_t>& shown() const { return shown_->px; }
    uint64_t frameCounter() const { return frames_; }

    // input from the backend (J2ME key codes: -1..-4 arrows, -5 select, -6/-7 soft keys, 48..57 digits ...)
    void keyDown(int code, bool repeat);
    void keyUp(int code);
    void pointerDown(int x, int y);

private:
    void paintSoftKeys(Graphics g);
    void keyPressed(int keyCode);
    void keyReleased(int keyCode);
    void keyRepeated(int keyCode);
    void showNotify();
    void hideNotify();

    GameMIDlet* midlet_ = nullptr;
    bool justActivated_ = false, suppressPause_ = false, repaintRequested_ = true;
    Command leftCommand_, rightCommand_;
    Image leftIcon_, rightIcon_;
    Font softKeyFont_;
    std::shared_ptr<Surface> screen_, shown_;
    uint64_t frames_ = 0;
};

} // namespace cbx
