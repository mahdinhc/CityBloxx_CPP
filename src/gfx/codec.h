// codec.h - dependency-free zlib inflate and PNG decoder.
#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace cbx {

// Inflates a zlib stream (RFC1950) or raw deflate data. expectedSize==0 means "unknown".
std::vector<uint8_t> zlibInflate(const uint8_t* data, size_t len, size_t expectedSize = 0, bool raw = false);

struct DecodedImage {
    int w = 0, h = 0;
    std::vector<uint32_t> argb;   // non-premultiplied 0xAARRGGBB
};
// Decodes non-interlaced PNG (palette 1/2/4/8 bit, gray, gray+alpha, RGB, RGBA at 8 bit). Throws on error.
DecodedImage decodePng(const uint8_t* data, size_t len);

} // namespace cbx
