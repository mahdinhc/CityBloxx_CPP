#include "name_dialog_view.h"
#include "../platform/jme.h"

namespace cbx {

void drawNameDialog(std::vector<uint32_t>& frame, int64_t ms) {
    if (!NameDialog::active) return;
    // dim the game
    for (auto& p : frame) {
        uint32_t r = ((p >> 16) & 255) / 3, g = ((p >> 8) & 255) / 3, b = (p & 255) / 3;
        p = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    auto surf = std::make_shared<Surface>(SCREEN_W, SCREEN_H);
    surf->px = frame;
    Graphics g(surf);
    Font bold = Font::getFont(32, 1, 0), plain = Font::getFont(32, 0, 8), smallBold = Font::getFont(32, 1, 8);
    const int bx = 20, by = 100, bw = 200, bh = 112;
    g.setColor(0x7A1A1A); g.fillRect(bx - 2, by - 2, bw + 4, bh + 4);
    g.setColor(0xFFFFFF); g.fillRect(bx, by, bw, bh);
    g.setFont(bold); g.setColor(0x7A0000);
    g.drawString(NameDialog::title, bx + bw / 2, by + 10, Graphics::HCENTER | Graphics::TOP);
    // text field
    const int fx = bx + 16, fy = by + 42, fw = bw - 32, fh = 22;
    g.setColor(0x9AB4D0); g.fillRect(fx - 1, fy - 1, fw + 2, fh + 2);
    g.setColor(0xF4F8FC); g.fillRect(fx, fy, fw, fh);
    g.setFont(plain); g.setColor(0x000000);
    String text = NameDialog::field != null ? NameDialog::field.getString() : String(u"");
    g.drawString(text, fx + 4, fy + 4, Graphics::LEFT | Graphics::TOP);
    if (((ms / 500) & 1) == 0) {
        int cx = fx + 4 + plain.stringWidth(text);
        g.fillRect(cx + 1, fy + 3, 1, plain.getHeight());
    }
    // buttons: label of the first command (OK)
    String label = u"OK"_s;
    if (NameDialog::commands != null && NameDialog::commands.length > 0 && NameDialog::commands[0] != null) label = NameDialog::commands[0].getLabel();
    const int ow = std::max(60, smallBold.stringWidth(label) + 20), oh = 22, ox = bx + (bw - ow) / 2, oy = by + bh - oh - 10;
    g.setColor(0xFFD84A); g.fillRect(ox, oy, ow, oh);
    g.setColor(0x7A1A1A); g.drawRect(ox, oy, ow - 1, oh - 1);
    g.setFont(smallBold); g.setColor(0x7A0000);
    g.drawString(label, ox + ow / 2, oy + 5, Graphics::HCENTER | Graphics::TOP);
    frame = surf->px;
}

} // namespace cbx
