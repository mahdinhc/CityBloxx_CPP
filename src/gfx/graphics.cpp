#include "graphics.h"
#include "codec.h"
#include "g3d.h"
#include <cstdlib>

namespace cbx {

// =================================================================================== Font + atlas
struct Glyph { uint8_t adv; int8_t dx, dy; uint8_t w, h; uint32_t off; };
struct Font::Impl {
    int face = 0, style = 0, size = 0;
    int height = 14, ascent = 11, descent = 3;
    std::map<int, Glyph> glyphs;
    bool loaded = false;
};

namespace {
struct AtlasStore {
    std::vector<uint8_t> blob;
    std::vector<std::shared_ptr<Font::Impl>> fonts;
    std::map<int, std::shared_ptr<Font::Impl>> byKey;
} g_atlas;
std::vector<uint8_t>& atlasBlob() { return g_atlas.blob; }
int fontKey(int face, int style, int size) { return (face << 16) | (style << 8) | size; }
}

void Font::loadAtlas(const std::vector<uint8_t>& z) {
    std::vector<uint8_t> raw = zlibInflate(z.data(), z.size());
    auto u16 = [&](size_t o) { return (int)(raw[o] | (raw[o + 1] << 8)); };
    auto u32 = [&](size_t o) { return (uint32_t)(raw[o] | (raw[o + 1] << 8) | (raw[o + 2] << 16) | ((uint32_t)raw[o + 3] << 24)); };
    if (raw.size() < 8 || u32(0) != 0x41464243u) throw std::runtime_error("bad font atlas");
    int n = (int)u32(4);
    size_t p = 8;
    struct Pending { std::shared_ptr<Impl> f; std::vector<std::pair<int, Glyph>> g; };
    std::vector<Pending> pend;
    for (int i = 0; i < n; i++) {
        auto f = std::make_shared<Impl>();
        f->face = raw[p]; f->style = raw[p + 1]; f->size = raw[p + 2];
        f->height = (int16_t)u16(p + 4); f->ascent = (int16_t)u16(p + 6); f->descent = (int16_t)u16(p + 8);
        int ng = u16(p + 10);
        p += 12;
        Pending pd; pd.f = f;
        for (int k = 0; k < ng; k++) {
            Glyph g;
            int cp = u16(p);
            g.adv = raw[p + 2]; g.dx = (int8_t)raw[p + 3]; g.dy = (int8_t)raw[p + 4]; g.w = raw[p + 5]; g.h = raw[p + 6];
            g.off = u32(p + 8);
            p += 12;
            pd.g.emplace_back(cp, g);
        }
        pend.push_back(std::move(pd));
    }
    g_atlas.blob.assign(raw.begin() + (long)p, raw.end());
    g_atlas.fonts.clear(); g_atlas.byKey.clear();
    for (auto& pd : pend) {
        for (auto& kv : pd.g) pd.f->glyphs[kv.first] = kv.second;
        pd.f->loaded = true;
        g_atlas.fonts.push_back(pd.f);
        g_atlas.byKey[fontKey(pd.f->face, pd.f->style, pd.f->size)] = pd.f;
    }
}

Font Font::getFont(int face, int style, int size) {
    Font f;
    auto it = g_atlas.byKey.find(fontKey(face, style, size));
    if (it != g_atlas.byKey.end()) { f.d_ = it->second; return f; }
    // not pre-rendered: fall back to the plain small monospaced face with the right metrics unknown
    auto fb = g_atlas.byKey.find(fontKey(32, 0, 8));
    if (fb != g_atlas.byKey.end()) f.d_ = fb->second;
    else f.d_ = std::make_shared<Impl>();
    return f;
}
int Font::getHeight() const { return impl()->height; }
int Font::getBaselinePosition() const { return impl()->ascent; }
int Font::getDescent() const { return impl()->descent; }
int Font::charWidth(char16_t c) const {
    auto d = impl();
    auto it = d->glyphs.find(c);
    if (it != d->glyphs.end()) return it->second.adv;
    it = d->glyphs.find('?');
    return it != d->glyphs.end() ? it->second.adv : 7;
}
int Font::stringWidth(const String& s) const {
    int w = 0;
    for (char16_t c : s.str()) w += charWidth(c);
    return w;
}
int Font::substringWidth(const String& s, int off, int len) const { return stringWidth(s.substring(off, off + len)); }

// ================================================================================= Image
Image Image::createImage(const Arr<int8_t>& data, int off, int len) {
    if (data == null) throw NullPointerException();
    if (off < 0 || len < 0 || off + len > data.length) throw ArrayIndexOutOfBoundsException(off, data.length);
    DecodedImage di;
    try { di = decodePng((const uint8_t*)data.data() + off, (size_t)len); }
    catch (const std::exception& e) { throw IOException(e.what()); }
    Image im; im.d_ = std::make_shared<Impl>();
    im.d_->w = di.w; im.d_->h = di.h; im.d_->argb = std::move(di.argb);
    return im;
}
Image Image::createRGBImage(const Arr<int>& rgb, int w, int h, bool alpha) {
    Image im; im.d_ = std::make_shared<Impl>();
    im.d_->w = w; im.d_->h = h; im.d_->argb.assign((size_t)w * (size_t)h, 0);
    for (int i = 0; i < w * h && i < rgb.length; i++) {
        uint32_t v = (uint32_t)rgb[i];
        im.d_->argb[(size_t)i] = alpha ? v : (v | 0xFF000000u);
    }
    return im;
}

// ============================================================================== Graphics
struct Graphics::Impl {
    std::shared_ptr<Surface> s;
    Font font;
    uint32_t color = 0xFF000000u;
    int clipX = 0, clipY = 0, clipW = 0, clipH = 0;   // as set by the game
    bool dotted = false;
    // effective clip rectangle [ex0,ex1) x [ey0,ey1)
    int ex0 = 0, ey0 = 0, ex1 = 0, ey1 = 0;
    void updateEff() {
        ex0 = std::max(0, clipX); ey0 = std::max(0, clipY);
        ex1 = std::min(s->w, clipX + std::max(0, clipW)); ey1 = std::min(s->h, clipY + std::max(0, clipH));
    }
};

Graphics::Graphics(std::shared_ptr<Surface> target) {
    d_ = std::make_shared<Impl>();
    d_->s = std::move(target);
    d_->font = Font::getFont(0, 0, 0);
    d_->clipX = 0; d_->clipY = 0; d_->clipW = d_->s->w; d_->clipH = d_->s->h;
    d_->updateEff();
}
std::shared_ptr<Surface> Graphics::surface() const { return I().s; }

// Any 2D call on a target first flushes pending 3D output so draw order is preserved.
static void sync(const Graphics& g) {
    if (Graphics3D::pendingId() == g.id()) Graphics3D::flushPending();
}

void Graphics::setColor(int rgb) { I().color = 0xFF000000u | ((uint32_t)rgb & 0xFFFFFFu); }
void Graphics::setColor(int r, int g, int b) { I().color = 0xFF000000u | ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255); }
void Graphics::setFont(const Font& f) { if (f != null) I().font = f; }
Font Graphics::getFont() const { return I().font; }
void Graphics::setClip(int x, int y, int w, int h) {
    sync(*this);
    auto& i = I();
    i.clipX = x; i.clipY = y; i.clipW = w; i.clipH = h;
    i.updateEff();
}
int Graphics::getClipX() const { return I().clipX; }
int Graphics::getClipY() const { return I().clipY; }
int Graphics::getClipWidth() const { return I().clipW; }
int Graphics::getClipHeight() const { return I().clipH; }
void Graphics::setStrokeStyle(int style) { I().dotted = (style == DOTTED); }

static inline void put(Graphics::Impl& i, int x, int y) {
    if (x >= i.ex0 && x < i.ex1 && y >= i.ey0 && y < i.ey1) i.s->px[(size_t)y * (size_t)i.s->w + (size_t)x] = i.color;
}

void Graphics::fillRect(int x, int y, int w, int h) {
    sync(*this);
    if (w <= 0 || h <= 0) return;
    auto& i = I();
    int x0 = std::max(x, i.ex0), y0 = std::max(y, i.ey0);
    int64_t xe = std::min<int64_t>((int64_t)x + w, i.ex1), ye = std::min<int64_t>((int64_t)y + h, i.ey1);
    for (int yy = y0; yy < ye; yy++)
        for (int xx = x0; xx < xe; xx++) i.s->px[(size_t)yy * (size_t)i.s->w + (size_t)xx] = i.color;
}

static void solidLine(Graphics::Impl& i, int x1, int y1, int x2, int y2) {
    int dx = x2 - x1, dy = y2 - y1, ax = std::abs(dx), ay = std::abs(dy);
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    int x = x1, y = y1;
    if (ax >= ay) {
        int e = -ax;
        for (int k = 0; k <= ax; k++) {
            put(i, x, y);
            e += ay * 2;
            if (e >= 0) { e -= ax * 2; if (dy) y += sy; }
            x += sx;
        }
    } else {
        int e = -ay;
        for (int k = 0; k <= ay; k++) {
            put(i, x, y);
            e += ax * 2;
            if (e >= 0) { e -= ay * 2; x += sx; }
            y += sy;
        }
    }
}

void Graphics::drawLine(int x1, int y1, int x2, int y2) {
    sync(*this);
    auto& i = I();
    if (i.dotted) {
        // Java2D strokes a [1 on, 1 off] dash pattern through the shape pipeline; for axis-aligned lines the
        // covered pixels are (measured against the JDK): dash starts at the first point, offset -1 across.
        if (y1 == y2 && x1 != x2) {
            int len = std::abs(x2 - x1), dir = x2 > x1 ? 1 : -1;
            for (int t = 0; t < len; t += 2) put(i, dir > 0 ? x1 + t : x1 - t - 1, y1 - 1);
            return;
        }
        if (x1 == x2 && y1 != y2) {
            int len = std::abs(y2 - y1), dir = y2 > y1 ? 1 : -1;
            for (int t = 0; t < len; t += 2) put(i, x1 - 1, dir > 0 ? y1 + t : y1 - t - 1);
            return;
        }
        if (x1 == x2 && y1 == y2) return;
        // diagonal dotted lines are never drawn by the game
    }
    solidLine(i, x1, y1, x2, y2);
    if (x1 == x2 && y1 == y2) put(i, x1, y1);
}

void Graphics::drawRect(int x, int y, int w, int h) {
    sync(*this);
    if (w < 0 || h < 0) return;
    auto& i = I();
    // thin (non-dashed) rectangle outline, (w+1) x (h+1) pixels
    for (int xx = x; xx <= x + w; xx++) { put(i, xx, y); put(i, xx, y + h); }
    for (int yy = y + 1; yy < y + h; yy++) { put(i, x, yy); put(i, x + w, yy); }
}

void Graphics::drawString(const String& s, int x, int y, int anchor) {
    if (s == null || s.length() == 0) return;
    sync(*this);
    auto& i = I();
    auto fd = i.font.impl();
    int sw = i.font.stringWidth(s);
    int px = x;
    if ((anchor & HCENTER) != 0) px = x - sw / 2;
    else if ((anchor & RIGHT) != 0) px = x - sw;
    int py;
    if ((anchor & BASELINE) != 0) py = y;
    else if ((anchor & BOTTOM) != 0) py = y - fd->descent;
    else py = y + fd->ascent;
    int pen = px;
    const auto& blob = atlasBlob();
    for (char16_t c : s.str()) {
        auto it = fd->glyphs.find(c);
        if (it == fd->glyphs.end()) it = fd->glyphs.find('?');
        if (it == fd->glyphs.end()) { pen += 7; continue; }
        const Glyph& g = it->second;
        for (int gy = 0; gy < g.h; gy++) {
            int yy = py + g.dy + gy;
            if (yy < i.ey0 || yy >= i.ey1) continue;
            for (int gx = 0; gx < g.w; gx++) {
                int xx = pen + g.dx + gx;
                if (xx < i.ex0 || xx >= i.ex1) continue;
                int a = blob[g.off + (size_t)gy * g.w + (size_t)gx];
                if (a == 0) continue;
                uint32_t& d = i.s->px[(size_t)yy * (size_t)i.s->w + (size_t)xx];
                d = blendOver(((uint32_t)a << 24) | (i.color & 0xFFFFFFu), d);
            }
        }
        pen += g.adv;
    }
}

void Graphics::drawImage(const Image& im, int x, int y, int anchor) { drawImageImpl(im, x, y, anchor, false); }
void Graphics::drawImageFlipped(const Image& im, int x, int y, int anchor) { drawImageImpl(im, x, y, anchor, true); }

void Graphics::drawImageImpl(const Image& im, int x, int y, int anchor, bool flip) {
    if (im == null) return;
    sync(*this);
    auto& i = I();
    auto d = im.impl();
    int iw = d->w, ih = d->h;
    int px = x, py = y;
    if ((anchor & HCENTER) != 0) px = x - iw / 2; else if ((anchor & RIGHT) != 0) px = x - iw;
    if ((anchor & VCENTER) != 0) py = y - ih / 2; else if ((anchor & BOTTOM) != 0) py = y - ih;
    int x0 = std::max(px, i.ex0), x1 = std::min(px + iw, i.ex1);
    int y0 = std::max(py, i.ey0), y1 = std::min(py + ih, i.ey1);
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            int sx = flip ? iw - 1 - (xx - px) : xx - px;
            uint32_t sp = d->argb[(size_t)(yy - py) * (size_t)iw + (size_t)sx];
            uint32_t& dp = i.s->px[(size_t)yy * (size_t)i.s->w + (size_t)xx];
            dp = blendOver(sp, dp);
        }
    }
}

void Graphics::compositeArgb(const uint32_t* src, int stride, int x0, int y0, int x1, int y1) {
    auto& i = I();
    x0 = std::max(x0, i.ex0); y0 = std::max(y0, i.ey0);
    x1 = std::min(x1, i.ex1); y1 = std::min(y1, i.ey1);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            uint32_t& dp = i.s->px[(size_t)y * (size_t)i.s->w + (size_t)x];
            dp = blendOver(src[(size_t)y * (size_t)stride + (size_t)x], dp);
        }
}

} // namespace cbx
