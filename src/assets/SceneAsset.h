#pragma once
#include "../core.h"

namespace claw {

struct Vertex {
    glm::vec3 position{0};
    glm::vec3 normal{0, 1, 0};
    glm::vec2 uv{0};
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
    float roughness = .5f;
    float metallic = 0;
    int texture = -1;
    glm::vec3 emission{0};
    bool transparent = false;
};

struct Image {
    int width = 0;
    int height = 0;
    int components = 0;
    std::vector<unsigned char> pixels;
};

struct SceneAsset {
    Scene scene;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Image> images;
};

} // namespace claw
