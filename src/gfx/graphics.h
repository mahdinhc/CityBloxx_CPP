// graphics.h - software 2D graphics with the exact pixel semantics of the original Java2D-backed layer.
//
// Font / Image / Graphics are value-type handles (shared state inside) so that the translated game code,
// which was written against reference types, compiles unchanged. All blending uses the integer math
// that was verified bit-exact against the JDK: mul8(a,b) = (a*b+127)/255.
#pragma once
#include "../jcompat/jcompat.h"
#include <memory>
#include <vector>
#include <map>

namespace cbx {

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;

inline int mul8(int a, int b) { return (a * b + 127) / 255; }
// source-over of a non-premultiplied ARGB pixel onto an opaque destination
inline uint32_t blendOver(uint32_t src, uint32_t dst) {
    int a = (int)(src >> 24);
    if (a == 0) return dst;
    if (a == 255) return 0xFF000000u | (src & 0xFFFFFFu);
    int ia = 255 - a;
    int r = mul8(a, (src >> 16) & 255) + mul8(ia, (dst >> 16) & 255);
    int g = mul8(a, (src >> 8) & 255) + mul8(ia, (dst >> 8) & 255);
    int b = mul8(a, src & 255) + mul8(ia, dst & 255);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// ------------------------------------------------------------------------------------------ Surface
struct Surface {
    int w, h;
    std::vector<uint32_t> px;   // 0xFFRRGGBB
    Surface(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * (size_t)h_, 0xFF000000u) {}
};

// -------------------------------------------------------------------------------------------- Font
struct GlyphAtlas;
class Font {
public:
    static const int STYLE_PLAIN = 0, STYLE_BOLD = 1, STYLE_ITALIC = 2;
    static const int SIZE_MEDIUM = 0, SIZE_SMALL = 8, SIZE_LARGE = 16;

    Font() = default;
    Font(NullT) {}
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }

    static Font getFont(int face, int style, int size);
    // Loads the pre-rendered glyph atlas (zlib-compressed assets/fontatlas.bin contents).
    static void loadAtlas(const std::vector<uint8_t>& zlibData);

    int getHeight() const;
    int getBaselinePosition() const;      // ascent
    int getDescent() const;
    int stringWidth(const String& s) const;
    int substringWidth(const String& s, int off, int len) const;
    int charWidth(char16_t c) const;

    struct Impl;
    std::shared_ptr<Impl> impl() const { if (!d_) throw NullPointerException(); return d_; }
private:
    std::shared_ptr<Impl> d_;
    friend class Graphics;
};

// -------------------------------------------------------------------------------------------- Image
class Graphics;
class Image {
public:
    Image() = default;
    Image(NullT) {}
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }

    static Image createImage(const Arr<int8_t>& data, int off, int len);   // decodes PNG
    static Image createRGBImage(const Arr<int>& rgb, int w, int h, bool alpha);

    int getWidth() const { return impl()->w; }
    int getHeight() const { return impl()->h; }

    struct Impl { int w = 0, h = 0; std::vector<uint32_t> argb; };
    std::shared_ptr<Impl> impl() const { if (!d_) throw NullPointerException(); return d_; }
private:
    std::shared_ptr<Impl> d_;
};

// ----------------------------------------------------------------------------------------- Graphics
class Graphics {
public:
    static const int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64;
    static const int SOLID = 0, DOTTED = 1;

    Graphics() = default;
    explicit Graphics(std::shared_ptr<Surface> target);
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }

    void setColor(int rgb);
    void setColor(int r, int g, int b);
    void setFont(const Font& f);
    Font getFont() const;
    void setClip(int x, int y, int w, int h);
    int getClipX() const; int getClipY() const; int getClipWidth() const; int getClipHeight() const;
    void setStrokeStyle(int style);
    void fillRect(int x, int y, int w, int h);
    void drawRect(int x, int y, int w, int h);
    void drawLine(int x1, int y1, int x2, int y2);
    void drawString(const String& s, int x, int y, int anchor);
    void drawImage(const Image& im, int x, int y, int anchor);
    void drawImageFlipped(const Image& im, int x, int y, int anchor);

    // Composite a block of ARGB pixels (used by the 3D rasteriser); honours the current clip.
    void compositeArgb(const uint32_t* src, int srcStride, int x0, int y0, int x1, int y1);
    std::shared_ptr<Surface> surface() const;
    const void* id() const { return d_.get(); }

    struct Impl;
private:
    std::shared_ptr<Impl> d_;
    Impl& I() const { if (!d_) throw NullPointerException(); return *d_; }
    void drawImageImpl(const Image& im, int x, int y, int anchor, bool flip);
};

} // namespace cbx
