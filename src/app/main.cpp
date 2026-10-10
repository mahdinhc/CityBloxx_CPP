// City Bloxx - C++ / SDL3 port. Window, input, audio and frame pacing around the (SDL-free) game core.
#include "bootstrap.h"
#include "name_dialog_view.h"
#include "png_writer.h"
#include "../audio/audio.h"
#include "../audio/midi_synth.h"
#include "../game/classes.h"
#include "../gfx/codec.h"
#include "../platform/assets.h"
#include "../platform/canvas.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>

using namespace cbx;

namespace {

// ------------------------------------------------------------------------------------------ audio
class SynthBackend : public AudioBackend {
public:
    SynthBackend() : synth_(44100) {}
    bool open() {
        SDL_AudioSpec spec;
        spec.freq = 44100;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &SynthBackend::callback, this);
        if (!stream_) {
            std::fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
            return false;
        }
        SDL_ResumeAudioStreamDevice(stream_);
        return true;
    }
    void close() {
        if (stream_) {
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
    }
    void playMidi(const std::vector<uint8_t>& smf, int loops) override {
        if (!stream_) return;
        SDL_LockAudioStream(stream_);
        synth_.play(smf, loops);
        SDL_UnlockAudioStream(stream_);
    }
    void stop() override {
        if (!stream_) return;
        SDL_LockAudioStream(stream_);
        synth_.stop();
        SDL_UnlockAudioStream(stream_);
    }
private:
    static void SDLCALL callback(void* ud, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/) {
        if (additional_amount <= 0) return;
        int frames = additional_amount / 4;
        std::vector<int16_t> buf(frames * 2);
        static_cast<SynthBackend*>(ud)->synth_.render(buf.data(), frames);
        SDL_PutAudioStreamData(stream, buf.data(), (int)(buf.size() * sizeof(int16_t)));
    }
    MidiSynth synth_;
    SDL_AudioStream* stream_ = nullptr;
};

// ------------------------------------------------------------------------------------------ input
// Same mapping as the original desktop shim (J2ME key codes).
int mapKey(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP: return -1;
        case SDLK_DOWN: return -2;
        case SDLK_LEFT: return -3;
        case SDLK_RIGHT: return -4;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return -5;
        case SDLK_F1: case SDLK_Z: return -6;
        case SDLK_F2: case SDLK_X: case SDLK_ESCAPE: return -7;
        case SDLK_BACKSPACE: case SDLK_DELETE: return -8;
        case SDLK_W: return 50;
        case SDLK_A: return 52;
        case SDLK_S: return 56;
        case SDLK_D: return 54;
        case SDLK_KP_MULTIPLY: case SDLK_ASTERISK: return 42;
        case SDLK_HASH: return 35;
        default: break;
    }
    if (k >= SDLK_0 && k <= SDLK_9) return 48 + (int)(k - SDLK_0);
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return 49 + (int)(k - SDLK_KP_1);
    if (k == SDLK_KP_0) return 48;
    return 0;
}

std::u16string utf8ToUtf16(const char* s) { return String::fromUtf8(s).str(); }

// ------------------------------------------------------------------------------------------ app
struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool linearFilter = false;
    bool smoothAllowed = true;
    int scaleOpt = 1;
    bool fullscreen = false;

    void makeTexture(bool linear) {
        if (texture) SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
        if (texture) {
            SDL_SetTextureScaleMode(texture, linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        }
        linearFilter = linear;
    }
    // Letterboxed destination rectangle in renderer pixels (same maths as the original panel).
    SDL_FRect destRect() {
        int ow = 0, oh = 0;
        SDL_GetRenderOutputSize(renderer, &ow, &oh);
        double s = std::min(ow / (double)SCREEN_W, oh / (double)SCREEN_H);
        float dw = (float)std::lround(SCREEN_W * s), dh = (float)std::lround(SCREEN_H * s);
        return SDL_FRect{(ow - dw) * 0.5f, (oh - dh) * 0.5f, dw, dh};
    }
    void present(const std::vector<uint32_t>& frame) {
        SDL_FRect dst = destRect();
        double s = dst.w / (double)SCREEN_W;
        bool integer = std::fabs(s - std::round(s)) < 0.001;
        bool wantLinear = smoothAllowed && !integer;
        if (wantLinear != linearFilter) makeTexture(wantLinear);
        SDL_UpdateTexture(texture, nullptr, frame.data(), SCREEN_W * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);
    }
    // window coordinates -> logical game coordinates (or false if outside the picture)
    bool toLogical(float wx, float wy, int& x, int& y) {
        int ww = 0, wh = 0, ow = 0, oh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        SDL_GetRenderOutputSize(renderer, &ow, &oh);
        if (ww <= 0 || wh <= 0) return false;
        double px = wx * (ow / (double)ww), py = wy * (oh / (double)wh);
        SDL_FRect d = destRect();
        double s = d.w / (double)SCREEN_W;
        x = (int)((px - d.x) / s); y = (int)((py - d.y) / s);
        return x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H;
    }
    void toggleFullscreen() {
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(window, fullscreen);
    }
};

bool pathExists(const std::string& p) { struct stat st; return ::stat(p.c_str(), &st) == 0; }

std::string findAssets(const std::string& hint) {
    std::vector<std::string> cands;
    if (!hint.empty()) cands.push_back(hint);
    if (const char* base = SDL_GetBasePath()) {
        cands.push_back(std::string(base) + "assets");
        cands.push_back(std::string(base) + "../share/citybloxx/assets");
    }
    cands.push_back("assets");
#ifdef CBX_ASSET_DIR
    cands.push_back(CBX_ASSET_DIR);
#endif
    for (auto& c : cands) if (Assets::isValidRoot(c)) return c;
    return hint.empty() ? "assets" : hint;
}

std::string defaultSaveDir() {
    if (pathExists("saves")) return "saves";           // keep using saves next to the game if they already exist
    char* pref = SDL_GetPrefPath("CityBloxx", "CityBloxx");
    std::string r = pref ? pref : "saves";
    if (pref) SDL_free(pref);
    return r;
}

void usage() {
    std::puts("City Bloxx (C++/SDL3)\n"
              "  --assets DIR     game data directory (default: ./assets next to the executable)\n"
              "  --saves DIR      save directory\n"
              "  --lang TAG       language tag, e.g. en-US, de, fr (default: system locale)\n"
              "  --scale N        initial window scale (default 1)\n"
              "  --fullscreen     start fullscreen\n"
              "  --no-smooth      always use nearest-neighbour scaling\n"
              "  --no-audio       disable sound\n"
              "Keys: arrows/WASD move, Enter/Space/5 select, F1/Z left soft key, F2/X/Esc right soft key,\n"
              "      0-9 digits, F11 or Alt+Enter fullscreen, F12 screenshot.");
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    App app;
    bool audio = true;
    std::string assetHint;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--assets") assetHint = next();
        else if (a == "--saves") opt.saves = next();
        else if (a == "--lang") opt.locale = next();
        else if (a == "--scale") app.scaleOpt = std::max(1, std::atoi(next().c_str()));
        else if (a == "--fullscreen") app.fullscreen = true;
        else if (a == "--no-smooth") app.smoothAllowed = false;
        else if (a == "--no-audio") audio = false;
        else if (a == "--seed") opt.seed = std::atoll(next().c_str());
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); usage(); return 2; }
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    opt.assets = findAssets(assetHint);
    if (opt.saves == "saves") opt.saves = defaultSaveDir();
    if (opt.locale.empty()) {
        int count = 0;
        SDL_Locale** loc = SDL_GetPreferredLocales(&count);
        if (loc && count > 0 && loc[0] && loc[0]->language) {
            opt.locale = loc[0]->language;
            if (loc[0]->country) opt.locale += std::string("-") + loc[0]->country;
        }
        if (loc) SDL_free(loc);
        if (opt.locale.empty()) opt.locale = Bootstrap::detectLocale();
    }

    app.window = SDL_CreateWindow("City Bloxx", SCREEN_W * app.scaleOpt, SCREEN_H * app.scaleOpt,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!app.window) { std::fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_SetWindowPosition(app.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowMinimumSize(app.window, SCREEN_W, SCREEN_H);

    app.renderer = SDL_CreateRenderer(app.window, nullptr);
    if (!app.renderer) { std::fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
    app.makeTexture(false);
    if (app.fullscreen) SDL_SetWindowFullscreen(app.window, true);

    House* game = nullptr;
    try { game = Bootstrap::create(opt); }
    catch (const std::exception& e) { std::fprintf(stderr, "startup failed: %s\n", e.what()); return 1; }

    // window icon from the game's own icon.png
    {
        std::vector<uint8_t> png;
        if (Assets::read("images/icon.png", png)) {
            try {
                static DecodedImage icon = decodePng(png.data(), png.size());
                SDL_Surface* s = SDL_CreateSurfaceFrom(icon.w, icon.h, SDL_PIXELFORMAT_ARGB8888, icon.argb.data(), icon.w * 4);
                if (s) { SDL_SetWindowIcon(app.window, s); SDL_DestroySurface(s); }
            } catch (const std::exception&) {}
        }
    }

    SynthBackend synth;
    if (audio && synth.open()) Audio::backend = &synth;

    bool textInput = false, running = true;
    uint32_t shotCounter = 0;
    while (running) {
        uint64_t frameStart = SDL_GetTicks();
        GameCanvas* canvas = GameCanvas::current;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT: game->destroyFromLauncher(); break;
                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keycode k = e.key.key;
                    bool alt = (e.key.mod & SDL_KMOD_ALT) != 0;
                    if (k == SDLK_F11 || (alt && (k == SDLK_RETURN || k == SDLK_KP_ENTER))) { if (!e.key.repeat) app.toggleFullscreen(); break; }
                    if (k == SDLK_F12) {
                        if (!e.key.repeat && canvas) {
                            std::string name = "citybloxx_" + std::to_string((long)std::time(nullptr)) + "_" + std::to_string(shotCounter++) + ".png";
                            writePng(name, canvas->shown(), SCREEN_W, SCREEN_H);
                            std::fprintf(stderr, "saved %s\n", name.c_str());
                        }
                        break;
                    }
                    if (NameDialog::active) {
                        if (k == SDLK_BACKSPACE) NameDialog::field.backspace();
                        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_ESCAPE) NameDialog::submit();
                        break;
                    }
                    int code = mapKey(k);
                    if (code != 0 && canvas) canvas->keyDown(code, e.key.repeat != 0);
                    break;
                }
                case SDL_EVENT_KEY_UP: {
                    if (NameDialog::active) break;
                    int code = mapKey(e.key.key);
                    if (code != 0 && canvas) canvas->keyUp(code);
                    break;
                }
                case SDL_EVENT_TEXT_INPUT:
                    if (NameDialog::active) {
                        for (char16_t c : utf8ToUtf16(e.text.text)) if (c >= 32 && c != 127) NameDialog::field.insert(c);
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (e.button.button == SDL_BUTTON_LEFT && canvas && !NameDialog::active) {
                        int x, y;
                        if (app.toLogical(e.button.x, e.button.y, x, y)) canvas->pointerDown(x, y);
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    if (canvas && !NameDialog::active && e.wheel.y != 0.0f) {
                        int code = e.wheel.y > 0 ? -1 : -2;
                        canvas->keyDown(code, false); canvas->keyUp(code);
                    }
                    break;
                default: break;
            }
        }
        // text entry only while the name prompt is up
        if (NameDialog::active != textInput) {
            textInput = NameDialog::active;
            if (textInput) SDL_StartTextInput(app.window); else SDL_StopTextInput(app.window);
        }

        if (game->isExitRequested()) { running = false; continue; }
        if (!game->paused) {
            game->stepFrame();
            if (game->isExitRequested()) { running = false; continue; }
        }
        if (GameCanvas::current) {
            std::vector<uint32_t> frame = GameCanvas::current->shown();
            if (NameDialog::active) drawNameDialog(frame, (uint32_t)SDL_GetTicks());
            app.present(frame);
        }
        if (game->paused) { SDL_Delay(10); continue; }
        int64_t spent = (int64_t)SDL_GetTicks() - (int64_t)frameStart;
        int64_t wait = 30 - spent;
        SDL_Delay((Uint32)(wait < 2 ? 2 : wait));
    }

    game->finish();
    synth.close();
    Audio::backend = nullptr;
    SDL_DestroyTexture(app.texture);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
