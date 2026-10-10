#include "codec.h"
#include <cstring>
#include <string>

namespace cbx {

namespace {

struct BitReader {
    const uint8_t* d; size_t n; size_t pos = 0; uint32_t bitbuf = 0; int bitcnt = 0;
    int bit() {
        if (bitcnt == 0) {
            if (pos >= n) throw std::runtime_error("inflate: out of data");
            bitbuf = d[pos++]; bitcnt = 8;
        }
        int b = bitbuf & 1; bitbuf >>= 1; bitcnt--; return b;
    }
    int bits(int k) { int v = 0; for (int i = 0; i < k; i++) v |= bit() << i; return v; }
    void alignByte() { bitcnt = 0; }
};

struct Huffman {
    uint16_t count[16]; uint16_t symbol[320];
    void build(const uint8_t* lengths, int n) {
        std::memset(count, 0, sizeof(count));
        for (int i = 0; i < n; i++) count[lengths[i]]++;
        count[0] = 0;
        uint16_t offs[16]; offs[1] = 0;
        for (int i = 1; i < 15; i++) offs[i + 1] = (uint16_t)(offs[i] + count[i]);
        for (int i = 0; i < n; i++) if (lengths[i]) symbol[offs[lengths[i]]++] = (uint16_t)i;
    }
    int decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; len++) {
            code |= br.bit();
            int c = count[len];
            if (code - c < first) return symbol[index + (code - first)];
            index += c; first += c; first <<= 1; code <<= 1;
        }
        throw std::runtime_error("inflate: bad code");
    }
};

const uint16_t kLenBase[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const uint16_t kLenExtra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const uint16_t kDistBase[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const uint16_t kDistExtra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

void inflateBlock(BitReader& br, std::vector<uint8_t>& out, const Huffman& lit, const Huffman& dist) {
    for (;;) {
        int sym = lit.decode(br);
        if (sym < 256) out.push_back((uint8_t)sym);
        else if (sym == 256) return;
        else {
            sym -= 257;
            if (sym >= 29) throw std::runtime_error("inflate: bad length");
            int len = kLenBase[sym] + br.bits(kLenExtra[sym]);
            int ds = dist.decode(br);
            if (ds >= 30) throw std::runtime_error("inflate: bad dist");
            size_t d = (size_t)kDistBase[ds] + (size_t)br.bits(kDistExtra[ds]);
            if (d > out.size()) throw std::runtime_error("inflate: dist too far");
            size_t from = out.size() - d;
            for (int i = 0; i < len; i++) out.push_back(out[from + (size_t)i]);
        }
    }
}

} // namespace

std::vector<uint8_t> zlibInflate(const uint8_t* data, size_t len, size_t expected, bool raw) {
    BitReader br{data, len};
    if (!raw) {
        if (len < 2 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0)
            throw std::runtime_error("zlib: bad header");
        br.pos = 2;
    }
    std::vector<uint8_t> out;
    if (expected) out.reserve(expected);
    int last;
    do {
        last = br.bit();
        int type = br.bits(2);
        if (type == 0) {
            br.alignByte();
            if (br.pos + 4 > len) throw std::runtime_error("inflate: stored");
            int n = data[br.pos] | (data[br.pos + 1] << 8);
            br.pos += 4;
            if (br.pos + (size_t)n > len) throw std::runtime_error("inflate: stored len");
            out.insert(out.end(), data + br.pos, data + br.pos + n);
            br.pos += (size_t)n;
        } else if (type == 1) {
            static Huffman lit, dist; static bool init = false;
            if (!init) {
                uint8_t l[288];
                int i = 0;
                for (; i < 144; i++) l[i] = 8;
                for (; i < 256; i++) l[i] = 9;
                for (; i < 280; i++) l[i] = 7;
                for (; i < 288; i++) l[i] = 8;
                lit.build(l, 288);
                uint8_t dl[30]; for (int k = 0; k < 30; k++) dl[k] = 5;
                dist.build(dl, 30);
                init = true;
            }
            inflateBlock(br, out, lit, dist);
        } else if (type == 2) {
            int nlen = br.bits(5) + 257, ndist = br.bits(5) + 1, ncode = br.bits(4) + 4;
            static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            uint8_t lengths[320] = {0};
            for (int i = 0; i < ncode; i++) lengths[order[i]] = (uint8_t)br.bits(3);
            Huffman lencode; lencode.build(lengths, 19);
            int idx = 0;
            uint8_t ll[320] = {0};
            while (idx < nlen + ndist) {
                int sym = lencode.decode(br);
                if (sym < 16) ll[idx++] = (uint8_t)sym;
                else {
                    int prev = 0, rep;
                    if (sym == 16) { if (idx == 0) throw std::runtime_error("inflate: no prev"); prev = ll[idx - 1]; rep = 3 + br.bits(2); }
                    else if (sym == 17) rep = 3 + br.bits(3);
                    else rep = 11 + br.bits(7);
                    if (idx + rep > nlen + ndist) throw std::runtime_error("inflate: repeat");
                    while (rep--) ll[idx++] = (uint8_t)prev;
                }
            }
            Huffman lit, dist;
            lit.build(ll, nlen);
            dist.build(ll + nlen, ndist);
            inflateBlock(br, out, lit, dist);
        } else throw std::runtime_error("inflate: bad block type");
    } while (!last);
    return out;
}

// ------------------------------------------------------------------------------------------- PNG
static uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

DecodedImage decodePng(const uint8_t* d, size_t n) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n < 8 || std::memcmp(d, sig, 8) != 0) throw std::runtime_error("png: bad signature");
    size_t pos = 8;
    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    std::vector<uint8_t> idat, plte, trns;
    while (pos + 8 <= n) {
        uint32_t clen = be32(d + pos);
        std::string type((const char*)d + pos + 4, 4);
        const uint8_t* body = d + pos + 8;
        if (pos + 12 + clen > n) throw std::runtime_error("png: truncated");
        if (type == "IHDR") { w = (int)be32(body); h = (int)be32(body + 4); depth = body[8]; ctype = body[9]; interlace = body[12]; }
        else if (type == "PLTE") plte.assign(body, body + clen);
        else if (type == "tRNS") trns.assign(body, body + clen);
        else if (type == "IDAT") idat.insert(idat.end(), body, body + clen);
        else if (type == "IEND") break;
        pos += 12 + clen;
    }
    if (w <= 0 || h <= 0 || interlace != 0) throw std::runtime_error("png: unsupported");
    int channels = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 3 ? 1 : ctype == 4 ? 2 : 4;
    if (depth != 8 && ctype != 3 && !(ctype == 0 && depth < 8)) throw std::runtime_error("png: unsupported depth");
    int bpp = std::max(1, channels * depth / 8);
    size_t stride = ((size_t)w * channels * depth + 7) / 8;
    std::vector<uint8_t> raw = zlibInflate(idat.data(), idat.size(), (stride + 1) * (size_t)h);
    if (raw.size() < (stride + 1) * (size_t)h) throw std::runtime_error("png: short data");
    std::vector<uint8_t> img(stride * (size_t)h);
    for (int y = 0; y < h; y++) {
        int f = raw[(stride + 1) * (size_t)y];
        const uint8_t* src = &raw[(stride + 1) * (size_t)y + 1];
        uint8_t* cur = &img[stride * (size_t)y];
        const uint8_t* up = y ? &img[stride * (size_t)(y - 1)] : nullptr;
        for (size_t i = 0; i < stride; i++) {
            int a = i >= (size_t)bpp ? cur[i - (size_t)bpp] : 0;
            int b = up ? up[i] : 0;
            int c = (up && i >= (size_t)bpp) ? up[i - (size_t)bpp] : 0;
            int v = src[i];
            switch (f) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: { int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                          v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
                default: throw std::runtime_error("png: bad filter");
            }
            cur[i] = (uint8_t)v;
        }
    }
    DecodedImage out; out.w = w; out.h = h; out.argb.resize((size_t)w * (size_t)h);
    for (int y = 0; y < h; y++) {
        const uint8_t* row = &img[stride * (size_t)y];
        for (int x = 0; x < w; x++) {
            uint32_t a = 255, r, g, b;
            if (ctype == 3 || (ctype == 0 && depth < 8)) {
                int per = 8 / depth, shift = 8 - depth * (x % per + 1), mask = (1 << depth) - 1;
                int idx = (row[x / per] >> shift) & mask;
                if (ctype == 3) {
                    if ((size_t)idx * 3 + 2 >= plte.size()) throw std::runtime_error("png: bad palette index");
                    r = plte[(size_t)idx * 3]; g = plte[(size_t)idx * 3 + 1]; b = plte[(size_t)idx * 3 + 2];
                    if ((size_t)idx < trns.size()) a = trns[(size_t)idx];
                } else { r = g = b = (uint32_t)(idx * 255 / mask); }
            } else if (ctype == 0) { r = g = b = row[x]; }
            else if (ctype == 2) { r = row[x * 3]; g = row[x * 3 + 1]; b = row[x * 3 + 2]; }
            else if (ctype == 4) { r = g = b = row[x * 2]; a = row[x * 2 + 1]; }
            else { r = row[x * 4]; g = row[x * 4 + 1]; b = row[x * 4 + 2]; a = row[x * 4 + 3]; }
            out.argb[(size_t)y * (size_t)w + (size_t)x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return out;
}

} // namespace cbx
