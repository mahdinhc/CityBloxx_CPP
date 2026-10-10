// model_obj.cpp - loads game meshes from Wavefront OBJ/MTL files (assets/models), the same subset and the same
// numeric behaviour as the original Java loader (jme/Model.java).
//
//   OBJ: v x y z [r g b], vt s t, f (v, v/vt, v/vt/vn, v//vn; polygons fan-triangulated; negative indices),
//        mtllib, usemtl, "#@default_color AARRGGBB"; everything else is ignored.
//   MTL: newmtl, map_Kd, and "#@key value" render-state comments (blend, alpha_threshold, depth_test,
//        depth_write, cull, winding, texture_function, clamp).
//   The V texture coordinate is stored flipped (1 - t) in OBJ files and flipped back here.
#include "g3d.h"
#include "codec.h"
#include "../platform/assets.h"
#include <cctype>
#include <cstdlib>
#include <map>
#include <unordered_map>

namespace cbx {

namespace {

// ---- Java string helpers -------------------------------------------------------------------------
std::string jtrim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') b++;
    while (e > b && (unsigned char)s[e - 1] <= ' ') e--;
    return s.substr(b, e - b);
}
bool parseBoolJ(const std::string& v) {      // Boolean.parseBoolean
    if (v.size() != 4) return false;
    const char* t = "true";
    for (int i = 0; i < 4; i++) if (std::tolower((unsigned char)v[(size_t)i]) != t[i]) return false;
    return true;
}
bool isWs(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }
// line.split("\\s+") for an already trimmed line (no leading/trailing whitespace)
std::vector<std::string> splitWs(const std::string& s) {
    std::vector<std::string> out; size_t i = 0;
    while (i < s.size()) {
        size_t j = i;
        while (j < s.size() && !isWs(s[j])) j++;
        out.push_back(s.substr(i, j - i));
        while (j < s.size() && isWs(s[j])) j++;
        i = j;
    }
    return out;
}
// s.split("/", -1)
std::vector<std::string> splitSlash(const std::string& s) {
    std::vector<std::string> out; size_t i = 0;
    for (;;) {
        size_t j = s.find('/', i);
        if (j == std::string::npos) { out.push_back(s.substr(i)); break; }
        out.push_back(s.substr(i, j - i)); i = j + 1;
    }
    return out;
}
const std::string& tk(const std::vector<std::string>& t, size_t i) {
    if (i >= t.size()) throw ArrayIndexOutOfBoundsException((int)i, (int)t.size());
    return t[i];
}
// Float.parseFloat: correctly rounded decimal -> float (the C locale is never changed by this program)
float parseFloatJ(const std::string& str) {
    std::string s = jtrim(str);
    if (s.empty()) throw NumberFormatException();
    char* end = nullptr;
    float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str()) throw NumberFormatException();
    if (*end == 'f' || *end == 'F' || *end == 'd' || *end == 'D') end++;
    if (*end != 0) throw NumberFormatException();
    return v;
}
int parseIntJ(const std::string& s) { return Integer::parseInt(String::fromUtf8(s)); }
// Math.round(float)
int roundJ(float x) { return f2i(std::floor((double)x + 0.5)); }

// BufferedReader.readLine over an in-memory file ("\n", "\r" and "\r\n" terminate lines)
std::vector<std::string> readLines(const std::string& path) {
    std::vector<uint8_t> data;
    if (!Assets::read(path, data)) throw IOException("missing asset " + path);
    std::vector<std::string> lines; std::string cur;
    for (size_t i = 0; i < data.size(); i++) {
        char c = (char)data[i];
        if (c == '\n' || c == '\r') {
            lines.push_back(cur); cur.clear();
            if (c == '\r' && i + 1 < data.size() && data[i + 1] == '\n') i++;
        } else cur.push_back(c);
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

struct Mat {
    std::string texPath; bool hasTex = false;
    int blending = Model::BLEND_REPLACE, alphaThreshold = 0, culling = Model::CULL_BACK, texFunc = Model::FUNC_MODULATE;
    bool depthTest = true, depthWrite = true, windingCCW = true, clamp = false;
};

std::map<std::string, std::shared_ptr<Tex>>& textureCache() { static std::map<std::string, std::shared_ptr<Tex>> c; return c; }

std::shared_ptr<Tex> loadTexture(const std::string& path) {
    auto& cache = textureCache();
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    try {
        std::vector<uint8_t> data;
        if (!Assets::read(path, data)) return nullptr;
        DecodedImage di = decodePng(data.data(), data.size());
        auto t = std::make_shared<Tex>();
        t->w = di.w; t->h = di.h; t->argb = std::move(di.argb);
        cache[path] = t;
        return t;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "texture %s: %s\n", path.c_str(), e.what());
        return nullptr;
    }
}

void parseMtl(const std::string& path, std::map<std::string, Mat>& out) {
    try {
        Mat* cur = nullptr;
        for (std::string line : readLines(path)) {
            line = jtrim(line);
            if (line.rfind("newmtl ", 0) == 0) { cur = &out[jtrim(line.substr(7))]; *cur = Mat(); }
            else if (!cur) continue;
            else if (line.rfind("map_Kd ", 0) == 0) { cur->texPath = Assets::dirOf(path) + jtrim(line.substr(7)); cur->hasTex = true; }
            else if (line.rfind("#@", 0) == 0) {
                std::string rest = jtrim(line.substr(2));
                size_t sp = 0;
                while (sp < rest.size() && !isWs(rest[sp])) sp++;
                if (sp >= rest.size()) continue;               // fewer than two parts
                std::string k = rest.substr(0, sp), v = jtrim(rest.substr(sp));
                if (k == "blend") cur->blending = v == "ALPHA" ? Model::BLEND_ALPHA : Model::BLEND_REPLACE;
                else if (k == "alpha_threshold") cur->alphaThreshold = parseIntJ(v);
                else if (k == "depth_test") cur->depthTest = parseBoolJ(v);
                else if (k == "depth_write") cur->depthWrite = parseBoolJ(v);
                else if (k == "cull") cur->culling = v == "FRONT" ? Model::CULL_FRONT : v == "NONE" ? Model::CULL_NONE : Model::CULL_BACK;
                else if (k == "winding") cur->windingCCW = v != "CW";
                else if (k == "texture_function") cur->texFunc = v == "REPLACE" ? Model::FUNC_REPLACE : Model::FUNC_MODULATE;
                else if (k == "clamp") cur->clamp = parseBoolJ(v);
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "mtl %s: %s\n", path.c_str(), e.what());
    }
}

int idxJ(int i, int count) { return i > 0 ? i - 1 : count + i; }

std::shared_ptr<Model> loadObj(const std::string& objPath) {
    std::vector<float> vx, vt, vc, outPos, outUv, outCol;
    bool haveColors = false, anyUv = false;
    int nv = 0, nvt = 0;
    int defaultColor = (int)0xFFFFFFFFu;
    std::map<std::string, Mat> materials;
    std::unordered_map<uint64_t, int> unify;
    std::vector<Mat> subMats;
    std::vector<std::vector<int>> subTris;
    int current = -1;
    Mat curMat;
    bool curDirty = true;

    for (std::string line : readLines(objPath)) {
        line = jtrim(line);
        if (line.empty()) continue;
        if (line[0] == '#') {
            if (line.rfind("#@default_color ", 0) == 0)
                defaultColor = (int)(uint32_t)std::strtoull(jtrim(line.substr(16)).c_str(), nullptr, 16);
            continue;
        }
        std::vector<std::string> t = splitWs(line);
        const std::string& key = t[0];
        if (key == "v") {
            vx.push_back(parseFloatJ(tk(t, 1))); vx.push_back(parseFloatJ(tk(t, 2))); vx.push_back(parseFloatJ(tk(t, 3)));
            if (t.size() >= 7) {
                haveColors = true;
                vc.push_back(parseFloatJ(t[4])); vc.push_back(parseFloatJ(t[5])); vc.push_back(parseFloatJ(t[6]));
            } else { vc.push_back(-1.0f); vc.push_back(-1.0f); vc.push_back(-1.0f); }
            nv++;
        } else if (key == "vt") {
            vt.push_back(parseFloatJ(tk(t, 1)));
            vt.push_back(1.0f - (t.size() > 2 ? parseFloatJ(t[2]) : 0.0f));
            nvt++;
        } else if (key == "mtllib") {
            parseMtl(Assets::dirOf(objPath) + jtrim(line.substr(6)), materials);
        } else if (key == "usemtl") {
            auto it = materials.find(jtrim(line.substr(6)));
            curMat = it != materials.end() ? it->second : Mat();
            curDirty = true;
        } else if (key == "f") {
            if (curDirty) {
                subTris.emplace_back(); subMats.push_back(curMat);
                current = (int)subTris.size() - 1; curDirty = false;
            }
            int cnt = (int)t.size() - 1;
            std::vector<int> face((size_t)cnt);
            for (int k = 0; k < cnt; k++) {
                std::vector<std::string> c = splitSlash(t[(size_t)k + 1]);
                int vi = idxJ(parseIntJ(tk(c, 0)), nv);
                int ti = (c.size() > 1 && !c[1].empty()) ? idxJ(parseIntJ(c[1]), nvt) : -1;
                uint64_t ukey = ((uint64_t)(uint32_t)vi << 32) | (uint64_t)(int64_t)(ti + 1LL);
                auto it = unify.find(ukey);
                int u;
                if (it == unify.end()) {
                    u = (int)(outPos.size() / 3);
                    unify[ukey] = u;
                    if (vi < 0 || (size_t)vi * 3 + 2 >= vx.size()) throw ArrayIndexOutOfBoundsException(vi * 3, (int)vx.size());
                    outPos.push_back(vx[(size_t)vi * 3]); outPos.push_back(vx[(size_t)vi * 3 + 1]); outPos.push_back(vx[(size_t)vi * 3 + 2]);
                    if (ti >= 0) {
                        if ((size_t)ti * 2 + 1 >= vt.size()) throw ArrayIndexOutOfBoundsException(ti * 2, (int)vt.size());
                        outUv.push_back(vt[(size_t)ti * 2]); outUv.push_back(vt[(size_t)ti * 2 + 1]); anyUv = true;
                    } else { outUv.push_back(0.0f); outUv.push_back(0.0f); }
                    outCol.push_back(vc[(size_t)vi * 3]); outCol.push_back(vc[(size_t)vi * 3 + 1]); outCol.push_back(vc[(size_t)vi * 3 + 2]);
                } else u = it->second;
                face[(size_t)k] = u;
            }
            for (int k = 1; k + 1 < cnt; k++) {      // fan triangulation
                auto& tr = subTris[(size_t)current];
                tr.push_back(face[0]); tr.push_back(face[(size_t)k]); tr.push_back(face[(size_t)k + 1]);
            }
        }
    }

    auto m = std::make_shared<Model>();
    m->defaultColor = defaultColor;
    m->vertexCount = (int)(outPos.size() / 3);
    m->pos = outPos;
    if (anyUv) m->uv = outUv;
    m->colors.assign((size_t)m->vertexCount, 0);
    for (int v = 0; v < m->vertexCount; v++) {
        float cr = outCol[(size_t)v * 3];
        if (!haveColors || cr < 0.0f) m->colors[(size_t)v] = defaultColor;
        else {
            int rr = roundJ(cr * 255.0f), gg = roundJ(outCol[(size_t)v * 3 + 1] * 255.0f), bb = roundJ(outCol[(size_t)v * 3 + 2] * 255.0f);
            m->colors[(size_t)v] = (int)(0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb);
        }
    }
    m->subs.resize(subTris.size());
    for (size_t i = 0; i < subTris.size(); i++) {
        const Mat& mt = subMats[i];
        ModelSub& s = m->subs[i];
        s.tris = subTris[i];
        s.blending = mt.blending; s.alphaThreshold = mt.alphaThreshold;
        s.depthTest = mt.depthTest; s.depthWrite = mt.depthWrite;
        s.culling = mt.culling; s.windingCCW = mt.windingCCW;
        s.texFunc = mt.texFunc; s.clamp = mt.clamp;
        s.tex = mt.hasTex ? loadTexture(mt.texPath) : nullptr;
    }
    return m;
}

// models.txt: "<model id> <obj file>" per line
std::map<int, std::string> loadIndex() {
    std::map<int, std::string> map;
    try {
        std::vector<uint8_t> probe;
        if (!Assets::read("models/models.txt", probe)) return map;
        for (std::string line : readLines("models/models.txt")) {
            line = jtrim(line);
            if (line.empty() || line[0] == '#') continue;
            std::vector<std::string> p = splitWs(line);
            if (p.size() >= 2) map[parseIntJ(p[0])] = p[1];
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "models.txt: %s\n", e.what());
    }
    return map;
}

} // namespace

std::shared_ptr<Model> Model::find(int userId) {
    static std::map<int, std::shared_ptr<Model>> cache;
    static std::map<int, std::string> index;
    static bool indexLoaded = false;
    auto it = cache.find(userId);
    if (it != cache.end()) return it->second;
    if (!indexLoaded) { index = loadIndex(); indexLoaded = true; }
    auto f = index.find(userId);
    if (f == index.end()) return nullptr;
    try {
        std::shared_ptr<Model> m = loadObj("models/" + f->second);
        m->userId = userId;
        cache[userId] = m;
        return m;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "model %d: %s\n", userId, e.what());
        return nullptr;
    }
}

} // namespace cbx
