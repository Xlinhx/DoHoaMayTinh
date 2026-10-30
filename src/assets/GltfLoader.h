#pragma once
#include "core.h"
namespace claw {
struct Vertex {
    glm::vec3 position, normal;
    glm::vec2 uv;
};
struct Primitive {
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    int material = -1;
};
struct Mesh {
    std::string name;
    std::vector<Primitive> primitives;
};
struct Material {
    std::string name;
    glm::vec4 color{1};
    float roughness = .5f, metallic = 0;
    int texture = -1;
    glm::vec3 emission{0};
    bool transparent = false;
};
struct Image {
    int width = 0, height = 0, components = 0;
    std::vector<unsigned char> pixels;
};
struct SceneAsset {
    Scene scene;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Image> images;
};
struct GltfLoader {
    static SceneAsset load(const std::filesystem::path &);
};
} // namespace claw
