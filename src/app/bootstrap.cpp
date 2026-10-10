#include "bootstrap.h"
#include "../game/classes.h"
#include "../platform/assets.h"
#include "../platform/canvas.h"
#include "../jcompat/clock.h"
#include <cstdlib>
#include <stdexcept>

namespace cbx {

// Equivalent of Locale.getDefault().toLanguageTag() from POSIX environment variables.
std::string Bootstrap::detectLocale() {
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"}) {
        const char* v = std::getenv(var);
        if (!v || !*v) continue;
        std::string s(v);
        size_t cut = s.find_first_of(".@:");
        if (cut != std::string::npos) s = s.substr(0, cut);
        if (s == "C" || s == "POSIX" || s.empty()) continue;
        for (auto& c : s) if (c == '_') c = '-';
        return s;
    }
    return "en-US";
}

House* Bootstrap::create(const Options& o) {
    Assets::setRoot(o.assets);
    SaveStore::setRoot(o.saves);
    if (o.virtualClock) Clock::useVirtual(1000000);

    if (!Assets::isValidRoot(o.assets)) throw std::runtime_error("assets directory '" + o.assets + "' not found or has no manifest.txt");
    std::vector<uint8_t> atlas;
    if (!Assets::read("fonts/fontatlas.bin", atlas)) throw std::runtime_error("assets/fonts/fontatlas.bin not found (assets dir: " + o.assets + ")");
    Font::loadAtlas(atlas);

    Lang::locale = String::fromUtf8(o.locale.empty() ? detectLocale() : o.locale);

    // The canvas size is fixed; the original set these when the canvas was created, before any lazily
    // initialised class (HighScores...) looked at them.
    GameMIDlet::screenWidth = GameCanvas::LOGICAL_W;
    GameMIDlet::screenHeight = GameCanvas::LOGICAL_H;
    Lang::staticInit(); Storage::staticInit(); Resources::staticInit(); GameMIDlet::staticInit(); Ui::staticInit();
    MenuController::staticInit(); HighScores::staticInit(); CityMode::staticInit(); House::staticInit();

    int64_t seed = o.seed != 0 ? o.seed : Clock::nowMs();
    House::rng = Random(seed);

    House* h = new House();
    h->startFromLauncher();
    return h;
}

} // namespace cbx
