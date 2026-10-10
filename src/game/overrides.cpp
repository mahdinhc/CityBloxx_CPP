// overrides.cpp - hand-written parts of otherwise generated classes: everything that touches the platform
// (files, window, threads) instead of game logic.
#include "classes.h"
#include "../platform/assets.h"
#include "../platform/canvas.h"
#include <cstdlib>

namespace cbx {

// ---------------------------------------------------------------------------------- asset streams
DataInputStream openAssetStream(const String& name) {
    std::vector<uint8_t> bytes;
    if (!Assets::read(name.toUtf8(), bytes)) return null;
    return DataInputStream(std::move(bytes));
}

ICanvas* PlatformFactory::createCanvas() { return new GameCanvas(); }

// ------------------------------------------------------------------------------------------- Lang
// lang/lang.<locale> is a table of big-endian ushort offsets followed by modified-UTF-8 strings.
namespace {
DataInputStream g_langStream = null;
}

String Lang::getString(int index, Arr<String> formatArgs) {
    String result;
    try {
        if (g_langStream == null) {
            const std::string loc = locale.toUtf8();
            std::vector<std::string> tries = {"lang/lang." + loc};
            size_t dash = loc.find('-');
            if (dash != std::string::npos && dash > 0) tries.push_back("lang/lang." + loc.substr(0, dash));
            tries.push_back("lang/lang.en-US");
            tries.push_back("lang/lang.xx");
            std::vector<uint8_t> bytes;
            bool ok = false;
            for (auto& t : tries) if (Assets::read(t, bytes)) { ok = true; break; }
            if (!ok) return u"X"_s;
            g_langStream = DataInputStream(std::move(bytes));
        }
        // The original re-wound the stream after every lookup, so each call starts from offset 0.
        g_langStream.rewind();
        g_langStream.skipBytes(index * 2);
        g_langStream.skipBytes((g_langStream.readUnsignedShort() - (index * 2)) - 2);
        result = g_langStream.readUTF();
        g_langStream.rewind();
        if (formatArgs != null) {
            if (formatArgs.length == 1) {
                result = replaceAll(result, u"%U"_s, formatArgs[0]);
            } else {
                for (int i = 0; i < formatArgs.length; i++)
                    result = replaceAll(result, u"%"_s + i + u"U"_s, formatArgs[i]);
            }
        }
    } catch (const IOException&) {
        g_langStream.rewind();
        result = u"E"_s;
    }
    return result;
}

// -------------------------------------------------------------------------------------- Storage
namespace {
DataOutputStream g_out = null;
std::string g_recordName;
bool g_pendingWrite = false;
}

DataInputStream Storage::openRead(String name) {
    std::vector<uint8_t> bytes;
    std::string n = name.toUtf8();
    for (auto& c : n) if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) c = '_';
    if (!SaveStore::read(n, bytes)) return null;
    return DataInputStream(std::move(bytes));
}
DataOutputStream Storage::openWrite(String name) {
    g_recordName = name.toUtf8();
    for (auto& c : g_recordName) if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) c = '_';
    g_out = DataOutputStream::create();
    g_pendingWrite = true;
    return g_out;
}
void Storage::flush() {
    if (!g_pendingWrite) return;
    g_pendingWrite = false;
    SaveStore::write(g_recordName, g_out.bytes());
}
void Storage::close() {
    try {
        if (g_pendingWrite) flush();
        g_out = null;
    } catch (const JavaException&) {}
}

// ------------------------------------------------------------------------------------ Resources
// assets/manifest.txt maps a resource id (only the low 15 bits of the packed ids used by the game code matter)
// to a file below the assets root. Resources are whole files.
namespace {
std::map<int, std::string> g_manifest;
}

void Resources::init() {
    try {
        g_manifest.clear();
        std::vector<uint8_t> bytes;
        if (!Assets::read("manifest.txt", bytes)) throw NullPointerException();   // new InputStreamReader(null)
        std::string text(bytes.begin(), bytes.end());
        size_t pos = 0;
        while (pos < text.size()) {                       // BufferedReader.readLine + trim
            size_t e = text.find_first_of("\r\n", pos);
            std::string line = text.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
            pos = e == std::string::npos ? text.size() : e + ((text[e] == '\r' && e + 1 < text.size() && text[e + 1] == '\n') ? 2 : 1);
            size_t b = 0, en = line.size();
            while (b < en && (unsigned char)line[b] <= ' ') b++;
            while (en > b && (unsigned char)line[en - 1] <= ' ') en--;
            line = line.substr(b, en - b);
            if (line.empty() || line[0] == '#') continue;
            size_t sp = line.find_first_of(" \t\f\v");
            if (sp == std::string::npos) continue;           // split("\\s+", 2) gave a single part
            size_t rest = line.find_first_not_of(" \t\f\v", sp);
            g_manifest[Integer::parseInt(String::fromUtf8(line.substr(0, sp)))] = line.substr(rest);
        }
        langInts = Arr<int>::sized(153);
        loadLangFile(Storage::getSetting(2));
        if (getString(152).compareTo(u"0"_s) == 0) Storage::setSetting(4, 1);
        else Storage::setSetting(4, 0);
    } catch (const JavaException&) {}
}

String Resources::pathOf(int resourceId) {
    if (resourceId == -1) return null;
    auto it = g_manifest.find(resourceId & 32767);
    return it == g_manifest.end() ? String(null) : String::fromUtf8(it->second);
}

Arr<int8_t> Resources::getBytes(int resourceId) {
    String path = pathOf(resourceId);
    if (path == null) return null;
    std::vector<uint8_t> b;
    if (!Assets::read(path.toUtf8(), b)) return null;
    Arr<int8_t> data = Arr<int8_t>::sized((int)b.size());
    if (!b.empty()) std::memcpy(data.data(), b.data(), b.size());
    return data;
}

DataInputStream Resources::openStream(int resourceId) {
    String path = pathOf(resourceId);
    if (path == null) return null;
    std::vector<uint8_t> b;
    if (!Assets::read(path.toUtf8(), b)) return null;
    return DataInputStream(std::move(b));
}

void Resources::close() {}
void Resources::loadLangFile(int) {}   // the optional lang/l<N> files are not part of the shipped data

// ----------------------------------------------------------------------------------- GameMIDlet
void GameMIDlet::startApp() {
    try {
        if (started) { resume(); return; }
        loading = true;
        onInit();
        onLoad();
        canvas->setMidlet(this);
        canvas->setActive(true);
        started = true;
    } catch (const JavaException& t) {
        std::fprintf(stderr, "exception in startApp: %s\n", t.what());
    }
}

void GameMIDlet::destroyApp(bool) {
    destroyedExternally = true;
    exitRequested = true;
}

void GameMIDlet::run() {}   // the original game loop thread; see stepFrame() / main loop

// One iteration of the original run() loop body (without the sleep).
void GameMIDlet::stepFrame() {
    try {
        if (!paused) {
            int i = calculateDeltaTime();
            currentScreen = getActiveScreen();
            if (currentScreen != nullptr) {
                currentScreen->onModeChange();
                Screen* p = getActiveScreen();
                if (currentScreen != p) {
                    if (p != nullptr) {
                        p->onEnter();
                        resetTiming();
                    }
                    currentScreen = p;
                } else if (currentScreen == menu) {
                    onMenuTick();
                }
                if (!(exitRequested || paused)) {
                    if (currentScreen != this) updateBackground(i);
                    currentScreen->update(i, elapsedMs);
                    repaintNow();
                }
            } else if (loading) {
                bool d = processLoading(i, elapsedMs);
                if (!(exitRequested || paused || !d)) repaintNow();
            }
        }
    } catch (const JavaException& t) {
        std::fprintf(stderr, "EXCEPTION in frame: %s\n", t.what());
    }
}

// Tail of the original run(): executed once when the loop ends.
void GameMIDlet::finish() {
    try {
        if (exitRequested) {
            sound->stopAll();
            saveSettingsRecord();
            saveSettings();
            saveGame();
            releaseResources();
            if (!destroyedExternally) notifyDestroyed();
        }
    } catch (const JavaException&) {}
}

// -------------------------------------------------------------------------------- MenuController
// The name-entry dialog (a Swing JDialog in the original) is an in-game overlay here (NameDialog).
void MenuController::enterSubMenu(int menuType, int menuParam, Arr<Command> cmds) {
    menuType = menuType;               // (sic) the original assigns the parameter to itself
    entryCount = menuParam;
    Storage::cursor = 0;
    if (menuType == 3) {
        int inputMode = menuParam == 1 ? 3 : 0;
        (void)inputMode;
        String initial = Storage::getText(0).equals(u"-"_s) ? String(u"") : Storage::getText(0);
        nameInput = TextField(u""_s, initial, 10, inputMode);
        nameField = nameInput;
        canvas->setActive(false);
        MenuController* self = this;
        NameDialog::open(Resources::getString(146), nameInput, cmds, [self](Command command) {
            if (command == self->deleteCommand) {
                self->caretPos = self->nameInput.getCaretPosition();
                if (self->caretPos != 0 && self->nameInput.size() != 0) self->nameInput.deleteChars(self->caretPos - 1, 1);
                return;
            }
            self->textSlot = 0;
            Storage::setText(self->textSlot, self->nameInput.getString().trim().replace(u'\n', u' '));
            self->nameInput = null;
            MenuController::getCanvas()->setActive(true);
            GameMIDlet::getInstance()->handleCommand(command);
        });
    }
    commands = cmds;
    if (cmds != null) installCommands();
}

} // namespace cbx
