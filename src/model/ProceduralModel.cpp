#include "ProceduralModel.h"

#include <array>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace claw {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kTeal = 0;
constexpr int kDark = 1;
constexpr int kCream = 2;
constexpr int kSteel = 3;
constexpr int kOrange = 4;
constexpr int kGlass = 5;
constexpr int kLed = 6;
constexpr int kSeam = 7;
constexpr int kPrize = 8;

enum class Shape { Box, Cylinder, Sphere, Torus, Finger };

Primitive makeBox(int material) {
    Primitive p;
    p.material = material;
    const glm::vec3 positions[6][4] = {
        {{-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}},
        {{.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}},
        {{-.5f, -.5f, -.5f}, {-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}},
        {{.5f, -.5f, .5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}},
        {{-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}},
        {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {-.5f, -.5f, .5f}}
    };
    const glm::vec3 normals[6] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0},
                                   {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int face = 0; face < 6; ++face) {
        unsigned base = static_cast<unsigned>(p.vertices.size());
        for (int v = 0; v < 4; ++v)
            p.vertices.push_back({positions[face][v], normals[face], uv[v]});
        p.indices.insert(p.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return p;
}

Primitive makeSphere(int material, int slices = 24, int stacks = 16) {
    Primitive p;
    p.material = material;
    for (int y = 0; y <= stacks; ++y) {
        float v = float(y) / float(stacks);
        float phi = v * kPi;
        for (int x = 0; x <= slices; ++x) {
            float u = float(x) / float(slices);
            float theta = u * 2.f * kPi;
            glm::vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi),
                        std::sin(phi) * std::sin(theta));
            p.vertices.push_back({n, glm::normalize(n), {u, v}});
        }
    }
    for (int y = 0; y < stacks; ++y) {
        for (int x = 0; x < slices; ++x) {
            unsigned a = static_cast<unsigned>(y * (slices + 1) + x);
            unsigned b = a + static_cast<unsigned>(slices + 1);
            p.indices.insert(p.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    return p;
}

Primitive makeCylinder(int material, int segments = 24) {
    Primitive p;
    p.material = material;
    // Unit cylinder: radius 1, height 1, axis Y.
    for (int i = 0; i <= segments; ++i) {
        float a = 2.f * kPi * float(i) / float(segments);
        float x = std::cos(a), z = std::sin(a);
        p.vertices.push_back({{x, -.5f, z}, {x, 0, z}, {float(i) / segments, 0}});
        p.vertices.push_back({{x, .5f, z}, {x, 0, z}, {float(i) / segments, 1}});
    }
    for (int i = 0; i < segments; ++i) {
        unsigned a = static_cast<unsigned>(i * 2);
        p.indices.insert(p.indices.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2});
    }
    // Flat end caps use their own vertices/normals.
    for (int cap = 0; cap < 2; ++cap) {
        float y = cap == 0 ? -.5f : .5f;
        glm::vec3 normal(0, cap == 0 ? -1.f : 1.f, 0);
        unsigned center = static_cast<unsigned>(p.vertices.size());
        p.vertices.push_back({{0, y, 0}, normal, {.5f, .5f}});
        unsigned ring = static_cast<unsigned>(p.vertices.size());
        for (int i = 0; i <= segments; ++i) {
            float a = 2.f * kPi * float(i) / float(segments);
            float x = std::cos(a), z = std::sin(a);
            p.vertices.push_back({{x, y, z}, normal, {x * .5f + .5f, z * .5f + .5f}});
        }
        for (int i = 0; i < segments; ++i) {
            unsigned a = ring + static_cast<unsigned>(i);
            if (cap == 0)
                p.indices.insert(p.indices.end(), {center, a + 1, a});
            else
                p.indices.insert(p.indices.end(), {center, a, a + 1});
        }
    }
    return p;
}

Primitive makeTorus(int material, int majorSegments = 48, int minorSegments = 8) {
    Primitive p;
    p.material = material;
    constexpr float majorRadius = .94f;
    constexpr float minorRadius = .06f;
    for (int i = 0; i <= majorSegments; ++i) {
        float u = float(i) / float(majorSegments);
        float a = u * 2.f * kPi;
        for (int j = 0; j <= minorSegments; ++j) {
            float v = float(j) / float(minorSegments);
            float b = v * 2.f * kPi;
            float ring = majorRadius + minorRadius * std::cos(b);
            glm::vec3 position(ring * std::cos(a), minorRadius * std::sin(b),
                               ring * std::sin(a));
            glm::vec3 normal(std::cos(b) * std::cos(a), std::sin(b),
                             std::cos(b) * std::sin(a));
            p.vertices.push_back({position, glm::normalize(normal), {u, v}});
        }
    }
    for (int i = 0; i < majorSegments; ++i) {
        for (int j = 0; j < minorSegments; ++j) {
            unsigned a = static_cast<unsigned>(i * (minorSegments + 1) + j);
            unsigned b = a + static_cast<unsigned>(minorSegments + 1);
            p.indices.insert(p.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    return p;
}

// Tính một điểm trên đường cong Bézier bậc 3
glm::vec2 cubicBezier(
    const glm::vec2& p0,
    const glm::vec2& p1,
    const glm::vec2& p2,
    const glm::vec2& p3,
    float t
) {
    float u = 1.0f - t;

    return u * u * u * p0
         + 3.0f * u * u * t * p1
         + 3.0f * u * t * t * p2
         + t * t * t * p3;
}

Primitive makeFinger(int material) {
    Primitive p;
    p.material = material;

    constexpr int radialSegments = 10;
    constexpr float radiusXY = 0.00713f;
    constexpr float radiusZ = 0.0075f;

    // Số đoạn chia trên mỗi đường Bézier
    constexpr int samplesPerSegment = 12;

    // Hai đoạn Bézier, dùng chung điểm nối
    constexpr int ringCount = 2 * samplesPerSegment + 1;

    // Lưu các điểm trên đường tâm càng
    std::array<glm::vec2, ringCount> centerline{};

    // ==========================================
    // ĐOẠN BEZIER 1: TỪ KHỚP GẮP CONG RA NGOÀI
    // ==========================================

    const glm::vec2 A0(0.000f,  0.000f);
    const glm::vec2 A1(0.027f, -0.049f);
    const glm::vec2 A2(0.066f, -0.069f);
    const glm::vec2 A3(0.070f, -0.125f);

    // Sinh 13 điểm trên đoạn Bézier thứ nhất
    for (int i = 0; i <= samplesPerSegment; ++i) {
        float t = float(i) / samplesPerSegment;

        centerline[i] = cubicBezier(A0, A1, A2, A3, t);
    }

    // ==========================================
    // ĐOẠN BEZIER 2: CONG NGƯỢC VÀO TRONG
    // ==========================================

    const glm::vec2 B0(0.070f, -0.125f);
    const glm::vec2 B1(0.074f, -0.179f);
    const glm::vec2 B2(0.040f, -0.204f);
    const glm::vec2 B3(0.020f, -0.255f);

    // Sinh thêm 12 điểm, không lặp điểm nối B0
    for (int i = 1; i <= samplesPerSegment; ++i) {
        float t = float(i) / samplesPerSegment;

        centerline[samplesPerSegment + i] =
            cubicBezier(B0, B1, B2, B3, t);
    }

    // Build one ring perpendicular to the local centerline at each sample.
    // The section is circular and stays smooth rather than looking like a
    // rectangular tapered bar.
    for (int ring = 0; ring < ringCount; ++ring) {
        glm::vec2 tangent;
        if (ring == 0)
            tangent = centerline[1] - centerline[0];
        else if (ring == ringCount - 1)
            tangent = centerline[ring] - centerline[ring - 1];
        else
            tangent = centerline[ring + 1] - centerline[ring - 1];
        tangent = glm::normalize(tangent);
        const glm::vec2 normalXY = glm::normalize(glm::vec2(-tangent.y, tangent.x));

        const float v = float(ring) / float(ringCount - 1);
        for (int side = 0; side <= radialSegments; ++side) {
            const float angle = 2.f * kPi * float(side) / float(radialSegments);
            const float c = std::cos(angle);
            const float sn = std::sin(angle);
            const glm::vec3 normal = glm::normalize(
                glm::vec3(normalXY.x * c, normalXY.y * c, sn));
            const glm::vec3 position(
                centerline[ring].x + normalXY.x * radiusXY * c,
                centerline[ring].y + normalXY.y * radiusXY * c,
                radiusZ * sn);
            p.vertices.push_back({position, normal,
                                  {float(side) / float(radialSegments), v}});
        }
    }

    // Join adjacent rings. Winding is outward-facing for the right-handed
    // local frame (tangent, normalXY, +Z).
    constexpr unsigned ringStride = radialSegments + 1;
    for (int ring = 0; ring < ringCount - 1; ++ring) {
        for (int side = 0; side < radialSegments; ++side) {
            const unsigned a = unsigned(ring) * ringStride + unsigned(side);
            const unsigned b = a + 1;
            const unsigned d = unsigned(ring + 1) * ringStride + unsigned(side);
            const unsigned c = d + 1;
            p.indices.insert(p.indices.end(), {a, b, c, a, c, d});
        }
    }

    // Close both ends of the swept finger so it has no open tube ends.
    for (int end = 0; end < 2; ++end) {
        const int ring = end == 0 ? 0 : ringCount - 1;
        glm::vec2 tangent = end == 0
            ? glm::normalize(centerline[1] - centerline[0])
            : glm::normalize(centerline[ringCount - 1] - centerline[ringCount - 2]);
        glm::vec3 capNormal(tangent.x, tangent.y, 0.f);
        if (end == 0)
            capNormal = -capNormal;

        const unsigned centerIndex = static_cast<unsigned>(p.vertices.size());
        p.vertices.push_back({glm::vec3(centerline[ring], 0.f), capNormal,
                              {0.5f, 0.5f}});
        const unsigned first = static_cast<unsigned>(ring) * ringStride;
        for (int side = 0; side < radialSegments; ++side) {
            const unsigned a = first + static_cast<unsigned>(side);
            const unsigned b = a + 1;
            if (end == 0)
                p.indices.insert(p.indices.end(), {centerIndex, b, a});
            else
                p.indices.insert(p.indices.end(), {centerIndex, a, b});
        }
    }
    return p;
}

class ModelBuilder {
    SceneAsset asset_;
    std::map<std::pair<Shape, int>, int> meshCache_;

    int mesh(Shape shape, int material) {
        auto key = std::make_pair(shape, material);
        auto found = meshCache_.find(key);
        if (found != meshCache_.end())
            return found->second;
        Primitive primitive;
        switch (shape) {
        case Shape::Box: primitive = makeBox(material); break;
        case Shape::Cylinder: primitive = makeCylinder(material); break;
        case Shape::Sphere: primitive = makeSphere(material); break;
        case Shape::Torus: primitive = makeTorus(material); break;
        case Shape::Finger: primitive = makeFinger(material); break;
        }
        Mesh m;
        m.name = "procedural_" + std::to_string(static_cast<int>(shape)) + "_mat_" + std::to_string(material);
        m.primitives.push_back(std::move(primitive));
        int index = static_cast<int>(asset_.meshes.size());
        asset_.meshes.push_back(std::move(m));
        meshCache_[key] = index;
        return index;
    }

  public:
    ModelBuilder() {
        // Palette translated from the design specification. Textures are intentionally unused.
        asset_.materials = {
            {"teal",   {0.009721f, 0.149960f, 0.135633f, 1.f}, .35f, 0.f, -1, {0, 0, 0}, false},
            {"dark",   {0.008568f, 0.043735f, 0.042311f, 1.f}, .55f, 0.f, -1, {0, 0, 0}, false},
            {"cream",  {0.896269f, 0.830770f, 0.723055f, 1.f}, .42f, 0.f, -1, {0, 0, 0}, false},
            {"steel",  {0.467784f, 0.539479f, 0.539479f, 1.f}, .24f, .78f, -1, {0, 0, 0}, false},
            {"orange", {0.846873f, 0.184475f, 0.046665f, 1.f}, .36f, 0.f, -1, {0, 0, 0}, false},
            {"glass",  {0.623960f, 0.791298f, 0.737911f, .065f}, .12f, 0.f, -1, {0, 0, 0}, true},
            {"led",    {1.f, 0.760525f, 0.391573f, 1.f}, .40f, 0.f, -1, {0.35f, 0.23f, 0.08f}, false},
            {"seam",   {0.396755f, 0.116971f, 0.082283f, 1.f}, .90f, 0.f, -1, {0, 0, 0}, false},
            {"prize",  {0.807f, 0.456f, 0.381f, 1.f}, .92f, 0.f, -1, {0, 0, 0}, false}
        };
    }

    int node(const std::string &name, const std::string &group, int parent,
             int meshIndex, const glm::vec3 &position = glm::vec3(0),
             const glm::vec3 &scale = glm::vec3(1),
             const glm::quat &rotation = glm::quat(1, 0, 0, 0)) {
        Node n;
        n.name = name;
        n.group = group;
        n.parent = parent;
        n.mesh = meshIndex;
        n.local.translation = position;
        n.local.scale = scale;
        n.local.rotation = rotation;
        int index = static_cast<int>(asset_.scene.nodes.size());
        asset_.scene.nodes.push_back(std::move(n));
        return index;
    }

    int emptyNode(const std::string &name, const std::string &group, int parent,
                  const glm::vec3 &position = glm::vec3(0),
                  const glm::quat &rotation = glm::quat(1, 0, 0, 0)) {
        return node(name, group, parent, -1, position, glm::vec3(1), rotation);
    }

    int box(const std::string &name, const std::string &group, int parent, int material,
            const glm::vec3 &position, const glm::vec3 &size,
            const glm::quat &rotation = glm::quat(1, 0, 0, 0)) {
        return node(name, group, parent, mesh(Shape::Box, material), position, size, rotation);
    }

    int cylinder(const std::string &name, const std::string &group, int parent, int material,
                 const glm::vec3 &position, float radius, float height,
                 const glm::quat &rotation = glm::quat(1, 0, 0, 0)) {
        return node(name, group, parent, mesh(Shape::Cylinder, material), position,
                    glm::vec3(radius, height, radius), rotation);
    }

    int sphere(const std::string &name, const std::string &group, int parent, int material,
               const glm::vec3 &position, const glm::vec3 &radii) {
        return node(name, group, parent, mesh(Shape::Sphere, material), position, radii);
    }

    void torus(const std::string &name, const std::string &group, int parent, int material,
               const glm::vec3 &position, const glm::vec3 &scale) {
        node(name, group, parent, mesh(Shape::Torus, material), position, scale);
    }

    void buildCabinet() {
        // Lower housing and the four structural sides.
        box("base_plinth", "body", -1, kDark, {0, .065f, 0}, {.90f, .09f, .76f});
        box("base_back", "body", -1, kTeal, {0, .365f, -.354f}, {.86f, .55f, .045f});
        box("base_left", "body", -1, kTeal, {-.424f, .365f, 0}, {.045f, .55f, .71f});
        box("base_right", "body", -1, kTeal, {.424f, .365f, 0}, {.045f, .55f, .71f});
        box("front_post_left", "body", -1, kCream, {-.424f, .365f, .355f}, {.05f, .55f, .05f});
        box("front_post_right", "body", -1, kCream, {.424f, .365f, .355f}, {.05f, .55f, .05f});

        // Cream rails around the lower edge of the display chamber.
        box("bottom_frame_back", "body", -1, kCream, {0, .662f, -.355f}, {.90f, .07f, .05f});
        box("bottom_frame_front", "body", -1, kCream, {0, .662f, .355f}, {.90f, .07f, .05f});
        box("bottom_frame_left", "body", -1, kCream, {-.425f, .662f, 0}, {.05f, .07f, .66f});
        box("bottom_frame_right", "body", -1, kCream, {.425f, .662f, 0}, {.05f, .07f, .66f});

        // The floor is built as four pieces, leaving the prize-drop opening clear.
        box("floor_back_left", "body", -1, kCream, {-.27f, .682f, -.125f}, {.26f, .036f, .41f});
        box("floor_front_rim", "body", -1, kCream, {-.27f, .682f, .315f}, {.26f, .036f, .03f});
        box("floor_left_rim", "body", -1, kCream, {-.385f, .682f, .19f}, {.03f, .036f, .22f});
        box("floor_main", "body", -1, kCream, {.13f, .682f, 0}, {.54f, .036f, .66f});

        // Orange rim around the drop opening.
        box("chute_back", "body", -1, kOrange, {-.255f, .725f, .065f}, {.25f, .06f, .018f});
        box("chute_front", "body", -1, kOrange, {-.255f, .725f, .315f}, {.25f, .06f, .018f});
        box("chute_left", "body", -1, kOrange, {-.38f, .725f, .19f}, {.018f, .06f, .24f});
        box("chute_right", "body", -1, kOrange, {-.13f, .725f, .19f}, {.018f, .06f, .24f});
        box("drop_tunnel_back", "body", -1, kDark, {-.255f, .56f, .075f}, {.23f, .22f, .014f});
        box("drop_tunnel_left", "body", -1, kDark, {-.365f, .56f, .19f}, {.014f, .22f, .22f});
        box("drop_tunnel_right", "body", -1, kDark, {-.145f, .56f, .19f}, {.014f, .22f, .22f});

        // Prize receiving tray at the front-left of the cabinet.
        box("tray_floor", "body", -1, kDark, {-.255f, .217f, .30f}, {.245f, .022f, .25f});
        box("tray_backstop", "body", -1, kDark, {-.255f, .32f, .165f}, {.245f, .21f, .025f});
        box("tray_leftwall", "body", -1, kDark, {-.379f, .32f, .28f}, {.02f, .21f, .23f});
        box("tray_rightwall", "body", -1, kDark, {-.131f, .32f, .28f}, {.02f, .21f, .23f});
        box("tray_lip", "body", -1, kOrange, {-.255f, .23f, .425f}, {.27f, .055f, .035f});
        box("tray_label", "body", -1, kCream, {-.255f, .475f, .384f}, {.12f, .023f, .003f});

        // Control console and its inset plate.
        box("console", "body", -1, kCream, {0, .625f, .425f}, {.88f, .085f, .21f});
        box("console_teal_plate", "body", -1, kTeal, {.12f, .672f, .427f}, {.48f, .013f, .15f});
        box("coin_slot_plate", "body", -1, kDark, {.265f, .444f, .386f}, {.12f, .15f, .009f});
        box("coin_slot", "body", -1, kSteel, {.265f, .475f, .393f}, {.065f, .009f, .005f});
        box("coin_label", "body", -1, kCream, {.265f, .412f, .397f}, {.026f, .024f, .002f});
        cylinder("joystick_socket", "body", -1, kDark, {.04f, .686f, .434f}, .032f, .014f);
        cylinder("joystick_stem", "body", -1, kSteel, {.04f, .717f, .434f}, .008f, .06f);
        sphere("joystick_grip", "body", -1, kDark, {.04f, .753f, .434f}, {.019f, .025f, .019f});
        cylinder("grab_button_rim", "body", -1, kCream, {.26f, .687f, .434f}, .032f, .012f);
        cylinder("grab_button", "body", -1, kOrange, {.26f, .699f, .434f}, .023f, .018f);

        // Transparent windows, separated into a group so a cutaway view can hide them.
        box("glass_back", "glass", -1, kGlass, {0, 1.18f, -.354f}, {.84f, .95f, .006f});
        box("glass_front", "glass", -1, kGlass, {0, 1.18f, .354f}, {.84f, .95f, .006f});
        box("glass_left", "glass", -1, kGlass, {-.426f, 1.18f, 0}, {.006f, .95f, .70f});
        box("glass_right", "glass", -1, kGlass, {.426f, 1.18f, 0}, {.006f, .95f, .70f});
        box("glass_front_edge_left", "body", -1, kCream, {-.394f, 1.18f, .356f}, {.018f, .95f, .025f});
        box("glass_front_edge_right", "body", -1, kCream, {.394f, 1.18f, .356f}, {.018f, .95f, .025f});
        box("glass_front_top", "body", -1, kCream, {0, 1.65f, .356f}, {.82f, .025f, .025f});
        box("glass_front_bottom", "body", -1, kCream, {0, .705f, .356f}, {.82f, .025f, .025f});

        // Side/rear vents and small exterior panel details.
        for (int i = 0; i < 5; ++i)
            box("rear_vent_" + std::to_string(i), "body", -1, kDark,
                {.22f, .25f + .036f * i, -.378f}, {.24f, .009f, .003f});
        box("front_window_lower", "body", -1, kDark, {-.265f, .175f, .357f}, {.25f, .20f, .012f});
        box("front_window_upper", "body", -1, kTeal, {-.265f, .535f, .357f}, {.25f, .18f, .012f});

        // Roof cap, underside, fascia, logo plate and LED strips.
        box("roof_crown", "roof", -1, kCream, {0, 1.745f, 0}, {.90f, .11f, .76f});
        box("roof_underside", "roof", -1, kTeal, {0, 1.683f, 0}, {.81f, .018f, .67f});
        box("roof_teal_insert", "roof", -1, kTeal, {0, 1.746f, .384f}, {.69f, .071f, .012f});
        box("roof_wordmark_plate", "roof", -1, kOrange, {0, 1.747f, .394f}, {.25f, .035f, .004f});
        box("roof_led_front", "roof", -1, kLed, {0, 1.665f, .31f}, {.55f, .008f, .012f});
        box("roof_led_left", "roof", -1, kLed, {-.38f, 1.665f, 0}, {.012f, .008f, .48f});
        box("roof_led_right", "roof", -1, kLed, {.38f, 1.665f, 0}, {.012f, .008f, .48f});

        // Studio floor used by the renderer's reference captures.
        box("studio_ground", "studio", -1, kCream, {0, -.009f, 0}, {20.f, .018f, 20.f});
    }

    void buildGantry() {
        const glm::quat alongZ = glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0));
        const glm::quat alongX = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 0, 1));
        for (float x : {-0.325f, 0.325f}) {
            std::string suffix = x < 0 ? "left" : "right";
            cylinder("fixed_y_rail_" + suffix, "gantry", -1, kSteel, {x, 1.62f, 0}, .009f, .61f, alongZ);
            for (float z : {-0.30f, 0.30f})
                box("rail_mount_" + suffix + (z < 0 ? "_back" : "_front"), "gantry", -1,
                    kTeal, {x, 1.645f, z}, {.045f, .065f, .045f});
        }

        // Y-bridge slides front/back on the fixed rails.
        int bridge = emptyNode("bridge_y", "gantry", -1, {0, 0, -.10f});
        box("bridge_beam", "gantry", bridge, kTeal, {0, 1.61f, 0}, {.71f, .045f, .038f});
        cylinder("moving_x_rail", "gantry", bridge, kSteel, {0, 1.595f, .032f}, .008f, .61f, alongX);

        // The trolley is a child of the moving bridge; its X is controlled by Round::applyPose.
        int trolley = emptyNode("trolley_x", "gantry", bridge);
        box("trolley_body", "gantry", trolley, kOrange, {0, 1.57f, .015f}, {.10f, .065f, .095f});
        cylinder("trolley_wheel_left", "gantry", trolley, kDark, {-.04f, 1.62f, -.035f}, .017f, .016f, alongX);
        cylinder("trolley_wheel_right", "gantry", trolley, kDark, {.04f, 1.62f, -.035f}, .017f, .016f, alongX);
        cylinder("lift_cable", "gantry", trolley, kDark, {0, 1.52f, 0}, .004f, .10f);

        int head = emptyNode("claw_head", "head", trolley, {0, 1.46f, 0});
        sphere("claw_cap", "head", head, kSteel, {0, .025f, 0}, {.043f, .0175f, .043f});
        cylinder("claw_housing", "head", head, kTeal, {0, 0, 0}, .047f, .055f);
        cylinder("claw_housing_ring", "head", head, kOrange, {0, -.021f, 0}, .049f, .012f);

        const float azimuths[3] = {30.f, 150.f, -90.f};
        for (int i = 0; i < 3; ++i) {
            int azimuth = emptyNode("finger_azimuth_" + std::to_string(i), "head", head, glm::vec3(0),
                                    glm::angleAxis(glm::radians(azimuths[i]), glm::vec3(0, 1, 0)));
            int joint = emptyNode("finger_joint_" + std::to_string(i), "head", azimuth,
                                  {.035f, -.025f, 0});
            node("finger_" + std::to_string(i), "head", joint, mesh(Shape::Finger, kSteel));
            sphere("finger_pad_" + std::to_string(i), "head", joint, kDark,
                   {.02f, -.255f, 0}, {.012f, .014f, .012f});
            cylinder("joint_pin_" + std::to_string(i), "head", joint, kOrange,
                     {0, 0, 0}, .014f, .026f, alongX);
        }
    }

    void buildPrizes() {
        struct PrizePlacement { const char *id; float x, y, yaw; };
        const PrizePlacement placements[] = {
            {"prize_01", -.22f, -.20f, -18.f}, {"prize_02", -.01f, -.20f, 21.f},
            {"prize_03", .14f, -.06f, -12.f}, {"prize_04", .25f, -.21f, 44.f},
            {"prize_05", -.22f, -.02f, 24.f}, {"prize_06", -.04f, -.02f, -36.f},
            {"prize_07", .02f, .18f, 18.f}, {"prize_08", .25f, .16f, -26.f}
        };
        for (const auto &placement : placements) {
            glm::quat yaw = glm::angleAxis(glm::radians(placement.yaw), glm::vec3(0, 1, 0));
            int root = emptyNode(placement.id, "prize", -1,
                                 {placement.x, .762f, placement.y}, yaw);
            sphere(std::string(placement.id) + "_volume", "prize", root, kPrize,
                   {0, 0, 0}, {.073f, .062f, .062f});
            torus(std::string(placement.id) + "_seam", "prize", root, kSeam,
                  {0, -.002f, 0}, {.073f, .025f, .062f});
            // Small cream stitches distributed around the equatorial seam.
            for (int stitch = 0; stitch < 12; ++stitch) {
                float a = 2.f * kPi * float(stitch) / 12.f;
                glm::vec3 position(.073f * std::cos(a), -.002f, .062f * std::sin(a));
                sphere(std::string(placement.id) + "_stitch_" +
                           (stitch < 10 ? "0" : "") + std::to_string(stitch),
                       "prize", root, kCream, position, {.0022f, .0032f, .0022f});
            }
        }
    }

    SceneAsset finish() {
        buildCabinet();
        buildGantry();
        buildPrizes();
        for (size_t i = 0; i < asset_.scene.nodes.size(); ++i)
            asset_.scene.worldMatrix(static_cast<int>(i));
        return std::move(asset_);
    }
};

} // namespace

SceneAsset buildProceduralScene() {
    return ModelBuilder{}.finish();
}

} // namespace claw
