// png_writer.h - minimal PNG encoder (stored deflate blocks) for screenshots and test dumps.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace cbx {
inline uint32_t crc32Update(uint32_t crc, const uint8_t* p, size_t n) {
    static uint32_t table[256]; static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 255] ^ (crc >> 8);
    return ~crc;
}
// crc32 over the RGB bytes of 0xAARRGGBB pixels (matches the Java harness)
inline uint32_t frameCrc(const std::vector<uint32_t>& px) {
    std::vector<uint8_t> b(px.size() * 3);
    for (size_t i = 0; i < px.size(); i++) { b[i * 3] = (uint8_t)(px[i] >> 16); b[i * 3 + 1] = (uint8_t)(px[i] >> 8); b[i * 3 + 2] = (uint8_t)px[i]; }
    return crc32Update(0, b.data(), b.size());
}
inline bool writePng(const std::string& path, const std::vector<uint32_t>& px, int w, int h) {
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) { uint32_t p = px[(size_t)y * (size_t)w + (size_t)x]; raw.push_back((uint8_t)(p >> 16)); raw.push_back((uint8_t)(p >> 8)); raw.push_back((uint8_t)p); }
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back((uint8_t)n); z.push_back((uint8_t)(n >> 8)); z.push_back((uint8_t)~n); z.push_back((uint8_t)(~n >> 8));
        z.insert(z.end(), raw.begin() + (long)pos, raw.begin() + (long)(pos + n)); pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    uint32_t ad = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) z.push_back((uint8_t)(ad >> s));
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto chunk = [&](const char* type, const std::vector<uint8_t>& d) {
        uint8_t len[4] = {(uint8_t)(d.size() >> 24), (uint8_t)(d.size() >> 16), (uint8_t)(d.size() >> 8), (uint8_t)d.size()};
        std::fwrite(len, 1, 4, f);
        std::vector<uint8_t> td(type, type + 4); td.insert(td.end(), d.begin(), d.end());
        std::fwrite(td.data(), 1, td.size(), f);
        uint32_t c = crc32Update(0, td.data(), td.size());
        uint8_t cb[4] = {(uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
        std::fwrite(cb, 1, 4, f);
    };
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w, (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 2, 0, 0, 0};
    chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", {});
    std::fclose(f);
    return true;
}
}
