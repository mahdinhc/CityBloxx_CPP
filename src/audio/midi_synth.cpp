#include "midi_synth.h"
#include <algorithm>
#include <cmath>

namespace cbx {

namespace {
struct Rd {
    const uint8_t* p; size_t n, i = 0;
    bool ok(size_t k = 1) const { return i + k <= n; }
    int u8() { return ok() ? p[i++] : 0; }
    uint32_t be(int k) { uint32_t v = 0; while (k--) v = (v << 8) | (uint32_t)u8(); return v; }
    uint32_t vlq() { uint32_t v = 0; for (int k = 0; k < 4 && ok(); k++) { int b = u8(); v = (v << 7) | (b & 127); if (!(b & 128)) break; } return v; }
};
struct RawEv { uint64_t tick; uint8_t status, d1, d2; uint32_t tempo; bool isTempo; };
const double kPi = 3.14159265358979323846;
}

void MidiSynth::reset() {
    for (auto& c : ch_) c = Chan();
    for (auto& v : voices_) v = Voice();
    ch_[9].program = 0;
}

bool MidiSynth::play(const std::vector<uint8_t>& smf, int loops) {
    events_.clear(); stop();
    Rd r{smf.data(), smf.size()};
    if (r.be(4) != 0x4D546864u) return false;
    uint32_t hlen = r.be(4);
    int fmt = (int)r.be(2), ntr = (int)r.be(2); int div = (int)r.be(2);
    (void)fmt;
    r.i = 8 + hlen;
    if (div & 0x8000 || div == 0) return false;
    std::vector<RawEv> evs;
    for (int t = 0; t < ntr && r.ok(8); t++) {
        if (r.be(4) != 0x4D54726Bu) break;
        uint32_t len = r.be(4);
        size_t end = std::min(r.n, r.i + len);
        uint64_t tick = 0; uint8_t running = 0;
        while (r.i < end) {
            tick += r.vlq();
            int b = r.u8();
            if (b == 0xFF) {
                int type = r.u8(); uint32_t l = r.vlq();
                if (type == 0x51 && l == 3) { uint32_t tp = r.be(3); evs.push_back({tick, 0, 0, 0, tp, true}); }
                else r.i += l;
            } else if (b == 0xF0 || b == 0xF7) { uint32_t l = r.vlq(); r.i += l; }
            else {
                uint8_t st;
                if (b & 0x80) { st = (uint8_t)b; running = st; } else { st = running; r.i--; }
                int hi = st & 0xF0;
                uint8_t d1 = (uint8_t)r.u8(), d2 = 0;
                if (hi != 0xC0 && hi != 0xD0) d2 = (uint8_t)r.u8();
                evs.push_back({tick, st, d1, d2, 0, false});
            }
        }
        r.i = end;
    }
    std::stable_sort(evs.begin(), evs.end(), [](const RawEv& a, const RawEv& b) { return a.tick < b.tick; });
    // ticks -> samples using the tempo map
    double tempo = 500000.0, samples = 0; uint64_t lastTick = 0;
    for (auto& e : evs) {
        samples += (double)(e.tick - lastTick) * tempo / (double)div / 1e6 * rate_;
        lastTick = e.tick;
        if (e.isTempo) tempo = (double)e.tempo;
        else events_.push_back({samples, e.status, e.d1, e.d2});
    }
    lengthSamples_ = samples;
    loops_ = loops;
    startSong();
    return !events_.empty();
}

void MidiSynth::startSong() { reset(); next_ = 0; pos_ = 0; playing_ = true; }
void MidiSynth::stop() { playing_ = false; for (auto& v : voices_) v.active = false; }

void MidiSynth::allOff(int ch) { for (auto& v : voices_) if (v.active && v.ch == ch) v.released = true; }

void MidiSynth::noteOn(int ch, int note, int vel) {
    if (vel == 0) { noteOff(ch, note); return; }
    Voice* slot = nullptr;
    for (auto& v : voices_) if (!v.active) { slot = &v; break; }
    if (!slot) { float best = -1; for (auto& v : voices_) { float score = v.released ? 1e9f + v.age : v.age; if (score > best) { best = score; slot = &v; } } }
    Voice& v = *slot;
    v = Voice();
    v.active = true; v.ch = ch; v.note = note; v.vel = vel / 127.f; v.program = ch_[ch].program;
    v.freq = 440.f * std::pow(2.f, (note - 69) / 12.f);
    if (ch == 9) {
        v.drumKind = note;
        v.atkRate = 1.f; v.sus = 0; v.relRate = 0.001f;
        bool kick = note == 35 || note == 36, snare = note == 38 || note == 40 || note == 37 || note == 39;
        bool hat = note == 42 || note == 44 || note == 46, cym = note == 49 || note == 51 || note == 52 || note == 55 || note == 57 || note == 59;
        v.decRate = kick ? 0.00035f : snare ? 0.0005f : hat ? (note == 46 ? 0.00018f : 0.0012f) : cym ? 0.00007f : 0.0004f;
        v.stage = 1; v.env = 1;
    } else {
        int p = v.program;
        if (p < 8) { v.atkRate = 0.5f; v.decRate = 0.00012f; v.sus = 0.0f; }          // piano: decays
        else if (p >= 24 && p < 32) { v.atkRate = 0.4f; v.decRate = 0.00010f; v.sus = 0.1f; }   // guitar
        else if (p >= 40 && p < 56) { v.atkRate = 0.003f; v.sus = 0.85f; v.decRate = 0.00001f; }  // strings/ensemble
        else if (p >= 88 && p < 96) { v.atkRate = 0.0015f; v.sus = 0.9f; v.decRate = 0.f; }   // pads
        else { v.atkRate = 0.03f; v.sus = 0.8f; v.decRate = 0.00002f; }
        v.relRate = (p >= 88 && p < 96) ? 0.0003f : 0.0012f;
    }
}

void MidiSynth::noteOff(int ch, int note) {
    for (auto& v : voices_) if (v.active && !v.released && v.ch == ch && v.note == note && ch != 9) v.released = true;
}

void MidiSynth::handle(const Event& e) {
    int ch = e.status & 15;
    switch (e.status & 0xF0) {
        case 0x90: noteOn(ch, e.d1, e.d2); break;
        case 0x80: noteOff(ch, e.d1); break;
        case 0xC0: ch_[ch].program = e.d1; break;
        case 0xE0: ch_[ch].bend = ((e.d2 << 7 | e.d1) - 8192) / 8192.f * 2.f; break;
        case 0xB0:
            if (e.d1 == 7) ch_[ch].vol = e.d2 / 127.f;
            else if (e.d1 == 11) ch_[ch].expr = e.d2 / 127.f;
            else if (e.d1 == 10) ch_[ch].pan = e.d2 / 127.f;
            else if (e.d1 == 123 || e.d1 == 120) allOff(ch);
            else if (e.d1 == 121) { ch_[ch].expr = 1; ch_[ch].bend = 0; }
            break;
    }
}

float MidiSynth::renderVoice(Voice& v, float& panOut) {
    const Chan& c = ch_[v.ch];
    panOut = c.pan;
    float out;
    rng_ = rng_ * 1664525u + 1013904223u;
    float nz = ((rng_ >> 8) & 0xFFFF) / 32768.f - 1.f;
    if (v.drumKind >= 0) {
        int n = v.drumKind;
        if (n == 35 || n == 36) {                       // kick: pitch-swept sine
            float f = 45.f + 110.f * v.env * v.env;
            v.phase += f / rate_; if (v.phase > 1) v.phase -= 1;
            out = std::sin(2 * (float)kPi * v.phase) * 1.1f;
        } else if (n == 38 || n == 40 || n == 37 || n == 39) {   // snare/rim/clap
            v.phase += 190.f / rate_; if (v.phase > 1) v.phase -= 1;
            out = nz * 0.7f + std::sin(2 * (float)kPi * v.phase) * 0.4f * v.env;
        } else if (n == 42 || n == 44 || n == 46 || n == 49 || n == 51 || n == 52 || n == 55 || n == 57 || n == 59) {  // hats/cymbals
            v.lp = nz - v.lp * 0.5f; out = v.lp * 0.35f;
        } else {                                        // toms and misc percussion
            float f = 70.f + (n - 41) * 14.f;
            v.phase += f * (0.8f + 0.4f * v.env) / rate_; if (v.phase > 1) v.phase -= 1;
            out = std::sin(2 * (float)kPi * v.phase) * 0.9f;
        }
        v.env -= v.decRate; if (v.env <= 0.001f) { v.active = false; return 0; }
        return out * v.env * v.vel * c.vol * c.expr;
    }
    // melodic voice
    float f = v.freq * std::pow(2.f, c.bend / 12.f);
    // vibrato-less oscillators
    v.phase += f / rate_; if (v.phase >= 1) v.phase -= 1;
    float ph = v.phase, p = v.program;
    float s;
    if (p < 8) s = std::sin(2 * (float)kPi * ph) * 0.7f + std::sin(4 * (float)kPi * ph) * 0.2f + std::sin(6 * (float)kPi * ph) * 0.08f;
    else if (p < 24) s = std::sin(2 * (float)kPi * ph) * 0.5f + std::sin(4 * (float)kPi * ph) * 0.3f + std::sin(6 * (float)kPi * ph) * 0.2f;
    else if (p < 40) s = (ph * 2 - 1) * 0.35f + std::sin(2 * (float)kPi * ph) * 0.5f;
    else if (p < 56) s = (ph * 2 - 1) * 0.45f + std::sin(2 * (float)kPi * ph) * 0.3f;
    else if (p < 80) s = (ph < 0.5f ? 1.f : -1.f) * 0.3f + std::sin(2 * (float)kPi * ph) * 0.4f;
    else if (p < 88) s = (ph * 2 - 1) * 0.4f;
    else s = std::sin(2 * (float)kPi * ph) * 0.6f + std::sin(2 * (float)kPi * ph * 1.003f) * 0.3f;
    // envelope
    if (v.released) { v.env -= v.relRate; if (v.env <= 0) { v.active = false; return 0; } }
    else if (v.stage == 0) { v.env += v.atkRate; if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } }
    else { if (v.env > v.sus) v.env -= v.decRate; if (v.env < v.sus) v.env = v.sus; if (v.sus == 0 && v.env <= 0.0005f) { v.active = false; return 0; } }
    v.age += 1;
    return s * v.env * v.vel * c.vol * c.expr * 0.8f;
}

void MidiSynth::render(int16_t* out, int frames) {
    std::fill(out, out + frames * 2, (int16_t)0);
    if (!playing_) return;
    int done = 0;
    while (done < frames) {
        // dispatch events due now
        while (next_ < events_.size() && events_[next_].t <= pos_) handle(events_[next_++]);
        int chunk = frames - done;
        if (next_ < events_.size()) chunk = std::min<double>(chunk, std::max(1.0, std::ceil(events_[next_].t - pos_)));
        for (int i = 0; i < chunk; i++) {
            float l = 0, rgt = 0;
            for (auto& v : voices_) {
                if (!v.active) continue;
                float pan; float s = renderVoice(v, pan);
                l += s * (1.f - pan); rgt += s * pan;
            }
            l = std::tanh(l * 1.0f) * 0.9f; rgt = std::tanh(rgt * 1.0f) * 0.9f;
            out[(done + i) * 2] = (int16_t)(l * 32000); out[(done + i) * 2 + 1] = (int16_t)(rgt * 32000);
        }
        pos_ += chunk; done += chunk;
        if (next_ >= events_.size()) {
            bool any = false; for (auto& v : voices_) if (v.active) any = true;
            if (!any || pos_ > lengthSamples_ + rate_ * 2.0) {
                if (loops_ == -1 || --loops_ > 0) startSong();
                else { playing_ = false; break; }
            }
        }
    }
}

} // namespace cbx
