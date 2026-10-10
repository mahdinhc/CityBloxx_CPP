#include "canvas.h"
#include "../game/classes.h"

namespace cbx {

GameCanvas* GameCanvas::current = nullptr;

GameCanvas::GameCanvas() {
    screen_ = std::make_shared<Surface>(LOGICAL_W, LOGICAL_H);
    shown_ = std::make_shared<Surface>(LOGICAL_W, LOGICAL_H);
    softKeyFont_ = Font::getFont(32, 1, 8);
    current = this;
}

int GameCanvas::getGameAction(int k) {
    switch (k) {
        case -1: case 50: return UP;
        case -2: case 56: return DOWN;
        case -3: case 52: return LEFT;
        case -4: case 54: return RIGHT;
        case -5: case 53: return FIRE;
        case 49: return GAME_A;
        case 51: return GAME_B;
        case 55: return GAME_C;
        case 57: return GAME_D;
        default: return 0;
    }
}

String GameCanvas::getKeyName(int k) {
    if (k >= 48 && k <= 57) return String::valueOf((char16_t)k);
    if (k == 42) return u"*"_s;
    if (k == 35) return u"#"_s;
    switch (k) {
        case -1: return u"Up"_s;
        case -2: return u"Down"_s;
        case -3: return u"Left"_s;
        case -4: return u"Right"_s;
        case -5: return u"Select"_s;
        case -6: return u"Left soft key"_s;
        case -7: return u"Right soft key"_s;
        default: return u"Key "_s + k;
    }
}

static bool isLeftType(int t) { return t == 1 || t == 4 || t == 5 || t == 8; }
static bool isRightType(int t) { return t == 2 || t == 3 || t == 6 || t == 7; }

void GameCanvas::removeCommand(Command c) {
    int t = c.getCommandType();
    if (isLeftType(t)) { leftCommand_ = Command(); leftIcon_ = Image(); }
    else if (isRightType(t)) { rightCommand_ = Command(); rightIcon_ = Image(); }
}
void GameCanvas::setCommand(Command c, Image image) {
    int t = c.getCommandType();
    if (isLeftType(t)) { leftCommand_ = c; leftIcon_ = image; }
    else if (isRightType(t)) { rightCommand_ = c; rightIcon_ = image; }
}

void GameCanvas::setActive(bool active) {
    if (active) {
        suppressPause_ = false;
        justActivated_ = true;
        current = this;
        showNotify();          // Display.setCurrent(canvas) -> focus() -> showNotify()
        return;
    }
    suppressPause_ = true;
    hideNotify();              // Display.setCurrent(form) -> unfocus() -> hideNotify()
}

void GameCanvas::hideNotify() {
    try { if (!suppressPause_ && midlet_) midlet_->pauseApp(); } catch (const JavaException&) {}
}
void GameCanvas::showNotify() {
    try {
        if (justActivated_) justActivated_ = false;
        else if (midlet_) midlet_->resume();
    } catch (const JavaException&) {}
}

void GameCanvas::keyPressed(int keyCode) {
    if (!midlet_) return;
    if (keyCode == -6) {
        try { if (leftCommand_ != null) midlet_->handleCommand(leftCommand_); } catch (const JavaException&) {}
    } else if (keyCode == -7) {
        if (rightCommand_ != null) midlet_->handleCommand(rightCommand_);
    } else if (keyCode != -11 && keyCode != -12) {
        midlet_->keyPressedRaw(keyCode);
    }
}
void GameCanvas::keyReleased(int keyCode) {
    if (midlet_ && keyCode != -6 && keyCode != -7) {
        try { midlet_->keyReleasedRaw(keyCode); } catch (const JavaException&) {}
    }
}
void GameCanvas::keyRepeated(int keyCode) {
    if (midlet_ && keyCode != -6 && keyCode != -7) {
        try { midlet_->keyRepeatedRaw(keyCode); } catch (const JavaException&) {}
    }
}

void GameCanvas::keyDown(int code, bool repeat) {
    if (code == 0) return;
    try {
        if (repeat) keyRepeated(code);
        else keyPressed(code);
    } catch (const JavaException& e) { std::fprintf(stderr, "exception in keyPressed: %s\n", e.what()); }
}
void GameCanvas::keyUp(int code) {
    if (code == 0) return;
    keyReleased(code);
}

void GameCanvas::pointerDown(int pointerX, int pointerY) {
    try {
        if (!midlet_) return;
        if (pointerY >= GameMIDlet::screenHeight - 26) {
            if (pointerX < GameMIDlet::screenWidth / 3) { keyPressed(-6); keyReleased(-6); return; }
            if (pointerX > GameMIDlet::screenWidth * 2 / 3) { keyPressed(-7); keyReleased(-7); return; }
            keyPressed(-5); keyReleased(-5); return;
        }
        Screen* cur = midlet_->getCurrentScreen();
        if (MenuController* mc = dynamic_cast<MenuController*>(cur)) {
            int item = Ui::getItemAt(pointerX, pointerY);
            if (item >= 0) { mc->selectItem(item); return; }
            keyPressed(-5); keyReleased(-5); return;
        } else if (dynamic_cast<House*>(cur) != nullptr) {
            if (House::getMode() == 2) {
                if (CityMode::handleClick(pointerX, pointerY)) return;
            } else if (House::getMode() != 7) {
                House::dropBlock();
                return;
            }
        }
        keyPressed(-5); keyReleased(-5);
    } catch (const JavaException& e) { std::fprintf(stderr, "exception in pointerPressed: %s\n", e.what()); }
}

void GameCanvas::paintSoftKeys(Graphics g) {
    int baselineY = GameMIDlet::screenHeight - 2;
    g.setClip(0, GameMIDlet::screenHeight - ((softKeyFont_.getHeight() + 2) + 4), GameMIDlet::screenWidth, (softKeyFont_.getHeight() + 2) + 4);
    g.setFont(softKeyFont_);
    if (leftCommand_ != null) {
        if (leftIcon_ != null) {
            g.drawImage(leftIcon_, 2, baselineY, 36);
        } else {
            String label = leftCommand_.getLabel();
            int stringWidth = softKeyFont_.stringWidth(label);
            int x = (rightCommand_ != null) ? 4 : (GameMIDlet::screenWidth >> 1) - (stringWidth >> 1);
            int textY = baselineY - 1;
            g.setColor(0);
            g.drawString(label, x - 1, textY, 36);
            g.drawString(label, x, textY - 1, 36);
            g.drawString(label, x + 1, textY, 36);
            g.drawString(label, x, textY + 1, 36);
            g.setColor(16777215);
            g.drawString(label, x, textY, 36);
        }
    }
    if (rightCommand_ == null) return;
    if (rightIcon_ != null) { g.drawImage(rightIcon_, GameMIDlet::screenWidth - 2, baselineY, 40); return; }
    int rightX = GameMIDlet::screenWidth - 3;
    baselineY--;
    String label = rightCommand_.getLabel();
    g.setColor(0);
    g.drawString(label, rightX - 1, baselineY, 40);
    g.drawString(label, rightX, baselineY - 1, 40);
    g.drawString(label, rightX + 1, baselineY, 40);
    g.drawString(label, rightX, baselineY + 1, 40);
    g.setColor(16777215);
    g.drawString(label, rightX, baselineY, 40);
}

void GameCanvas::serviceRepaints() {
    if (!repaintRequested_) return;
    repaintRequested_ = false;
    Graphics g(screen_);
    try {
        if (midlet_) {
            try {
                midlet_->paintCanvas(g);
                paintSoftKeys(g);
            } catch (const JavaException& e) { std::fprintf(stderr, "exception in paint: %s\n", e.what()); }
        }
        Graphics3D::flushPending();
    } catch (...) { throw; }
    shown_->px = screen_->px;
    frames_++;
}

} // namespace cbx
