#pragma once
#include "assets/GltfLoader.h"
#include <glad/gl.h>
namespace claw {
struct RenderOptions {
    bool glass = true, shadow = true, lit = true, texture = true;
    std::vector<std::string> hide;
    std::string isolate;
};
class Renderer {
    struct GpuPrimitive {
        unsigned vao = 0, vbo = 0, ebo = 0;
        int count = 0, material = -1;
    };
    std::vector<std::vector<GpuPrimitive>> meshes_;
    std::vector<unsigned> textures_;
    const SceneAsset &asset_;
    unsigned lit_ = 0, depth_ = 0, shadowFbo_ = 0, shadowMap_ = 0;
    void cleanup();

  public:
    Renderer(const SceneAsset &, const std::filesystem::path &shaderRoot);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void draw(const Scene &, const Camera &, int width, int height, const RenderOptions &options = {},
              unsigned framebuffer = 0);
    void capture(const std::filesystem::path &, int width, int height);
};
} // namespace claw
