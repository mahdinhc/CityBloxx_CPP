// name_dialog_view.h - draws the high-score name prompt (replacement for the original Swing dialog)
// on top of the game frame, using the game's own bitmap font.
#pragma once
#include "../gfx/graphics.h"

namespace cbx {
// 'ms' drives the caret blink. Draws only while NameDialog::active.
void drawNameDialog(std::vector<uint32_t>& frame240x320, int64_t ms);
}
