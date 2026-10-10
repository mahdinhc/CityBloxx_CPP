// game_base.h - everything the translated game classes need (included by every generated header).
#pragma once
#include "../jcompat/jcompat.h"
#include "../gfx/graphics.h"
#include "../gfx/g3d.h"
#include "../platform/jme.h"

namespace cbx {

class GameMIDlet; class House; class MenuController; class HighScores; class SoundPlayer; class Vibra;
class CityMode; class Ui; class Storage; class Resources; class Lang;

// Opens a data file shipped with the game as a DataInputStream (null if it does not exist).
DataInputStream openAssetStream(const String& name);

} // namespace cbx
