#include "jcompat.h"
#include "clock.h"

namespace cbx {

// ---- UTF conversion helpers --------------------------------------------------------------------
String String::fromUtf8(const std::string& s) {
    std::u16string out;
    size_t i = 0;
    while (i < s.size()) {
        uint32_t c = (uint8_t)s[i];
        int extra = 0;
        if (c >= 0xF0) { c &= 0x07; extra = 3; }
        else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
        else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
        i++;
        while (extra-- > 0 && i < s.size()) c = (c << 6) | ((uint8_t)s[i++] & 0x3F);
        if (c >= 0x10000) { c -= 0x10000; out.push_back((char16_t)(0xD800 + (c >> 10))); out.push_back((char16_t)(0xDC00 + (c & 0x3FF))); }
        else out.push_back((char16_t)c);
    }
    return String(std::move(out));
}

std::string String::toUtf8() const {
    if (!p_) return "null";
    std::string out;
    const auto& s = *p_;
    for (size_t i = 0; i < s.size(); i++) {
        uint32_t c = s[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size()) { c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00); }
        if (c < 0x80) out.push_back((char)c);
        else if (c < 0x800) { out.push_back((char)(0xC0 | (c >> 6))); out.push_back((char)(0x80 | (c & 0x3F))); }
        else if (c < 0x10000) { out.push_back((char)(0xE0 | (c >> 12))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
        else { out.push_back((char)(0xF0 | (c >> 18))); out.push_back((char)(0x80 | ((c >> 12) & 0x3F))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
    }
    return out;
}

int String::compareTo(const String& o) const {
    const auto& a = str(); const auto& b = o.str();
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    return (int)a.size() - (int)b.size();
}

String String::trim() const {
    const auto& s = str();
    size_t b = 0, e = s.size();
    while (b < e && s[b] <= u' ') b++;
    while (e > b && s[e - 1] <= u' ') e--;
    return String(s.substr(b, e - b));
}

String String::valueOf(int v) { return fromUtf8(std::to_string(v)); }
String String::valueOf(int64_t v) { return fromUtf8(std::to_string(v)); }

String operator+(const String& a, const String& b) {
    std::u16string r = a.isNull() ? u"null" : a.str();
    r += b.isNull() ? std::u16string(u"null") : b.str();
    return String(std::move(r));
}

int Integer::parseInt(const String& s) {
    if (s.isNull()) throw NumberFormatException();
    const auto& t = s.str();
    if (t.empty()) throw NumberFormatException();
    size_t i = 0; bool neg = false;
    if (t[0] == u'-' || t[0] == u'+') { neg = t[0] == u'-'; i = 1; }
    if (i >= t.size()) throw NumberFormatException();
    int64_t v = 0;
    for (; i < t.size(); i++) {
        if (t[i] < u'0' || t[i] > u'9') throw NumberFormatException();
        v = v * 10 + (t[i] - u'0');
        if (v > 2147483648LL) throw NumberFormatException();
    }
    if (neg) v = -v;
    if (v > INT32_MAX || v < INT32_MIN) throw NumberFormatException();
    return (int)v;
}

int64_t System::currentTimeMillis() { return Clock::nowMs(); }
void System::Out::println(const String& s) const { std::fprintf(stderr, "%s\n", s.toUtf8().c_str()); }
void System::Out::println(const char* s) const { std::fprintf(stderr, "%s\n", s); }
void System::println(const String& s) { std::fprintf(stderr, "%s\n", s.toUtf8().c_str()); }
void System::println(const char* s) { std::fprintf(stderr, "%s\n", s); }

// ---- modified UTF-8 (DataInput/DataOutput) ------------------------------------------------------
String DataInputStream::readUTF() {
    int len = readUnsignedShort();
    need((size_t)len);
    std::u16string out;
    size_t end = *pos_ + (size_t)len;
    auto& b = *b_;
    while (*pos_ < end) {
        int c = b[(*pos_)++];
        if (c < 0x80) out.push_back((char16_t)c);
        else if ((c >> 5) == 6) {
            if (*pos_ >= end) throw IOException("UTFDataFormatException");
            int c2 = b[(*pos_)++];
            if ((c2 & 0xC0) != 0x80) throw IOException("UTFDataFormatException");
            out.push_back((char16_t)(((c & 0x1F) << 6) | (c2 & 0x3F)));
        } else if ((c >> 4) == 14) {
            if (*pos_ + 1 >= end + 0 && *pos_ + 2 > end) throw IOException("UTFDataFormatException");
            int c2 = b[(*pos_)++], c3 = b[(*pos_)++];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) throw IOException("UTFDataFormatException");
            out.push_back((char16_t)(((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F)));
        } else throw IOException("UTFDataFormatException");
    }
    return String(std::move(out));
}

void DataOutputStream::writeUTF(const String& s) {
    chk();
    std::vector<uint8_t> enc;
    for (char16_t c : s.str()) {
        if (c >= 1 && c <= 0x7F) enc.push_back((uint8_t)c);
        else if (c <= 0x7FF) { enc.push_back((uint8_t)(0xC0 | (c >> 6))); enc.push_back((uint8_t)(0x80 | (c & 0x3F))); }
        else { enc.push_back((uint8_t)(0xE0 | (c >> 12))); enc.push_back((uint8_t)(0x80 | ((c >> 6) & 0x3F))); enc.push_back((uint8_t)(0x80 | (c & 0x3F))); }
    }
    if (enc.size() > 65535) throw IOException("UTFDataFormatException");
    writeShort((int)enc.size());
    b_->insert(b_->end(), enc.begin(), enc.end());
}

} // namespace cbx
