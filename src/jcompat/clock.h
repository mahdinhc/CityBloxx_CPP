// clock.h - millisecond clock abstraction. The game's time-step logic depends on wall-clock deltas;
// tests substitute a virtual clock so that runs are deterministic.
#pragma once
#include <cstdint>

namespace cbx {
struct Clock {
    static int64_t nowMs();
    static void useVirtual(int64_t startMs);
    static void advance(int64_t ms);
    static bool isVirtual();
};
}
