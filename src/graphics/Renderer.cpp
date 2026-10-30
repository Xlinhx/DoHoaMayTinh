#include "Renderer.h"
#include <algorithm>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <stb_image_write.h>
#include <stdexcept>
namespace claw {
static unsigned compile(GLenum type, const std::filesystem::path &file) {
    auto source = readText(file);
    const char *p = source.c_str();
    unsigned shader = glCreateShader(type);
    glShaderSource(shader, 1, &p, nullptr);
    glCompileShader(shader);
    int good = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &good);
    if (!good) {
        char log[8192];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile " + file.string() + ": " + log);
    }
    return shader;
}
static unsigned program(const std::filesystem::path &root, const char *name) {
    unsigned v = compile(GL_VERTEX_SHADER, root / (std::string(name) + ".vert")), f = 0, p = 0;
    try {
        f = compile(GL_FRAGMENT_SHADER, root / (std::string(name) + ".frag"));
        p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        int good = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &good);
        if (!good) {
            char log[8192];
            glGetProgramInfoLog(p, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("Shader link ") + name + ": " + log);
        }
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    } catch (...) {
        glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        if (p)
            glDeleteProgram(p);
        throw;
    }
}
static void mat4(unsigned p, const char *name, const glm::mat4 &v) {
    glUniformMatrix4fv(glGetUniformLocation(p, name), 1, GL_FALSE, glm::value_ptr(v));
}
static void integer(unsigned p, const char *name, int v) {
    glUniform1i(glGetUniformLocation(p, name), v);
}
static void scalar(unsigned p, const char *name, float v) {
    glUniform1f(glGetUniformLocation(p, name), v);
}
static void vector3(unsigned p, const char *name, glm::vec3 v) {
    glUniform3fv(glGetUniformLocation(p, name), 1, glm::value_ptr(v));
}
Renderer::Renderer(const SceneAsset &a, const std::filesystem::path &root) : asset_(a) {
    try {
        lit_ = program(root, "lit");
        depth_ = program(root, "depth");
        for (auto &mesh : a.meshes) {
            meshes_.emplace_back();
            for (auto &prim : mesh.primitives) {
                meshes_.back().push_back({});
                auto &gpu = meshes_.back().back();
                gpu.count = int(prim.indices.size());
                gpu.material = prim.material;
                glGenVertexArrays(1, &gpu.vao);
                glGenBuffers(1, &gpu.vbo);
                glGenBuffers(1, &gpu.ebo);
                glBindVertexArray(gpu.vao);
                glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
                glBufferData(GL_ARRAY_BUFFER, prim.vertices.size() * sizeof(Vertex), prim.vertices.data(),
                             GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, prim.indices.size() * sizeof(unsigned),
                             prim.indices.data(), GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                      (void *)offsetof(Vertex, position));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                      (void *)offsetof(Vertex, normal));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, uv));
            }
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (auto &im : a.images) {
            if (im.components != 3 && im.components != 4)
                throw std::runtime_error("Expected RGB/RGBA base color texture");
            textures_.push_back(0);
            glGenTextures(1, &textures_.back());
            glBindTexture(GL_TEXTURE_2D, textures_.back());
            glTexImage2D(GL_TEXTURE_2D, 0, im.components == 4 ? GL_SRGB8_ALPHA8 : GL_SRGB8, im.width,
                         im.height, 0, im.components == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                         im.pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
        glGenFramebuffers(1, &shadowFbo_);
        glGenTextures(1, &shadowMap_);
        glBindTexture(GL_TEXTURE_2D, shadowMap_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 2048, 2048, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[] = {1, 1, 1, 1};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Shadow framebuffer is incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } catch (...) {
        cleanup();
        throw;
    }
}
void Renderer::cleanup() {
    for (auto &mesh : meshes_)
        for (auto &p : mesh) {
            glDeleteVertexArrays(1, &p.vao);
            glDeleteBuffers(1, &p.vbo);
            glDeleteBuffers(1, &p.ebo);
        }
    for (auto t : textures_)
        glDeleteTextures(1, &t);
    if (shadowMap_)
        glDeleteTextures(1, &shadowMap_);
    if (shadowFbo_)
        glDeleteFramebuffers(1, &shadowFbo_);
    if (lit_)
        glDeleteProgram(lit_);
    if (depth_)
        glDeleteProgram(depth_);
}
Renderer::~Renderer() {
    cleanup();
}
void Renderer::draw(const Scene &scene, const Camera &camera, int width, int height,
                    const RenderOptions &options, unsigned framebuffer) {
    if (width <= 0 || height <= 0)
        return;
    glm::mat4 lightVP = glm::ortho(-1.65f, 1.65f, -1.65f, 1.65f, .5f, 12.f) *
                        glm::lookAt(glm::vec3(3, 5, 4), glm::vec3(0, .8f, 0), glm::vec3(0, 1, 0));
    struct Item {
        const GpuPrimitive *gpu;
        glm::mat4 world;
        float distance;
        bool studio;
    };
    std::vector<Item> opaque, glass;
    for (size_t i = 0; i < scene.nodes.size(); i++) {
        auto &node = scene.nodes[i];
        if (node.mesh < 0)
            continue;
        if (std::find(options.hide.begin(), options.hide.end(), node.group) != options.hide.end())
            continue;
        if (options.isolate == "claw" && node.group != "head" && node.name.rfind("prize_03", 0) != 0)
            continue;
        if (options.isolate == "prize" && node.name.rfind("prize_03", 0) != 0)
            continue;
        auto world = scene.worldMatrix(int(i));
        for (auto &p : meshes_.at(node.mesh)) {
            auto &material = asset_.materials.at(p.material);
            Item item{&p, world, glm::length2(glm::vec3(world[3]) - camera.eye), node.group == "studio"};
            if (material.transparent) {
                if (options.glass)
                    glass.push_back(item);
            } else
                opaque.push_back(item);
        }
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    if (options.shadow) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, 2048, 2048);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(depth_);
        mat4(depth_, "lightVP", lightVP);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.f);
        for (auto &item : opaque) {
            if (item.studio)
                continue;
            mat4(depth_, "model", item.world);
            glBindVertexArray(item.gpu->vao);
            glDrawElements(GL_TRIANGLES, item.gpu->count, GL_UNSIGNED_INT, nullptr);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    const glm::vec3 studioBackground(.85f, .85f, .82f);
    glClearColor(studioBackground.r, studioBackground.g, studioBackground.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(lit_);
    mat4(lit_, "view", camera.view());
    mat4(lit_, "projection", camera.projection(width, height));
    mat4(lit_, "lightVP", lightVP);
    vector3(lit_, "eye", camera.eye);
    vector3(lit_, "studioBackground", studioBackground);
    integer(lit_, "enableShadow", options.shadow);
    integer(lit_, "enableLight", options.lit);
    integer(lit_, "colorTexture", 0);
    integer(lit_, "shadowTexture", 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap_);
    auto drawItem = [&](const Item &item) {
        auto &p = *item.gpu;
        auto &m = asset_.materials.at(p.material);
        mat4(lit_, "model", item.world);
        glUniform4fv(glGetUniformLocation(lit_, "baseColor"), 1, glm::value_ptr(m.color));
        vector3(lit_, "emission", m.emission);
        scalar(lit_, "roughness", m.roughness);
        scalar(lit_, "metallic", m.metallic);
        integer(lit_, "isGlass", m.transparent);
        integer(lit_, "isStudio", item.studio);
        integer(lit_, "hasTexture", options.texture && m.texture >= 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m.texture >= 0 ? textures_.at(m.texture) : 0);
        glBindVertexArray(p.vao);
        glDrawElements(GL_TRIANGLES, p.count, GL_UNSIGNED_INT, nullptr);
    };
    for (auto &item : opaque)
        drawItem(item);
    std::sort(glass.begin(), glass.end(),
              [](const Item &a, const Item &b) { return a.distance > b.distance; });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (auto &item : glass)
        drawItem(item);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
    auto error = glGetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error("OpenGL draw error: " + std::to_string(error));
}
void Renderer::capture(const std::filesystem::path &path, int width, int height) {
    struct Resolve {
        int source = 0;
        unsigned fbo = 0, color = 0;
        Resolve(int w, int h) {
            int samples = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &source);
            glGetIntegerv(GL_SAMPLES, &samples);
            if (samples < 2)
                return;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
            glGenRenderbuffers(1, &color);
            glBindRenderbuffer(GL_RENDERBUFFER, color);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
            glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        }
        ~Resolve() {
            glBindFramebuffer(GL_FRAMEBUFFER, source);
            if (color)
                glDeleteRenderbuffers(1, &color);
            if (fbo)
                glDeleteFramebuffers(1, &fbo);
        }
    } resolve(width, height);
    std::vector<unsigned char> bytes(size_t(width) * height * 3);
    int fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    glReadBuffer(fbo ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, bytes.data());
    std::vector<unsigned char> flipped(bytes.size());
    size_t stride = size_t(width) * 3;
    for (int y = 0; y < height; y++)
        std::copy_n(bytes.data() + size_t(height - y - 1) * stride, stride,
                    flipped.data() + size_t(y) * stride);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Cannot write capture: " + path.string());
    auto callback = [](void *c, void *data, int size) {
        static_cast<std::ostream *>(c)->write(static_cast<char *>(data), size);
    };
    if (!stbi_write_png_to_func(callback, &stream, width, height, 3, flipped.data(), width * 3))
        throw std::runtime_error("PNG encode failed");
}
} // namespace claw
