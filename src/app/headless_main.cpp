// cbx_headless - deterministic, window-less runner used for regression tests against the original Java game.
// Script commands: dt N | step N | key C... | dump NAME | namesubmit TEXT
#include "bootstrap.h"
#include "png_writer.h"
#include "name_dialog_view.h"
#include "../game/classes.h"
#include "../platform/canvas.h"
#include "../jcompat/clock.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace cbx;

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: cbx_headless <script> <outdir> <assets> [saves]\n"); return 2; }
    Options o;
    o.assets = argv[3];
    o.saves = argc > 4 ? argv[4] : std::string(argv[2]) + "/saves";
    o.locale = "en-US"; o.seed = 12345; o.virtualClock = true;
    std::string outDir = argv[2];
    House* game = nullptr;
    try { game = Bootstrap::create(o); }
    catch (const std::exception& e) { std::fprintf(stderr, "startup failed: %s\n", e.what()); return 1; }
    std::ofstream trace(outDir + "/trace.txt");
    std::ifstream in(argv[1]);
    std::string ln; long dt = 33; int frame = 0;
    while (std::getline(in, ln)) {
        std::istringstream ss(ln); std::string cmd;
        if (!(ss >> cmd) || cmd[0] == '#') continue;
        if (cmd == "dt") ss >> dt;
        else if (cmd == "step") {
            int n; ss >> n;
            for (int i = 0; i < n; i++) {
                Clock::advance(dt);
                game->stepFrame();
                trace << frame << " " << (GameCanvas::current ? frameCrc(GameCanvas::current->shown()) : 0u) << "\n";
                frame++;
            }
        } else if (cmd == "key") {
            int c; while (ss >> c) { GameCanvas::current->keyDown(c, false); GameCanvas::current->keyUp(c); }
        } else if (cmd == "dump") {
            std::string name; ss >> name;
            writePng(outDir + "/" + name + ".png", GameCanvas::current->shown(), 240, 320);
        } else if (cmd == "dumpoverlay") {
            std::string name; ss >> name;
            std::vector<uint32_t> f = GameCanvas::current->shown();
            drawNameDialog(f, 0);
            writePng(outDir + "/" + name + ".png", f, 240, 320);
        } else if (cmd == "opendialog") {   // debugging aid: open the name prompt without playing to a high score
            TextField tf(u""_s, u"Player"_s, 10, 0);
            Arr<Command> cmds = Arr<Command>({Command(u"OK"_s, Command::OK, 1)});
            NameDialog::open(u"Enter your name"_s, tf, cmds, [](Command) {});
        } else if (cmd == "namesubmit") {
            std::string t; ss >> t;
            if (NameDialog::active) { NameDialog::field.setString(String::fromUtf8(t.empty() ? "AAA" : t)); NameDialog::submit(); }
            else std::fprintf(stderr, "no form open\n");
        } else std::fprintf(stderr, "bad cmd %s\n", ln.c_str());
    }
    return 0;
}
