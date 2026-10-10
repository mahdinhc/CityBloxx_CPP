// midi_synth.h - Standard MIDI File sequencer + small polyphonic software synthesiser.
// Self-contained (no soundfont, no external library). Not tied to any audio API: call render().
#pragma once
#include <cstdint>
#include <vector>
#include <array>

namespace cbx {

class MidiSynth {
public:
    explicit MidiSynth(int sampleRate = 44100) : rate_(sampleRate) {}

    // Parses an SMF and starts playing it from the beginning. loops==-1: forever, otherwise 'loops' plays.
    bool play(const std::vector<uint8_t>& smf, int loops);
    void stop();
    bool isPlaying() const { return playing_; }

    // Renders interleaved stereo int16 samples (adds nothing when idle: output is zeroed).
    void render(int16_t* out, int frames);

    // total length of the loaded song in seconds (tests)
    double lengthSeconds() const { return lengthSamples_ / (double)rate_; }

private:
    struct Event { double t; uint8_t status, d1, d2; };   // t in samples
    struct Voice {
        bool active = false, released = false;
        int ch = 0, note = 0, program = 0;
        float vel = 0, phase = 0, phase2 = 0, freq = 0, env = 0, age = 0, noise = 1, lp = 0;
        int stage = 0;               // 0 attack, 1 decay/sustain
        int drumKind = -1;
        float relRate = 0.0005f, atkRate = 0.01f, sus = 0.7f, decRate = 0.00002f;
    };
    struct Chan { int program = 0; float vol = 100 / 127.f, expr = 1, pan = 0.5f, bend = 0; };

    void reset();
    void handle(const Event& e);
    void noteOn(int ch, int note, int vel);
    void noteOff(int ch, int note);
    void allOff(int ch);
    void startSong();
    float renderVoice(Voice& v, float& panOut);

    int rate_;
    std::vector<Event> events_;
    size_t next_ = 0;
    double pos_ = 0, lengthSamples_ = 0;
    int loops_ = 0;
    bool playing_ = false;
    std::array<Chan, 16> ch_{};
    std::array<Voice, 40> voices_{};
    uint32_t rng_ = 22222;
};

} // namespace cbx
