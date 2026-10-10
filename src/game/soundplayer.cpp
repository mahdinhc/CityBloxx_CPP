#include "classes.h"
#include "../audio/audio.h"

namespace cbx {

AudioBackend* Audio::backend = nullptr;

void SoundPlayer::play(int resourceId, int loopCount) {
    if (enabled_ && resourceId != -1) {
        if (ownsPlayer_ && playing_) { if (Audio::backend) Audio::backend->stop(); playing_ = false; }
        ownsPlayer_ = true;
        try {
            Arr<int8_t> bytes = Resources::getBytes(resourceId);
            if (bytes != null && Audio::backend) {
                std::vector<uint8_t> smf((uint8_t*)bytes.data(), (uint8_t*)bytes.data() + bytes.length);
                Audio::backend->playMidi(smf, loopCount);
                playing_ = true;
            }
        } catch (const JavaException&) {}
    }
}
void SoundPlayer::preload(int, bool) {}
void SoundPlayer::stopAll() {
    if (ownsPlayer_ && playing_) { if (Audio::backend) Audio::backend->stop(); playing_ = false; }
    ownsPlayer_ = false;
}
void SoundPlayer::setEnabled(bool e) {
    enabled_ = e;
    if (!enabled_) stopAll();
}

} // namespace cbx
