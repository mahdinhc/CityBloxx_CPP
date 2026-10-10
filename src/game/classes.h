// classes.h - umbrella header: all game classes in dependency order.
#pragma once
#include "game_base.h"
#include "vibra.h"
#include "soundplayer.h"
#include "gen/Lang.h"
#include "gen/Storage.h"
#include "gen/Resources.h"
#include "gen/GameMIDlet.h"
#include "gen/Ui.h"
#include "gen/MenuController.h"
#include "gen/HighScores.h"
#include "gen/CityMode.h"
#include "gen/House.h"

namespace cbx {
struct PlatformFactory {
    static ICanvas* createCanvas();
    static Vibra* createVibra() { return new Vibra(); }
    static SoundPlayer* createSoundPlayer() { return new SoundPlayer(); }
    static HighScores* createHighScores() { return new HighScores(); }
};
}
