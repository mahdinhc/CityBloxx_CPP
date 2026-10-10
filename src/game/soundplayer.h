#pragma once
#include "game_base.h"
namespace cbx {
// Port of SoundPlayer: one active player at a time, owned by the caller of play().
class SoundPlayer {
public:
    void play(int resourceId, int loopCount);
    void preload(int resourceId, bool load);
    void stopAll();
    void setEnabled(bool e);
private:
    bool enabled_ = true;
    bool ownsPlayer_ = false;
    bool playing_ = false;
};
}
