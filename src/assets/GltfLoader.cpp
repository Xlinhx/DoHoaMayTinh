#include "GltfLoader.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <cstring>
#include <stdexcept>
#include <tiny_gltf.h>
namespace claw {
SceneAsset GltfLoader::load(const std::filesystem::path &path) {
    auto bytes = readText(path);
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromMemory(&model, &err, &warn,
                                     reinterpret_cast<const unsigned char *>(bytes.data()),
                                     unsigned(bytes.size()), path.parent_path().string()))
        throw std::runtime_error("glTF " + path.string() + ": " + err);
    SceneAsset out;
    for (auto &n : model.nodes) {
        Node node;
        node.name = n.name;
        node.mesh = n.mesh;
        if (n.extras.Has("group"))
            node.group = n.extras.Get("group").Get<std::string>();
        if (n.matrix.size() == 16)
            throw std::runtime_error("glTF matrix nodes require TRS export: " + n.name);
        if (n.translation.size() == 3)
            node.local.translation = {n.translation[0], n.translation[1], n.translation[2]};
        if (n.rotation.size() == 4)
            node.local.rotation = {float(n.rotation[3]), float(n.rotation[0]), float(n.rotation[1]),
                                   float(n.rotation[2])};
        if (n.scale.size() == 3)
            node.local.scale = {n.scale[0], n.scale[1], n.scale[2]};
        out.scene.nodes.push_back(node);
    }
    for (size_t i = 0; i < model.nodes.size(); i++)
        for (int child : model.nodes[i].children) {
            if (child < 0 || child >= int(out.scene.nodes.size()))
                throw std::runtime_error("glTF invalid child");
            out.scene.nodes[child].parent = int(i);
        }
    auto address = [&](int accessor, size_t index, size_t bytesNeeded) -> const unsigned char * {
        auto &a = model.accessors.at(accessor);
        auto &v = model.bufferViews.at(a.bufferView);
        auto &b = model.buffers.at(v.buffer);
        size_t stride = a.ByteStride(v), offset = v.byteOffset + a.byteOffset + index * stride;
        if (index >= a.count || offset + bytesNeeded > b.data.size() || a.sparse.isSparse)
            throw std::runtime_error("glTF invalid/unsupported accessor");
        return b.data.data() + offset;
    };
    for (auto &m : model.meshes) {
        Mesh mesh;
        mesh.name = m.name;
        for (auto &p : m.primitives) {
            if (p.mode != TINYGLTF_MODE_TRIANGLES && p.mode != -1)
                throw std::runtime_error("glTF non-triangle primitive");
            Primitive prim;
            prim.material = p.material;
            int pa = p.attributes.at("POSITION");
            auto &access = model.accessors.at(pa);
            if (access.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
                throw std::runtime_error("glTF position must be FLOAT");
            int na = p.attributes.count("NORMAL") ? p.attributes.at("NORMAL") : -1,
                uv = p.attributes.count("TEXCOORD_0") ? p.attributes.at("TEXCOORD_0") : -1;
            for (size_t i = 0; i < access.count; i++) {
                Vertex v{};
                std::memcpy(&v.position, address(pa, i, 12), 12);
                if (na >= 0)
                    std::memcpy(&v.normal, address(na, i, 12), 12);
                else
                    v.normal = {0, 1, 0};
                if (uv >= 0)
                    std::memcpy(&v.uv, address(uv, i, 8), 8);
                prim.vertices.push_back(v);
            }
            if (p.indices >= 0) {
                auto &a = model.accessors.at(p.indices);
                for (size_t i = 0; i < a.count; i++) {
                    unsigned value = 0;
                    int type = a.componentType;
                    if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        std::memcpy(&value, address(p.indices, i, 4), 4);
                    else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        unsigned short x;
                        std::memcpy(&x, address(p.indices, i, 2), 2);
                        value = x;
                    } else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        value = *address(p.indices, i, 1);
                    else
                        throw std::runtime_error("glTF invalid index component");
                    if (value >= prim.vertices.size())
                        throw std::runtime_error("glTF index out of range");
                    prim.indices.push_back(value);
                }
            } else
                for (unsigned i = 0; i < prim.vertices.size(); i++)
                    prim.indices.push_back(i);
            mesh.primitives.push_back(std::move(prim));
        }
        out.meshes.push_back(std::move(mesh));
    }
    for (auto &m : model.materials) {
        Material mat;
        mat.name = m.name;
        auto &p = m.pbrMetallicRoughness;
        mat.color = {p.baseColorFactor[0], p.baseColorFactor[1], p.baseColorFactor[2], p.baseColorFactor[3]};
        mat.roughness = float(p.roughnessFactor);
        mat.metallic = float(p.metallicFactor);
        mat.transparent = m.alphaMode == "BLEND";
        if (p.baseColorTexture.index >= 0)
            mat.texture = model.textures.at(p.baseColorTexture.index).source;
        if (m.emissiveFactor.size() == 3)
            mat.emission = {m.emissiveFactor[0], m.emissiveFactor[1], m.emissiveFactor[2]};
        out.materials.push_back(mat);
    }
    for (auto &i : model.images) {
        if (i.image.empty() || i.bits != 8)
            throw std::runtime_error("glTF missing/unsupported image: " + i.name);
        out.images.push_back({i.width, i.height, i.component, i.image});
    }
    for (auto &n : out.scene.nodes)
        if (n.mesh >= int(out.meshes.size()))
            throw std::runtime_error("glTF invalid mesh node");
    for (size_t i = 0; i < out.scene.nodes.size(); i++)
        out.scene.worldMatrix(int(i));
    return out;
}
} // namespace claw
