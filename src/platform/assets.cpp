#include "assets.h"
#include "jme.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

namespace cbx {

static std::string g_assetRoot = "assets";
static std::string g_saveRoot = "saves";

void Assets::setRoot(const std::string& dir) { g_assetRoot = dir; }
const std::string& Assets::root() { return g_assetRoot; }

static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}
bool Assets::isValidRoot(const std::string& dir) {
    std::ifstream f(dir + "/manifest.txt", std::ios::binary);
    return (bool)f;
}

bool Assets::read(const std::string& name, std::vector<uint8_t>& out) {
    std::string clean = name;
    for (auto& c : clean) if (c == '\\') c = '/';
    while (!clean.empty() && clean[0] == '/') clean.erase(0, 1);
    if (clean.find("..") != std::string::npos) return false;
    return readFile(g_assetRoot + "/" + clean, out);
}

std::string Assets::dirOf(const std::string& path) {
    size_t i = path.rfind('/');
    return i == std::string::npos ? std::string() : path.substr(0, i + 1);
}

void SaveStore::setRoot(const std::string& dir) { g_saveRoot = dir; MKDIR(dir.c_str()); }
bool SaveStore::read(const std::string& name, std::vector<uint8_t>& out) { return readFile(g_saveRoot + "/" + name + ".rms", out); }
bool SaveStore::write(const std::string& name, const std::vector<uint8_t>& data) {
    MKDIR(g_saveRoot.c_str());
    std::ofstream f(g_saveRoot + "/" + name + ".rms", std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    return (bool)f;
}

// ---- MIDlet application properties (the JAD attributes the desktop shim supplied)
String MIDlet::getAppProperty(const String& key) const {
    std::string k = key.toUtf8();
    if (k == "DChoc-Rain-Probability") return String(u"20");
    if (k == "DChoc-Rain-Duration") return String(u"60");
    if (k == "DChoc-Snow-Probability") return String(u"30");
    if (k == "DChoc-Snow-Duration") return String(u"50");
    return String();
}

// ---- NameDialog model
bool NameDialog::active = false;
String NameDialog::title;
TextField NameDialog::field;
Arr<Command> NameDialog::commands;
std::function<void(Command)> NameDialog::onCommand;
void NameDialog::open(const String& t, const TextField& f, const Arr<Command>& cmds, std::function<void(Command)> cb) {
    title = t; field = f; commands = cmds; onCommand = std::move(cb); active = true;
}
void NameDialog::submit() {
    if (!active) return;
    Command c = (commands != null && commands.length > 0) ? commands[0] : Command();
    auto cb = onCommand;
    active = false;
    if (cb) cb(c);
}

} // namespace cbx
