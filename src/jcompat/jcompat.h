// jcompat.h - tiny Java-semantics runtime used by the translated game logic.
//
// The original (decompiled J2ME) game logic relies on Java behaviours plain C++ does not give for free:
// bounds-checked arrays, null-able reference arrays/strings, UTF-16 strings, wrapping 32-bit arithmetic
// and exceptions that the game catches and ignores. This header provides exactly that subset.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <exception>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cbx {

// ------------------------------------------------------------------------------------------ null
struct NullT {
    constexpr NullT() = default;
    constexpr NullT(std::nullptr_t) {}
    template <class T> constexpr operator T*() const { return nullptr; }
};
inline constexpr NullT null{};

// ------------------------------------------------------------------------------------ exceptions
class JavaException : public std::exception {
public:
    explicit JavaException(std::string m = "") : msg_(std::move(m)) {}
    const char* what() const noexcept override { return msg_.c_str(); }
    const std::string& message() const { return msg_; }
    // Java's e.getMessage()
    const std::string& getMessage() const { return msg_; }
    void printStackTrace() const { std::fprintf(stderr, "%s\n", msg_.c_str()); }
private:
    std::string msg_;
};
struct NullPointerException : JavaException { NullPointerException() : JavaException("NullPointerException") {} };
struct ArrayIndexOutOfBoundsException : JavaException {
    ArrayIndexOutOfBoundsException(int i, int len)
        : JavaException("Index " + std::to_string(i) + " out of bounds for length " + std::to_string(len)) {}
};
struct NegativeArraySizeException : JavaException { NegativeArraySizeException() : JavaException("NegativeArraySize") {} };
struct StringIndexOutOfBoundsException : JavaException { StringIndexOutOfBoundsException() : JavaException("StringIndexOutOfBounds") {} };
struct IOException : JavaException { explicit IOException(const std::string& m = "IOException") : JavaException(m) {} };
struct EOFException : IOException { EOFException() : IOException("EOFException") {} };
struct NumberFormatException : JavaException { NumberFormatException() : JavaException("NumberFormatException") {} };
struct RecordStoreException : IOException { explicit RecordStoreException(const std::string& m = "RecordStoreException") : IOException(m) {} };

// ------------------------------------------------------------------------------------------ String
// Immutable UTF-16 string with reference semantics (may be null), mirroring java.lang.String.
class String {
public:
    String() = default;
    String(NullT) {}
    String(const char16_t* s) : p_(std::make_shared<const std::u16string>(s)) {}
    String(const std::u16string& s) : p_(std::make_shared<const std::u16string>(s)) {}
    String(std::u16string&& s) : p_(std::make_shared<const std::u16string>(std::move(s))) {}
    static String fromUtf8(const std::string& s);

    bool isNull() const { return !p_; }
    const std::u16string& str() const { if (!p_) throw NullPointerException(); return *p_; }
    std::string toUtf8() const;

    int length() const { return (int)str().size(); }
    char16_t charAt(int i) const {
        const auto& s = str();
        if (i < 0 || i >= (int)s.size()) throw StringIndexOutOfBoundsException();
        return s[(size_t)i];
    }
    String substring(int from) const { return substring(from, length()); }
    String substring(int from, int to) const {
        const auto& s = str();
        if (from < 0 || to > (int)s.size() || from > to) throw StringIndexOutOfBoundsException();
        return String(s.substr((size_t)from, (size_t)(to - from)));
    }
    int indexOf(int ch, int from = 0) const {
        const auto& s = str();
        if (from < 0) from = 0;
        if (from >= (int)s.size()) return -1;
        size_t r = s.find((char16_t)ch, (size_t)from);
        return r == std::u16string::npos ? -1 : (int)r;
    }
    int indexOf(const String& t, int from = 0) const {
        const auto& s = str();
        if (from < 0) from = 0;
        if (from > (int)s.size()) from = (int)s.size();
        size_t r = s.find(t.str(), (size_t)from);
        return r == std::u16string::npos ? -1 : (int)r;
    }
    bool equals(const String& o) const { return p_ && o.p_ ? *p_ == *o.p_ : false; }
    std::vector<char16_t> toCharVector() const { const auto& s = str(); return std::vector<char16_t>(s.begin(), s.end()); }
    int compareTo(const String& o) const;
    String trim() const;
    template <class A = void> auto toCharArray() const;
    String replace(char16_t a, char16_t b) const {
        std::u16string s = str();
        for (auto& c : s) if (c == a) c = b;
        return String(std::move(s));
    }

    static String valueOf(int v);
    static String valueOf(int64_t v);
    static String valueOf(bool v) { return v ? String(u"true") : String(u"false"); }
    static String valueOf(char16_t c) { return String(std::u16string(1, c)); }
    static String valueOf(const String& s) { return s.isNull() ? String(u"null") : s; }

    bool operator==(NullT) const { return !p_; }
    bool operator!=(NullT) const { return (bool)p_; }
    // Java '==' on strings is identity; the game only compares against literals, so value equality is the intent.
    bool operator==(const String& o) const { return p_ && o.p_ ? *p_ == *o.p_ : p_ == o.p_; }
    bool operator!=(const String& o) const { return !(*this == o); }

private:
    std::shared_ptr<const std::u16string> p_;
};

inline String operator""_s(const char16_t* s, size_t n) { return String(std::u16string(s, n)); }

String operator+(const String& a, const String& b);
inline String operator+(const String& a, int v) { return a + String::valueOf(v); }
inline String operator+(int v, const String& a) { return String::valueOf(v) + a; }
inline String operator+(const String& a, char16_t c) { return a + String::valueOf(c); }
inline String operator+(char16_t c, const String& a) { return String::valueOf(c) + a; }
inline String operator+(const String& a, bool v) { return a + String::valueOf(v); }
inline String operator+(const String& a, int64_t v) { return a + String::valueOf(v); }
inline String operator+(const String& a, int16_t v) { return a + String::valueOf((int)v); }
inline String operator+(const String& a, int8_t v) { return a + String::valueOf((int)v); }
inline String operator+(const String& a, const std::string& v) { return a + String::fromUtf8(v); }
inline String& operator+=(String& a, const String& b) { a = a + b; return a; }

// ------------------------------------------------------------------------------------------ arrays
// Reference-semantics, bounds-checked, null-able array (Java T[]).
template <class T>
class Arr {
public:
    int length = 0;
    Arr() = default;
    Arr(NullT) {}
    Arr(std::initializer_list<T> il) : length((int)il.size()) {
        d_ = std::shared_ptr<T[]>(new T[il.size() ? il.size() : 1]());
        size_t i = 0;
        for (const T& v : il) d_[i++] = v;
    }
    static Arr sized(int n) {
        if (n < 0) throw NegativeArraySizeException();
        Arr a;
        a.length = n;
        a.d_ = std::shared_ptr<T[]>(new T[n ? (size_t)n : 1]());
        return a;
    }
    T& operator[](int i) const {
        if (!d_) throw NullPointerException();
        if ((unsigned)i >= (unsigned)length) throw ArrayIndexOutOfBoundsException(i, length);
        return d_[(size_t)i];
    }
    T* begin() const { if (!d_) throw NullPointerException(); return d_.get(); }
    T* end() const { return d_.get() + length; }
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }
    bool operator==(const Arr& o) const { return d_ == o.d_; }
    bool operator!=(const Arr& o) const { return d_ != o.d_; }
    T* data() const { return d_.get(); }
private:
    std::shared_ptr<T[]> d_;
};

template <class A> auto String::toCharArray() const {
    const auto& s = str();
    auto a = Arr<char16_t>::sized((int)s.size());
    for (size_t i = 0; i < s.size(); i++) a[(int)i] = s[i];
    return a;
}

// Array.newInstance(T.class, a, b[, c]) for rectangular multi-dimensional arrays.
template <class T> inline Arr<Arr<T>> newArray(int a, int b) {
    auto r = Arr<Arr<T>>::sized(a);
    for (int i = 0; i < a; i++) r[i] = Arr<T>::sized(b);
    return r;
}
template <class T> inline Arr<Arr<Arr<T>>> newArray(int a, int b, int c) {
    auto r = Arr<Arr<Arr<T>>>::sized(a);
    for (int i = 0; i < a; i++) r[i] = newArray<T>(b, c);
    return r;
}

// ------------------------------------------------------------------------------- Vector (raw type)
// java.util.Vector holding Strings or nested Vectors (all the game does with it).
class Vector {
public:
    struct Elem;
    Vector() : v_(std::make_shared<std::vector<Elem>>()) {}
    Vector(NullT) {}
    bool operator==(NullT) const { return !v_; }
    bool operator!=(NullT) const { return (bool)v_; }
    int size() const { chk(); return (int)v_->size(); }
    void addElement(const String& s);
    void addElement(const Vector& v);
    Elem elementAt(int i) const;
    void removeAllElements() { chk(); v_->clear(); }
private:
    void chk() const { if (!v_) throw NullPointerException(); }
    std::shared_ptr<std::vector<Elem>> v_;
};
struct Vector::Elem {
    String s;
    Vector v{null};
    operator String() const { return s; }
    operator Vector() const { return v; }
};
inline void Vector::addElement(const String& s) { chk(); Elem e; e.s = s; v_->push_back(e); }
inline void Vector::addElement(const Vector& v) { chk(); Elem e; e.v = v; v_->push_back(e); }
inline Vector::Elem Vector::elementAt(int i) const {
    chk();
    if ((unsigned)i >= v_->size()) throw ArrayIndexOutOfBoundsException(i, (int)v_->size());
    return (*v_)[(size_t)i];
}

// StringBuffer: only used by the original menu code as a nullable length() probe.
class StringBuffer {
public:
    StringBuffer() = default;
    StringBuffer(NullT) {}
    bool operator==(NullT) const { return !p_; }
    int length() const { if (!p_) throw NullPointerException(); return (int)p_->size(); }
private:
    std::shared_ptr<std::u16string> p_;
};

// ------------------------------------------------------------------------------------- java.lang.*
struct Math {
    static int abs(int a) { return a < 0 ? (int)(0u - (uint32_t)a) : a; }
    static int64_t abs(int64_t a) { return a < 0 ? -a : a; }
    static float abs(float a) { return std::fabs(a); }
    static double abs(double a) { return std::fabs(a); }
    static int min(int a, int b) { return a < b ? a : b; }
    static int max(int a, int b) { return a > b ? a : b; }
    static int64_t min(int64_t a, int64_t b) { return a < b ? a : b; }
    static int64_t max(int64_t a, int64_t b) { return a > b ? a : b; }
    static float min(float a, float b) { return a < b ? a : b; }
    static float max(float a, float b) { return a > b ? a : b; }
};

// Java's (int) cast of a floating point value saturates and maps NaN to 0.
inline int f2i(double v) {
    if (v != v) return 0;
    if (v >= 2147483647.0) return INT32_MAX;
    if (v <= -2147483648.0) return INT32_MIN;
    return (int)v;
}
inline int ushr(int a, int n) { return (int)((uint32_t)a >> (n & 31)); }

struct Integer {
    static constexpr int MAX_VALUE = INT32_MAX, MIN_VALUE = INT32_MIN;
    static int parseInt(const String& s);
};

struct System {
    struct Out {
        void println(const String& s) const;
        void println(const char* s) const;
    };
    static inline Out out{};
    static int64_t currentTimeMillis();   // virtual in tests (see clock.h)
    static void println(const String& s);
    static void println(const char* s);
    static void gc() {}
    template <class T> static void arraycopy(const Arr<T>& src, int sp, const Arr<T>& dst, int dp, int n) {
        if (src == null || dst == null) throw NullPointerException();
        if (sp < 0 || dp < 0 || n < 0 || sp + n > src.length || dp + n > dst.length)
            throw ArrayIndexOutOfBoundsException(sp, src.length);
        if (n > 0) std::memmove(dst.data() + dp, src.data() + sp, (size_t)n * sizeof(T));
    }
};

// java.util.Random (48-bit LCG) - identical sequence for identical seeds.
class Random {
public:
    Random() : Random(0) {}
    explicit Random(int64_t seed) { setSeed(seed); }
    void setSeed(int64_t seed) { seed_ = (seed ^ 0x5DEECE66DLL) & ((1LL << 48) - 1); }
    int nextInt() { return next(32); }
private:
    int next(int bits) {
        seed_ = (seed_ * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
        return (int)(seed_ >> (48 - bits));
    }
    int64_t seed_ = 0;
};

// ------------------------------------------------------------------------------------ data streams
// Byte-buffer backed java.io.DataInputStream / DataOutputStream (big endian, modified UTF-8).
class DataInputStream {
public:
    DataInputStream() = default;
    DataInputStream(NullT) {}
    explicit DataInputStream(std::vector<uint8_t> bytes)
        : b_(std::make_shared<std::vector<uint8_t>>(std::move(bytes))), pos_(std::make_shared<size_t>(0)) {}
    bool operator==(NullT) const { return !b_; }
    bool operator!=(NullT) const { return (bool)b_; }

    int readUnsignedByte() { need(1); return (*b_)[(*pos_)++]; }
    int8_t readByte() { return (int8_t)readUnsignedByte(); }
    bool readBoolean() { return readUnsignedByte() != 0; }
    int readUnsignedShort() { int a = readUnsignedByte(); int c = readUnsignedByte(); return (a << 8) | c; }
    int16_t readShort() { return (int16_t)readUnsignedShort(); }
    int readInt() {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | (uint32_t)readUnsignedByte();
        return (int)v;
    }
    String readUTF();
    int skipBytes(int n) {
        if (n <= 0) return 0;
        if (!b_) throw NullPointerException();
        size_t k = std::min((size_t)n, b_->size() - *pos_);
        *pos_ += k;
        return (int)k;
    }
    void close() {}
    void rewind() { if (pos_) *pos_ = 0; }
private:
    void need(size_t n) const {
        if (!b_) throw NullPointerException();
        if (*pos_ + n > b_->size()) throw EOFException();
    }
    std::shared_ptr<std::vector<uint8_t>> b_;
    std::shared_ptr<size_t> pos_;
};

class DataOutputStream {
public:
    DataOutputStream() = default;
    DataOutputStream(NullT) {}
    static DataOutputStream create() { DataOutputStream d; d.b_ = std::make_shared<std::vector<uint8_t>>(); return d; }
    bool operator==(NullT) const { return !b_; }
    bool operator!=(NullT) const { return (bool)b_; }
    void writeByte(int v) { chk(); b_->push_back((uint8_t)v); }
    void writeBoolean(bool v) { writeByte(v ? 1 : 0); }
    void writeShort(int v) { writeByte(v >> 8); writeByte(v); }
    void writeInt(int v) { writeByte(v >> 24); writeByte(v >> 16); writeByte(v >> 8); writeByte(v); }
    void writeUTF(const String& s);
    void close() {}
    const std::vector<uint8_t>& bytes() const { chk(); return *b_; }
private:
    void chk() const { if (!b_) throw NullPointerException(); }
    std::shared_ptr<std::vector<uint8_t>> b_;
};

} // namespace cbx
