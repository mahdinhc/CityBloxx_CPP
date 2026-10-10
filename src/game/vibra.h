#pragma once
#include "game_base.h"
namespace cbx {
// Desktop has no vibration motor: the original was a no-op as well.
class Vibra {
public:
    void vibrate(int /*ms*/) {}
    void setEnabled(bool e) { enabled_ = e; }
private:
    bool enabled_ = true;
};
}
