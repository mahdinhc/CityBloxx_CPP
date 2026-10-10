#include "g3d.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace cbx {

// ================================================================================ Graphics3D
namespace {
constexpr int W = SCREEN_W, H = SCREEN_H;
struct G3State {
    Graphics pendingTarget_, target;
    std::vector<uint32_t> color = std::vector<uint32_t>((size_t)W * H, 0);
    std::vector<float> depth = std::vector<float>((size_t)W * H, std::numeric_limits<float>::infinity());
    int dMinX = 0, dMinY = 0, dMaxX = 0, dMaxY = 0;
    bool dirty = false;
    int vpX = 0, vpY = 0, vpW = W, vpH = H;
    int clipX0 = 0, clipY0 = 0, clipX1 = 0, clipY1 = 0;
    std::vector<float> view = Graphics3D::identity(), proj = Graphics3D::identity();
};
G3State& S() { static G3State s; return s; }
constexpr double DEG2RAD = 0.017453292519943295;
}

std::vector<float> Graphics3D::identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

std::vector<float> Graphics3D::mul(const std::vector<float>& a, const std::vector<float>& b) {
    std::vector<float> r(16);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[(size_t)(i * 4 + k)] * b[(size_t)(k * 4 + j)];
            r[(size_t)(i * 4 + j)] = s;
        }
    return r;
}

std::vector<float> Graphics3D::invert(const std::vector<float>& m) {
    double a[32] = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) a[i * 8 + j] = m[(size_t)(i * 4 + j)];
        a[i * 8 + 4 + i] = 1;
    }
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (std::fabs(a[r * 8 + c]) > std::fabs(a[p * 8 + c])) p = r;
        if (std::fabs(a[p * 8 + c]) < 1e-12) return identity();
        if (p != c) for (int k = 0; k < 8; k++) { double t = a[c * 8 + k]; a[c * 8 + k] = a[p * 8 + k]; a[p * 8 + k] = t; }
        double d = a[c * 8 + c];
        for (int k = 0; k < 8; k++) a[c * 8 + k] /= d;
        for (int r = 0; r < 4; r++) if (r != c) {
            double f = a[r * 8 + c];
            if (f != 0) for (int k = 0; k < 8; k++) a[r * 8 + k] -= f * a[c * 8 + k];
        }
    }
    std::vector<float> out(16);
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) out[(size_t)(i * 4 + j)] = (float)a[i * 8 + 4 + j];
    return out;
}

std::vector<float> Graphics3D::translation(float x, float y, float z) {
    auto m = identity();
    m[3] = x; m[7] = y; m[11] = z;
    return m;
}

std::vector<float> Graphics3D::rotation(float angleDeg, float ax, float ay, float az) {
    double len = std::sqrt((double)(ax * ax + ay * ay + az * az));
    if (len == 0) return identity();
    double x = ax / len, y = ay / len, z = az / len;
    double a = (double)angleDeg * DEG2RAD, c = std::cos(a), s = std::sin(a), t = 1 - c;
    auto m = identity();
    m[0] = (float)(t * x * x + c);     m[1] = (float)(t * x * y - s * z); m[2] = (float)(t * x * z + s * y);
    m[4] = (float)(t * x * y + s * z); m[5] = (float)(t * y * y + c);     m[6] = (float)(t * y * z - s * x);
    m[8] = (float)(t * x * z - s * y); m[9] = (float)(t * y * z + s * x); m[10] = (float)(t * z * z + c);
    return m;
}

std::vector<float> Graphics3D::perspective(float fovyDeg, float aspect, float nearP, float farP) {
    float f = (float)(1.0 / std::tan((double)fovyDeg * DEG2RAD * 0.5));
    std::vector<float> m(16, 0.0f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = -(farP + nearP) / (farP - nearP);
    m[11] = -2 * farP * nearP / (farP - nearP);
    m[14] = -1;
    return m;
}

void Graphics3D::transform(const std::vector<float>& m, Arr<float>& v) {
    float x = v[0], y = v[1], z = v[2], w = v[3];
    v[0] = m[0] * x + m[1] * y + m[2] * z + m[3] * w;
    v[1] = m[4] * x + m[5] * y + m[6] * z + m[7] * w;
    v[2] = m[8] * x + m[9] * y + m[10] * z + m[11] * w;
    v[3] = m[12] * x + m[13] * y + m[14] * z + m[15] * w;
}

void Graphics3D::setCamera(const std::vector<float>& cameraToWorld, const std::vector<float>& projection) {
    S().view = invert(cameraToWorld);
    S().proj = projection;
}

void Graphics3D::bindTarget(const Graphics& g, int x, int y, int w, int h) {
    flushPending();
    auto& s = S();
    s.target = g;
    s.vpX = x; s.vpY = y; s.vpW = w; s.vpH = h;
    s.clipX0 = std::max(0, g.getClipX());
    s.clipY0 = std::max(0, g.getClipY());
    s.clipX1 = std::min(W, g.getClipX() + g.getClipWidth());
    s.clipY1 = std::min(H, g.getClipY() + g.getClipHeight());
}

void Graphics3D::releaseTarget() { flushPending(); S().target = Graphics(); }

void Graphics3D::clearDepth() {
    std::fill(S().depth.begin(), S().depth.end(), std::numeric_limits<float>::infinity());
}

const void* Graphics3D::pendingId() { return S().pendingTarget_.id(); }

void Graphics3D::flushPending() {
    auto& s = S();
    if (!s.dirty || s.pendingTarget_ == null) { s.dirty = false; s.pendingTarget_ = Graphics(); return; }
    Graphics t = s.pendingTarget_;
    s.pendingTarget_ = Graphics();
    int w = s.dMaxX - s.dMinX + 1, h = s.dMaxY - s.dMinY + 1;
    if (w > 0 && h > 0) {
        t.compositeArgb(s.color.data(), W, s.dMinX, s.dMinY, s.dMaxX + 1, s.dMaxY + 1);
        for (int y = s.dMinY; y <= s.dMaxY; y++)
            std::fill(s.color.begin() + (long)(y * W + s.dMinX), s.color.begin() + (long)(y * W + s.dMaxX + 1), 0u);
    }
    s.dirty = false;
}

namespace {
constexpr int MAXV = 12;
struct V10 { float f[10]; };

void drawTri(const ModelSub& s, const V10& A, const V10& B, const V10& C) {
    auto& st = S();
    const float* a = A.f; const float* b = B.f; const float* c = C.f;
    const float* v[3] = {a, b, c};
    float sx[3], sy[3], sz[3], iw[3];
    for (int i = 0; i < 3; i++) {
        float w = v[i][3];
        if (w <= 1e-6f) return;
        iw[i] = 1.0f / w;
        float nx = v[i][0] * iw[i], ny = v[i][1] * iw[i], nz = v[i][2] * iw[i];
        sx[i] = (float)st.vpX + (nx * 0.5f + 0.5f) * (float)st.vpW;
        sy[i] = (float)st.vpY + (-ny * 0.5f + 0.5f) * (float)st.vpH;
        sz[i] = nz;
    }
    float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (area == 0) return;
    bool ccwOnScreen = area < 0;
    bool front = s.windingCCW ? ccwOnScreen : !ccwOnScreen;
    if (s.culling == Model::CULL_BACK && !front) return;
    if (s.culling == Model::CULL_FRONT && front) return;

    int minX = std::max(st.clipX0, f2i(std::floor((double)std::min(sx[0], std::min(sx[1], sx[2])))));
    int maxX = std::min(st.clipX1 - 1, f2i(std::ceil((double)std::max(sx[0], std::max(sx[1], sx[2])))));
    int minY = std::max(st.clipY0, f2i(std::floor((double)std::min(sy[0], std::min(sy[1], sy[2])))));
    int maxY = std::min(st.clipY1 - 1, f2i(std::ceil((double)std::max(sy[0], std::max(sy[1], sy[2])))));
    if (minX > maxX || minY > maxY) return;

    float invArea = 1.0f / area;
    const Tex* tex = s.tex.get();
    bool alphaBlend = s.blending == Model::BLEND_ALPHA;
    int thr = s.alphaThreshold;
    for (int y = minY; y <= maxY; y++) {
        float py = (float)y + 0.5f;
        for (int x = minX; x <= maxX; x++) {
            float px = (float)x + 0.5f;
            float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * invArea;
            float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * invArea;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
            if (z < -1.0f || z > 1.0f) continue;
            int di = y * W + x;
            if (s.depthTest && z > st.depth[(size_t)di]) continue;
            float p0 = w0 * iw[0], p1 = w1 * iw[1], p2 = w2 * iw[2];
            float ps = p0 + p1 + p2;
            if (ps == 0) continue;
            float k0 = p0 / ps, k1 = p1 / ps, k2 = p2 / ps;
            int r, g, bl, al;
            float vr = k0 * a[6] + k1 * b[6] + k2 * c[6];
            float vg = k0 * a[7] + k1 * b[7] + k2 * c[7];
            float vb = k0 * a[8] + k1 * b[8] + k2 * c[8];
            float va = k0 * a[9] + k1 * b[9] + k2 * c[9];
            if (tex) {
                float u = k0 * a[4] + k1 * b[4] + k2 * c[4];
                float t = k0 * a[5] + k1 * b[5] + k2 * c[5];
                int tx = f2i(std::floor((double)(u * (float)tex->w))), ty = f2i(std::floor((double)(t * (float)tex->h)));
                if (s.clamp) {
                    tx = tx < 0 ? 0 : (tx >= tex->w ? tex->w - 1 : tx);
                    ty = ty < 0 ? 0 : (ty >= tex->h ? tex->h - 1 : ty);
                } else {
                    tx = ((tx % tex->w) + tex->w) % tex->w;
                    ty = ((ty % tex->h) + tex->h) % tex->h;
                }
                uint32_t tc = tex->argb[(size_t)(ty * tex->w + tx)];
                r = (int)((tc >> 16) & 255); g = (int)((tc >> 8) & 255); bl = (int)(tc & 255); al = (int)(tc >> 24);
                if (s.texFunc == Model::FUNC_MODULATE) {
                    r = f2i(r * vr / 255.0f); g = f2i(g * vg / 255.0f); bl = f2i(bl * vb / 255.0f); al = f2i(al * va / 255.0f);
                }
            } else {
                r = f2i(vr); g = f2i(vg); bl = f2i(vb); al = f2i(va);
            }
            if (thr > 0 && al < thr) continue;
            if (!alphaBlend) al = 255;
            if (s.depthWrite) st.depth[(size_t)di] = z;
            uint32_t dst = st.color[(size_t)di];
            if (al >= 255 || ((dst >> 24) == 0 && al >= 255)) {
                st.color[(size_t)di] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
            } else if (al > 0) {
                int da = (int)(dst >> 24);
                if (da == 0) {
                    st.color[(size_t)di] = ((uint32_t)al << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
                } else {
                    float fa = (float)al / 255.0f;
                    int nr = f2i(r * fa + (float)((dst >> 16) & 255) * (1 - fa));
                    int ng = f2i(g * fa + (float)((dst >> 8) & 255) * (1 - fa));
                    int nb = f2i(bl * fa + (float)(dst & 255) * (1 - fa));
                    st.color[(size_t)di] = 0xFF000000u | ((uint32_t)nr << 16) | ((uint32_t)ng << 8) | (uint32_t)nb;
                }
            } else continue;
            if (!st.dirty) { st.dirty = true; st.dMinX = x; st.dMaxX = x; st.dMinY = y; st.dMaxY = y; }
            else {
                if (x < st.dMinX) st.dMinX = x;
                if (x > st.dMaxX) st.dMaxX = x;
                if (y < st.dMinY) st.dMinY = y;
                if (y > st.dMaxY) st.dMaxY = y;
            }
            st.pendingTarget_ = st.target;
        }
    }
}
} // namespace

void Graphics3D::render(const Model& m, const std::vector<float>& model) {
    auto& st = S();
    if (st.target == null) return;
    auto mv = mul(st.view, model);
    auto mvp = mul(st.proj, mv);
    int n = m.vertexCount;
    std::vector<float> clip((size_t)n * 4);
    for (int i = 0; i < n; i++) {
        float x = m.pos[(size_t)i * 3], y = m.pos[(size_t)i * 3 + 1], z = m.pos[(size_t)i * 3 + 2];
        clip[(size_t)i * 4] = mvp[0] * x + mvp[1] * y + mvp[2] * z + mvp[3];
        clip[(size_t)i * 4 + 1] = mvp[4] * x + mvp[5] * y + mvp[6] * z + mvp[7];
        clip[(size_t)i * 4 + 2] = mvp[8] * x + mvp[9] * y + mvp[10] * z + mvp[11];
        clip[(size_t)i * 4 + 3] = mvp[12] * x + mvp[13] * y + mvp[14] * z + mvp[15];
    }
    V10 poly[MAXV], tmp[MAXV];
    for (const ModelSub& s : m.subs) {
        const auto& t = s.tris;
        for (size_t i = 0; i + 2 < t.size(); i += 3) {
            int cnt = 0;
            for (int k = 0; k < 3; k++) {
                int vi = t[i + (size_t)k];
                float* p = poly[cnt++].f;
                p[0] = clip[(size_t)vi * 4]; p[1] = clip[(size_t)vi * 4 + 1]; p[2] = clip[(size_t)vi * 4 + 2]; p[3] = clip[(size_t)vi * 4 + 3];
                if (!m.uv.empty()) { p[4] = m.uv[(size_t)vi * 2]; p[5] = m.uv[(size_t)vi * 2 + 1]; } else { p[4] = 0; p[5] = 0; }
                int c = m.colors[(size_t)vi];
                p[6] = (float)((c >> 16) & 255); p[7] = (float)((c >> 8) & 255); p[8] = (float)(c & 255); p[9] = (float)((uint32_t)c >> 24);
            }
            int outCnt = 0;
            for (int k = 0; k < cnt; k++) {
                const float* a = poly[k].f; const float* b = poly[(k + 1) % cnt].f;
                float da = a[2] + a[3], db = b[2] + b[3];
                if (da >= 0) std::memcpy(tmp[outCnt++].f, a, sizeof(float) * 10);
                if ((da >= 0) != (db >= 0)) {
                    float f = da / (da - db);
                    float* o = tmp[outCnt++].f;
                    for (int q = 0; q < 10; q++) o[q] = a[q] + (b[q] - a[q]) * f;
                }
            }
            if (outCnt < 3) continue;
            for (int k = 1; k + 1 < outCnt; k++) drawTri(s, tmp[0], tmp[k], tmp[k + 1]);
        }
    }
}

// ===================================================================================== Mesh3D
Mesh3D::Mesh3D(std::shared_ptr<Model> m) {
    d_ = std::make_shared<Impl>();
    d_->mesh = std::move(m);
    d_->transform = Graphics3D::identity();
}
void Mesh3D::render() { Graphics3D::render(*I().mesh, I().transform); }
void Mesh3D::translate(float dx, float dy, float dz) {
    if (dx < 1.0E-4f && dx > -1.0E-4f) dx = 0.0f;
    if (dy < 1.0E-4f && dy > -1.0E-4f) dy = 0.0f;
    if (dz < 1.0E-4f && dz > -1.0E-4f) dz = 0.0f;
    I().transform = Graphics3D::mul(I().transform, Graphics3D::translation(dx, dy, dz));
}
void Mesh3D::rotate(float angleFraction, float ax, float ay, float az) {
    I().transform = Graphics3D::mul(I().transform, Graphics3D::rotation(360.0f * angleFraction, ax, ay, az));
}
void Mesh3D::setupAppearance() { I().mesh->setupAppearance(); }
void Mesh3D::resetTransform() { I().transform = Graphics3D::identity(); }

// ================================================================================== Renderer3D
namespace {
std::map<int, Mesh3D> g_models;
std::vector<float> g_cameraTransform = Graphics3D::identity();
std::vector<float> g_projection = Graphics3D::identity();
float g_fov = 0, g_aspect = 0, g_aspectInv = 0;
int g_viewTop = 0, g_viewWidth = 0, g_viewHeight = 0;
}
Arr<float> Renderer3D::projected = Arr<float>::sized(4);

Mesh3D Renderer3D::getModel(int id, int, bool load) {
    auto it = g_models.find(id);
    if (it != g_models.end() || !load) return it != g_models.end() ? it->second : Mesh3D();
    auto mesh = Model::find(id);
    if (!mesh) return Mesh3D();
    Mesh3D created(mesh);
    created.setupAppearance();
    g_models[id] = created;
    return created;
}
void Renderer3D::setFov(float fovDegrees) {
    g_fov = fovDegrees;
    g_projection = Graphics3D::perspective(g_fov * g_aspectInv, g_aspect, 10.0f, 10000.0f);
}
void Renderer3D::lookAt(float eyeX, float eyeY, float eyeZ, float dirX, float dirY, float dirZ, float upX, float upY, float upZ) {
    if (dirX < 1.0E-4f && dirX > -1.0E-4f) dirX = 0.0f;
    if (dirY < 1.0E-4f && dirY > -1.0E-4f) dirY = 0.0f;
    if (dirZ < 1.0E-4f && dirZ > -1.0E-4f) dirZ = 0.0f;
    float crossX = (dirY * upZ) - (dirZ * upY);
    float crossY = (dirZ * upX) - (dirX * upZ);
    float crossZ = (dirX * upY) - (dirY * upX);
    float invLength = 1.0f / ((float)std::sqrt((double)(((crossX * crossX) + (crossY * crossY)) + (crossZ * crossZ))));
    crossX *= invLength; crossY *= invLength; crossZ *= invLength;
    float finalUpX = (crossY * dirZ) - (crossZ * dirY);
    float finalUpY = (crossZ * dirX) - (crossX * dirZ);
    float finalUpZ = (crossX * dirY) - (crossY * dirX);
    std::vector<float> m(16);
    m[0] = crossX; m[1] = finalUpX; m[2] = -dirX; m[3] = eyeX;
    m[4] = crossY; m[5] = finalUpY; m[6] = -dirY; m[7] = eyeY;
    m[8] = crossZ; m[9] = finalUpZ; m[10] = -dirZ; m[11] = eyeZ;
    m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
    g_cameraTransform = m;
    Graphics3D::setCamera(g_cameraTransform, g_projection);
}
void Renderer3D::init(int top, int width, int height) {
    g_models.clear();
    setupCamera(top, width, height);
}
void Renderer3D::beginFrame(const Graphics& g) {
    setClip(g);
    Graphics3D::bindTarget(g, 0, 0, g_viewWidth, g_viewHeight);
    Graphics3D::clearDepth();
    Graphics3D::setCamera(g_cameraTransform, g_projection);
}
void Renderer3D::project(Arr<float>& v) {
    auto inv = Graphics3D::invert(g_cameraTransform);
    Graphics3D::transform(inv, v);
    Graphics3D::transform(g_projection, v);
    v[0] = (((0.5f * (float)g_viewWidth) * v[0]) / v[3]) + (float)(g_viewWidth >> 1);
    v[1] = (((-0.5f * (float)g_viewHeight) * v[1]) / v[3]) + (float)(g_viewHeight >> 1);
}
void Renderer3D::preloadModels(const Arr<int>& ids, int resId) {
    for (int k = 0; k < ids.length; k++) getModel(ids[k], resId, true);
}
void Renderer3D::endFrame() { Graphics3D::releaseTarget(); }
void Renderer3D::setupCamera(int top, int width, int height) {
    g_viewTop = top; g_viewWidth = width; g_viewHeight = height;
    g_aspect = (((float)g_viewWidth / (float)(g_viewHeight - top)) * 0.7f) + 0.3f;
    g_aspectInv = (((float)(g_viewHeight - top) / (float)g_viewWidth) * 0.7f) + 0.3f;
    setFov(60.0f);
    g_cameraTransform = Graphics3D::identity();
}
void Renderer3D::setClip(Graphics g) { g.setClip(0, g_viewTop, g_viewWidth, g_viewHeight - g_viewTop); }

} // namespace cbx
