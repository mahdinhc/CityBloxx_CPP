// jme.h - the small MIDP-like API surface (Command, Screen, ICanvas, MIDlet ...) the game logic is written against.
#pragma once
#include "../jcompat/jcompat.h"
#include "../gfx/graphics.h"
#include <functional>

namespace cbx {

class GameMIDlet;

// ------------------------------------------------------------------------------------------ Command
class Command {
public:
    static const int SCREEN = 1, BACK = 2, CANCEL = 3, OK = 4, HELP = 5, STOP = 6, EXIT = 7, ITEM = 8;
    Command() = default;
    Command(NullT) {}
    Command(const String& label, int type, int priority) : d_(std::make_shared<Impl>()) {
        d_->label = label == null ? String(u"") : label; d_->type = type; d_->priority = priority;
    }
    String getLabel() const { return impl().label; }
    int getCommandType() const { return impl().type; }
    int getPriority() const { return impl().priority; }
    bool operator==(const Command& o) const { return d_ == o.d_; }
    bool operator!=(const Command& o) const { return d_ != o.d_; }
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }
private:
    struct Impl { String label; int type = 0, priority = 0; };
    const Impl& impl() const { if (!d_) throw NullPointerException(); return *d_; }
    std::shared_ptr<Impl> d_;
};

// ------------------------------------------------------------------------------------------ TextField
class TextField {
public:
    TextField() = default;
    TextField(NullT) {}
    TextField(const String& /*label*/, const String& text, int maxSize, int /*constraints*/) : d_(std::make_shared<Impl>()) {
        d_->max = maxSize;
        d_->text = text == null ? std::u16string() : text.str();
        if ((int)d_->text.size() > maxSize) d_->text.resize((size_t)maxSize);
        d_->caret = (int)d_->text.size();
    }
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }
    String getString() const { return String(I().text); }
    void setString(const String& s) {
        I().text = s == null ? std::u16string() : s.str();
        if ((int)I().text.size() > I().max) I().text.resize((size_t)I().max);
        I().caret = (int)I().text.size();
    }
    int getCaretPosition() const { return I().caret; }
    int size() const { return (int)I().text.size(); }
    int getMaxSize() const { return I().max; }
    void deleteChars(int offset, int length) {
        auto& t = I().text;
        if (offset >= 0 && offset + length <= (int)t.size()) t.erase((size_t)offset, (size_t)length);
        if (I().caret > (int)t.size()) I().caret = (int)t.size();
    }
    void insert(char16_t c) {
        if ((int)I().text.size() < I().max) { I().text.insert((size_t)I().caret, 1, c); I().caret++; }
    }
    bool backspace() {
        auto& t = I().text;
        if (I().caret > 0 && !t.empty()) { t.erase((size_t)I().caret - 1, 1); I().caret--; return true; }
        return false;
    }
private:
    struct Impl { std::u16string text; int max = 0; int caret = 0; };
    Impl& I() const { if (!d_) throw NullPointerException(); return *d_; }
    std::shared_ptr<Impl> d_;
};

// ------------------------------------------------------------------------------------------ interfaces
class Screen {
public:
    virtual ~Screen() = default;
    virtual void update(int delta, int time) = 0;
    virtual void commandAction(Command command) = 0;
    virtual void paint(Graphics g, bool fullRedraw) = 0;
    virtual void keyPressed(int keyCode, int gameAction) = 0;
    virtual void onEnter() = 0;
    virtual void onModeChange() = 0;
};
class ModalScreen : public Screen {};

class ICanvas {
public:
    virtual ~ICanvas() = default;
    virtual void setMidlet(GameMIDlet* midlet) = 0;
    virtual void removeCommand(Command command) = 0;
    virtual void setCommand(Command command, Image image) = 0;
    virtual void setActive(bool active) = 0;
    virtual int getGameAction(int keyCode) = 0;
    virtual int getHeight() = 0;
    virtual String getKeyName(int keyCode) = 0;
    virtual int getWidth() = 0;
};

class MIDlet {
public:
    virtual ~MIDlet() = default;
    virtual void startApp() = 0;
    virtual void pauseApp() = 0;
    virtual void destroyApp(bool unconditional) = 0;
    String getAppProperty(const String& key) const;
    void notifyDestroyed() { destroyedNotified = true; }
    void startFromLauncher() { startApp(); }
    void destroyFromLauncher() { destroyApp(true); }
    bool destroyedNotified = false;
};

// ------------------------------------------------------------------------------------------ name dialog
// The original opened a Swing dialog for the high-score name. Here it is an in-game overlay model;
// the platform (SDL) renders it and feeds it characters.
struct NameDialog {
    static bool active;
    static String title;
    static TextField field;
    static Arr<Command> commands;     // OK-type commands shown as buttons
    static std::function<void(Command)> onCommand;
    static void open(const String& t, const TextField& f, const Arr<Command>& cmds, std::function<void(Command)> cb);
    static void submit();             // fire first command (Enter)
    static void close() { active = false; }
};

} // namespace cbx
