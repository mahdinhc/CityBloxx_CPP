// bootstrap.h - brings the game up (assets, fonts, static state) independent of the window backend.
#pragma once
#include <cstdint>
#include <string>

namespace cbx {
class House;
struct Options {
    std::string assets = "assets";
    std::string saves = "saves";
    std::string locale;            // e.g. "en-US"; empty = detect from environment
    int64_t seed = 0;              // 0 = time based
    bool virtualClock = false;     // deterministic time for tests
};
struct Bootstrap {
    static std::string detectLocale();
    static House* create(const Options& o);
};
}
