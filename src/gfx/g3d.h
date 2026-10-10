// g3d.h - OBJ/MTL model loader and the small software 3D rasteriser used by the game.
#pragma once
#include "graphics.h"
#include <map>
#include <memory>
#include <vector>

namespace cbx {

struct Tex { int w = 0, h = 0; std::vector<uint32_t> argb; };

struct ModelSub {
    std::vector<int> tris;
    std::shared_ptr<Tex> tex;
    int blending = 68;            // BLEND_REPLACE
    int alphaThreshold = 0;
    bool depthTest = true, depthWrite = true;
    int culling = 160;            // CULL_BACK
    bool windingCCW = true;
    int texFunc = 227;            // FUNC_MODULATE
    bool clamp = false;
};

struct Model {
    static constexpr int BLEND_ALPHA = 64, BLEND_REPLACE = 68;
    static constexpr int FUNC_REPLACE = 228, FUNC_MODULATE = 227;
    static constexpr int CULL_BACK = 160, CULL_FRONT = 161, CULL_NONE = 162;

    int userId = 0;
    int vertexCount = 0;
    std::vector<float> pos, uv;       // uv empty if absent
    std::vector<int> colors;
    int defaultColor = (int)0xFFFFFFFF;
    std::vector<ModelSub> subs;

    // Returns the model registered under the given numeric id in assets/models/models.txt (loaded from its
    // Wavefront OBJ/MTL + PNG textures on first use, then cached), or nullptr.
    static std::shared_ptr<Model> find(int userId);
    void setupAppearance() { for (auto& s : subs) { s.texFunc = FUNC_REPLACE; s.clamp = true; } }
};

class Graphics3D {
public:
    static std::vector<float> identity();
    static std::vector<float> mul(const std::vector<float>& a, const std::vector<float>& b);
    static std::vector<float> invert(const std::vector<float>& m);
    static std::vector<float> translation(float x, float y, float z);
    static std::vector<float> rotation(float angleDeg, float ax, float ay, float az);
    static std::vector<float> perspective(float fovyDeg, float aspect, float nearP, float farP);
    static void transform(const std::vector<float>& m, Arr<float>& v);

    static void setCamera(const std::vector<float>& cameraToWorld, const std::vector<float>& projection);
    static void bindTarget(const Graphics& g, int x, int y, int w, int h);
    static void releaseTarget();
    static void clearDepth();
    static void flushPending();
    static const void* pendingId();
    static void render(const Model& m, const std::vector<float>& model);
};

// A model instance with its own transform.
class Mesh3D {
public:
    Mesh3D() = default;
    Mesh3D(NullT) {}
    explicit Mesh3D(std::shared_ptr<Model> m);
    bool operator==(NullT) const { return !d_; }
    bool operator!=(NullT) const { return (bool)d_; }
    void render();
    void translate(float dx, float dy, float dz);
    void rotate(float angleFraction, float ax, float ay, float az);
    void setupAppearance();
    void resetTransform();
private:
    struct Impl { std::shared_ptr<Model> mesh; std::vector<float> transform; };
    std::shared_ptr<Impl> d_;
    Impl& I() const { if (!d_) throw NullPointerException(); return *d_; }
};

class Renderer3D {
public:
    static Arr<float> projected;
    static Mesh3D getModel(int id, int resId, bool load);
    static void setFov(float fovDegrees);
    static void lookAt(float eyeX, float eyeY, float eyeZ, float dirX, float dirY, float dirZ, float upX, float upY, float upZ);
    static void init(int top, int width, int height);
    static void beginFrame(const Graphics& g);
    static void project(Arr<float>& vertexCoords);
    static void preloadModels(const Arr<int>& ids, int resId);
    static void endFrame();
private:
    static void setupCamera(int top, int width, int height);
    static void setClip(Graphics g);
};

} // namespace cbx
