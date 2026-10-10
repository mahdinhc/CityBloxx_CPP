// audio.h - music/sfx playback interface. The SDL backend plugs in a MIDI synthesiser; tests use a recorder.
#pragma once
#include <cstdint>
#include <vector>

namespace cbx {
class AudioBackend {
public:
    virtual ~AudioBackend() = default;
    // Play a Standard MIDI File. loops == -1: forever, otherwise play it 'loops' times in total.
    virtual void playMidi(const std::vector<uint8_t>& smf, int loops) = 0;
    virtual void stop() = 0;
};
struct Audio {
    static AudioBackend* backend;     // may be null (silent)
};
}
