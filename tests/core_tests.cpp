#include "assets/GltfLoader.h"
#include "core.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
using namespace claw;
using Catch::Approx;
const std::filesystem::path root = SOURCE_ROOT;
DesignConfig design() {
    return loadDesign(root / "mockups/design-source.json");
}
TEST_CASE("resources resolve beside executable, not cwd", "[resource]") {
    CHECK(resourceRoot(root / "build/release/claw_lab.exe") == root / "build/release");
    CHECK_THROWS_WITH(readText(root / "missing.vert"),
                      "Cannot read resource: " + (root / "missing.vert").string());
}
TEST_CASE("design validates identity and bounds", "[resource]") {
    auto d = design();
    REQUIRE(d.data.contains("prizes"));
    CHECK(d.data["prizes"].size() == 8);
    auto temp = root / "build/invalid-design.json";
    auto j = d.data;
    j["prizes"][1]["id"] = "prize_01";
    {
        std::ofstream f(temp);
        f << j;
    }
    CHECK_THROWS(loadDesign(temp));
    std::filesystem::remove(temp);
}
TEST_CASE("parent rotation moves child around its pivot", "[transform]") {
    Scene s;
    Node parent;
    parent.local.rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 1, 0));
    Node child;
    child.parent = 0;
    child.local.translation = {1, 0, 0};
    s.nodes = {parent, child};
    auto p = glm::vec3(s.worldMatrix(1) * glm::vec4(0, 0, 0, 1));
    CHECK(p.x == Approx(0).margin(1e-5));
    CHECK(p.z == Approx(-1).margin(1e-5));
    CHECK(fromAuthoring({1, 2, 3}) == glm::vec3(1, 3, -2));
}
TEST_CASE("normal matrix preserves perpendicularity under nonuniform scale", "[transform]") {
    Transform t;
    t.scale = {2, 3, 4};
    auto m = localMatrix(t);
    auto n = glm::transpose(glm::inverse(glm::mat3(m))) * glm::vec3(1, 1, 0);
    CHECK(glm::dot(n, glm::mat3(m) * glm::vec3(1, -1, 0)) == Approx(0).margin(1e-5));
    CHECK(m[0][0] == Approx(2));
}
TEST_CASE("camera projection handles aspect, parallel lines and minimize", "[camera]") {
    Camera c;
    c.orthographic = false;
    auto p = c.projection(1600, 800);
    CHECK(p[1][1] / p[0][0] == Approx(2));
    c.orthographic = true;
    p = c.projection(100, 100);
    CHECK(p[3][3] == Approx(1));
    CHECK(p[2][3] == Approx(0));
    CHECK(std::isfinite(c.projection(0, 0)[0][0]));
    c.preset(design().data["shots"][1]);
    CHECK(c.eye.z == Approx(4.8));
    CHECK(c.eye.y == Approx(.9));
}
TEST_CASE("orbit retains low viewing angles instead of masking studio rendering defects", "[camera]") {
    Camera c;
    c.preset(design().data["shots"][1]);
    const float radius = glm::distance(c.eye, c.target);
    c.orbit(.3f, -5.f);
    CHECK(c.eye.y < c.target.y);
    CHECK(glm::distance(c.eye, c.target) == Approx(radius));
    c.zoom(-10.f);
    CHECK(c.eye.y < c.target.y);
    c.orbit(0, 10.f);
    CHECK(std::isfinite(c.view()[0][0]));
    CHECK(c.eye.y > c.target.y);
}
TEST_CASE("candidate nearest then stable id, radius, height and delivery", "[grab]") {
    std::vector<PrizeState> p = {{"b", {.02f, .762f, 0}}, {"a", {-.02f, .762f, 0}}};
    REQUIRE(chooseCandidate({0, .762f, 0}, p));
    CHECK(*chooseCandidate({0, .762f, 0}, p) == "a");
    p[0].position.x = .01f;
    CHECK(*chooseCandidate({0, .762f, 0}, p) == "b");
    CHECK_FALSE(chooseCandidate({.1f, .762f, 0}, p));
    CHECK_FALSE(chooseCandidate({0, 1, 0}, p));
    p[0].delivered = p[1].delivered = true;
    CHECK_FALSE(chooseCandidate({0, .762f, 0}, p));
}
TEST_CASE("chute crossing checks swept center and full extents", "[grab]") {
    auto d = design();
    auto chute = d.data["chute"];
    CHECK(crossesChute({-.255f, .9f, .18f}, {-.255f, .4f, .18f}, {.073f, .062f, .059f}, chute, .7f));
    CHECK_FALSE(crossesChute({-.14f, .9f, .18f}, {-.14f, .4f, .18f}, {.073f, .062f, .059f}, chute, .7f));
    CHECK_FALSE(crossesChute({-.255f, .9f, .18f}, {-.255f, .4f, .18f}, {.2f, .062f, .2f}, chute, .7f));
}
void finish(Round &r, int ticks = 900) {
    for (int i = 0; i < ticks; i++)
        r.advance(1. / 60);
}
TEST_CASE("success carries same instance without snapping, scores once at delivery", "[round]") {
    Round r(design());
    r.setAim({.16f, -.06f});
    r.advance(1. / 60, {{0, 0}, true});
    bool held = false, falling = false;
    glm::vec3 original{.14f, .762f, -.06f};
    for (int i = 0; i < 900; i++) {
        auto before = r.snapshot();
        r.advance(1. / 60);
        auto s = r.snapshot();
        if (!held && s.held) {
            held = true;
            CHECK(*s.held == "prize_03");
            CHECK(glm::distance(s.prizes[2].position, original) < .001f);
        }
        if (s.phase == Phase::Releasing) {
            falling = true;
            CHECK_FALSE(s.held);
        }
    }
    auto s = r.snapshot();
    CHECK(held);
    CHECK(falling);
    CHECK(s.phase == Phase::Result);
    CHECK(s.score == 1);
    REQUIRE(s.prizes.size() == 8);
    CHECK(s.prizes[2].id == "prize_03");
    CHECK(s.prizes[2].delivered);
    CHECK(s.prizes[2].position.y == Approx(.29));
    finish(r);
    CHECK(r.snapshot().score == 1);
}
TEST_CASE("miss is a complete round with zero points", "[round]") {
    Round r(design());
    r.setAim({-.27f, -.22f});
    r.advance(1. / 60, {{0, 0}, true});
    finish(r);
    CHECK(r.snapshot().phase == Phase::Result);
    CHECK(r.snapshot().score == 0);
    CHECK(r.snapshot().rounds == 1);
}
TEST_CASE("held space does not repeat; reset restores every phase; pause freezes", "[round]") {
    Round r(design());
    r.setAim({.14f, -.06f});
    for (int i = 0; i < 1200; i++)
        r.advance(1. / 60, {{0, 0}, true});
    CHECK(r.snapshot().rounds == 1);
    CHECK(r.snapshot().score == 1);
    for (int tick : {5, 85, 110, 180, 265, 300, 350, 500}) {
        r.reset();
        r.setAim({.14f, -.06f});
        r.advance(1. / 60, {{0, 0}, true});
        finish(r, tick);
        auto before = r.snapshot();
        r.advance(100, {{0, 0}, false, false, true});
        CHECK(r.snapshot().phase == before.phase);
        CHECK(r.snapshot().headY == before.headY);
        r.reset();
        CHECK(r.snapshot().score == 0);
        CHECK(r.snapshot().prizes.size() == 8);
        CHECK_FALSE(r.snapshot().held);
        CHECK(r.snapshot().prizes[2].position.y == Approx(.762));
    }
}
TEST_CASE("fixed clock matches frame rates and drops focus backlog", "[round]") {
    for (int fps : {30, 60, 144}) {
        Round r(design());
        FixedClock clock;
        r.setAim({.14f, -.06f});
        r.advance(1. / 60, {{0, 0}, true});
        for (int f = 0; f < fps * 15; f++)
            for (int n = clock.steps(1. / fps, true); n > 0; n--)
                r.advance(1. / 60);
        CHECK(r.snapshot().score == 1);
        CHECK(r.snapshot().phase == Phase::Result);
    }
    FixedClock clock;
    CHECK(clock.steps(20, true) == 5);
    CHECK(clock.steps(20, false) == 0);
    CHECK(clock.steps(0, true) == 0);
}
TEST_CASE("GLB import keeps named Y-up hierarchy and shared geometry", "[import]") {
    auto a = GltfLoader::load(root / "assets/models/claw-lab-r1.glb");
    REQUIRE(a.scene.find("claw_head") >= 0);
    REQUIRE(a.scene.find("prize_03") >= 0);
    auto p = glm::vec3(a.scene.worldMatrix(a.scene.find("prize_03"))[3]);
    CHECK(p.x == Approx(.14));
    CHECK(p.y == Approx(.762));
    CHECK(p.z == Approx(-.06));
    int finger = a.scene.nodes[a.scene.find("finger_0")].mesh;
    CHECK(a.scene.nodes[a.scene.find("finger_1")].mesh == finger);
    CHECK(a.scene.nodes[a.scene.find("finger_2")].mesh == finger);
    int shared = a.scene.nodes[a.scene.find("prize_01_volume")].mesh;
    for (int i = 1; i <= 8; i++)
        CHECK(a.scene.nodes[a.scene.find("prize_0" + std::to_string(i) + "_volume")].mesh == shared);
    CHECK_FALSE(a.images.empty());
    CHECK_THROWS(GltfLoader::load(root / "missing.glb"));
}
TEST_CASE("woven sRGB texture preserves the canonical pink base color", "[import]") {
    auto a = GltfLoader::load(root / "assets/models/claw-lab-r1.glb");
    REQUIRE_FALSE(a.images.empty());
    const auto &image = a.images.front();
    double mean[3] = {0, 0, 0};
    int pixels = image.width * image.height;
    for (int i = 0; i < pixels; i++)
        for (int c = 0; c < 3; c++)
            mean[c] += image.pixels[i * image.components + c] / double(pixels);
    CHECK(mean[0] == Approx(232).margin(12));
    CHECK(mean[1] == Approx(180).margin(12));
    CHECK(mean[2] == Approx(166).margin(12));
}
TEST_CASE("all replayed rounds preserve eight identities; movement unlocks after result", "[round]") {
    Round r(design());
    r.setAim({.14f, -.06f});
    r.advance(1. / 60, {{0, 0}, true});
    finish(r);
    REQUIRE(r.snapshot().score == 1);
    r.advance(1. / 60, {{1, 0}});
    CHECK(r.snapshot().phase == Phase::Aiming);
    CHECK(r.snapshot().aim.x > -.255f);
    CHECK(r.snapshot().prizes.size() == 8);
}
TEST_CASE("rail motion clamps, automatic cycle ignores steering", "[round]") {
    Round r(design());
    r.setAim({9, -9});
    CHECK(r.snapshot().aim.x == Approx(.27));
    CHECK(r.snapshot().aim.y == Approx(-.22));
    r.advance(1. / 60, {{0, 0}, true});
    for (int i = 0; i < 40; i++)
        r.advance(1. / 60, {{-1, 1}});
    CHECK(r.snapshot().aim.x == Approx(.27));
    CHECK(r.snapshot().aim.y == Approx(-.22));
}
