#include "clock.h"
#include <chrono>

namespace cbx {
namespace { bool g_virtual = false; int64_t g_now = 0; }
int64_t Clock::nowMs() {
    if (g_virtual) return g_now;
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
void Clock::useVirtual(int64_t startMs) { g_virtual = true; g_now = startMs; }
void Clock::advance(int64_t ms) { g_now += ms; }
bool Clock::isVirtual() { return g_virtual; }
}
