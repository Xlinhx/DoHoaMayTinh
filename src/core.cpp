#include "core.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <stdexcept>
namespace claw {
static glm::vec3 vec3(const Json &j) {
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}
glm::mat4 localMatrix(const Transform &t) {
    return glm::translate(glm::mat4(1), t.translation) * glm::toMat4(t.rotation) *
           glm::scale(glm::mat4(1), t.scale);
}
glm::vec3 fromAuthoring(glm::vec3 v) {
    return {v.x, v.z, -v.y};
}
int Scene::find(const std::string &name) const {
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].name == name)
            return int(i);
    return -1;
}
glm::mat4 Scene::worldMatrix(int i) const {
    if (i < 0 || i >= int(nodes.size()))
        throw std::out_of_range("Invalid scene node");
    auto result = localMatrix(nodes[i].local);
    int parent = nodes[i].parent;
    size_t count = 0;
    while (parent >= 0) {
        if (parent >= int(nodes.size()) || ++count > nodes.size())
            throw std::runtime_error("Invalid/cyclic scene hierarchy");
        result = localMatrix(nodes[parent].local) * result;
        parent = nodes[parent].parent;
    }
    return result;
}
std::string readText(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot read resource: " + path.string());
    return {std::istreambuf_iterator<char>(f), {}};
}
std::filesystem::path resourceRoot(const std::filesystem::path &executable) {
    return std::filesystem::absolute(executable).parent_path().lexically_normal();
}
DesignConfig loadDesign(const std::filesystem::path &path) {
    try {
        auto j = Json::parse(readText(path));
        if (j.at("schema") != "claw-lab.design/v1" || j.at("revision") != "r1")
            throw std::runtime_error("schema/revision unsupported");
        std::set<std::string> ids;
        for (auto &p : j.at("prizes")) {
            if (!ids.insert(p.at("id").get<std::string>()).second)
                throw std::runtime_error("prizes.id duplicate");
            if (p.at("xy").size() != 2)
                throw std::runtime_error("prizes.xy invalid");
        }
        if (ids.size() != 8 || !ids.count(j.at("prize_asset").at("target_id").get<std::string>()))
            throw std::runtime_error("prize identity/count invalid");
        if (j.at("claw").at("finger_count") != 3)
            throw std::runtime_error("claw.finger_count invalid");
        for (auto key : {"rail_limit_x", "rail_limit_y", "head_z"}) {
            auto b = j.at("cabinet").at(key);
            if (b.size() != 2 || b[0].get<double>() >= b[1].get<double>())
                throw std::runtime_error(std::string("cabinet.") + key + " bounds invalid");
        }
        return {j};
    } catch (const std::exception &e) {
        throw std::runtime_error("Invalid design " + path.string() + ": " + e.what());
    }
}
glm::mat4 Camera::view() const {
    auto direction = glm::normalize(target - eye);
    auto up = std::abs(direction.y) > .999f ? glm::vec3(0, 0, -1) : glm::vec3(0, 1, 0);
    return glm::lookAt(eye, target, up);
}
glm::mat4 Camera::projection(int width, int height) const {
    float aspect = float(std::max(width, 1)) / float(std::max(height, 1));
    float vertical = orthoScale, horizontal = vertical * aspect;
    return orthographic
               ? glm::ortho(-horizontal / 2, horizontal / 2, -vertical / 2, vertical / 2, .02f, 100.f)
               : glm::perspective(glm::radians(34.f), aspect, .02f, 100.f);
}
void Camera::preset(const Json &shot) {
    eye = fromAuthoring(vec3(shot.at("camera")));
    target = fromAuthoring(vec3(shot.at("look_at")));
    orthoScale = shot.at("ortho");
    orthographic = true;
}
void Camera::orbit(float dx, float dy) {
    auto d = eye - target;
    float r = glm::length(d);
    float yaw = std::atan2(d.x, d.z) + dx;
    float pitch = glm::clamp(std::asin(d.y / r) + dy, -1.45f, 1.48f);
    eye = target +
          glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)) * r;
}
void Camera::zoom(float amount) {
    float f = std::exp(-amount * .1f);
    orthoScale = glm::clamp(orthoScale * f, .5f, 6.f);
    eye = target + glm::normalize(eye - target) * glm::clamp(glm::distance(eye, target) * f, 1.2f, 12.f);
}
std::optional<InstanceId> chooseCandidate(glm::vec3 grip, const std::vector<PrizeState> &prizes) {
    float best = .045f * .045f;
    std::optional<InstanceId> result;
    for (auto &p : prizes) {
        if (p.delivered || std::abs(p.position.y - grip.y) > .035f)
            continue;
        auto d = glm::vec2(p.position.x - grip.x, p.position.z - grip.z);
        float d2 = glm::dot(d, d);
        if (d2 <= best + 1e-8f && (!result || d2 < best - 1e-8f || p.id < *result)) {
            best = d2;
            result = p.id;
        }
    }
    return result;
}
bool crossesChute(glm::vec3 before, glm::vec3 after, glm::vec3 half, const Json &chute, float floor) {
    float plane = floor + half.y;
    if (before.y < plane || after.y > plane || before.y == after.y)
        return false;
    float t = (before.y - plane) / (before.y - after.y);
    auto p = glm::mix(before, after, t);
    return p.x - half.x >= chute["opening_x"][0].get<float>() &&
           p.x + half.x <= chute["opening_x"][1].get<float>() &&
           -p.z - half.z >= chute["opening_y"][0].get<float>() &&
           -p.z + half.z <= chute["opening_y"][1].get<float>();
}
const char *phaseName(Phase p) {
    static const char *names[] = {"ready",   "aiming",    "lowering",  "closing",
                                  "lifting", "returning", "releasing", "result"};
    return names[int(p)];
}
Round::Round(DesignConfig c) : config_(std::move(c)) {
    reset();
}
void Round::reset() {
    s_ = RoundSnapshot{};
    clock_ = 0;
    lastGrab_ = false;
    falling_.reset();
    fallVelocity_ = 0;
    guided_ = false;
    guideTime_ = 0;
    auto &j = config_.data;
    auto &idle = j["states"]["idle"];
    s_.aim = {idle["xy"][0].get<float>(), -idle["xy"][1].get<float>()};
    s_.headY = idle["head_z"];
    s_.finger = idle["finger_deg"];
    float y = j["cabinet"]["floor_z"].get<float>() + j["prize_asset"]["dimensions"][2].get<float>() / 2;
    for (auto &p : j["prizes"])
        s_.prizes.push_back({p["id"], {p["xy"][0].get<float>(), y, -p["xy"][1].get<float>()}, false});
}
void Round::setAim(glm::vec2 aim) {
    if (s_.phase != Phase::Ready && s_.phase != Phase::Aiming)
        return;
    auto &c = config_.data["cabinet"];
    s_.aim = {glm::clamp(aim.x, c["rail_limit_x"][0].get<float>(), c["rail_limit_x"][1].get<float>()),
              glm::clamp(aim.y, -c["rail_limit_y"][1].get<float>(), -c["rail_limit_y"][0].get<float>())};
    s_.phase = Phase::Aiming;
}
void Round::transition(Phase p) {
    s_.phase = p;
    clock_ = 0;
    startAim_ = s_.aim;
    startHeight_ = s_.headY;
}
static float ease(float t) {
    t = glm::clamp(t, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
void Round::advance(double dt, const Commands &cmd) {
    if (cmd.reset) {
        reset();
        lastGrab_ = cmd.grab;
        return;
    }
    bool edge = cmd.grab && !lastGrab_;
    lastGrab_ = cmd.grab;
    if (cmd.paused || dt <= 0)
        return;
    dt = std::min(dt, .25);
    auto &j = config_.data;
    auto &c = j["cabinet"];
    float high = c["head_z"][1], low = c["head_z"][0], open = j["claw"]["finger_open_deg"];
    if (s_.phase == Phase::Result) {
        if (edge || glm::length(cmd.move) > 0) {
            transition(Phase::Ready);
        } else
            return;
    }
    if (s_.phase == Phase::Ready || s_.phase == Phase::Aiming) {
        if (glm::length(cmd.move) > 0)
            setAim(s_.aim + glm::normalize(cmd.move) * float(dt) * .24f);
        if (edge) {
            ++s_.rounds;
            transition(Phase::Lowering);
        } else
            return;
    }
    clock_ += dt;
    switch (s_.phase) {
    case Phase::Lowering:
        s_.headY = glm::mix(high, low, ease(float(clock_ / 1.30)));
        if (clock_ + 1e-8 >= 1.30)
            transition(Phase::Closing);
        break;
    case Phase::Closing:
        s_.finger = glm::mix(open, 0.f, ease(float(clock_ / .45)));
        if (clock_ + 1e-8 >= .45) {
            s_.held = chooseCandidate(
                {s_.aim.x, s_.headY + j["claw"]["grip_offset_z"].get<float>(), s_.aim.y}, s_.prizes);
            if (s_.held)
                for (auto &p : s_.prizes)
                    if (p.id == *s_.held)
                        gripLocal_ = p.position - glm::vec3(s_.aim.x, s_.headY, s_.aim.y);
            transition(Phase::Lifting);
        }
        break;
    case Phase::Lifting:
        s_.headY = glm::mix(low, high, ease(float(clock_ / 1.25)));
        if (clock_ + 1e-8 >= 1.25)
            transition(Phase::Returning);
        break;
    case Phase::Returning: {
        auto xy = j["chute"]["drop_xy"];
        glm::vec2 drop{xy[0].get<float>(), -xy[1].get<float>()};
        s_.aim = glm::mix(startAim_, drop, ease(float(clock_ / 1.4)));
        if (clock_ + 1e-8 >= 1.4) {
            falling_ = s_.held;
            s_.held.reset();
            fallVelocity_ = 0;
            guided_ = false;
            guideTime_ = 0;
            transition(Phase::Releasing);
        }
        break;
    }
    case Phase::Releasing:
        s_.finger = glm::mix(0.f, open, ease(float(clock_ / .55)));
        if (clock_ > .55 && !falling_)
            transition(Phase::Result);
        break;
    default:
        break;
    }
    if (s_.held)
        for (auto &p : s_.prizes)
            if (p.id == *s_.held)
                p.position = glm::vec3(s_.aim.x, s_.headY, s_.aim.y) + gripLocal_;
    if (falling_) {
        for (auto &p : s_.prizes)
            if (p.id == *falling_) {
                if (!guided_) {
                    auto before = p.position;
                    fallVelocity_ -= 9.81f * float(dt);
                    p.position.y += fallVelocity_ * float(dt);
                    auto dims = vec3(j["prize_asset"]["dimensions"]);
                    glm::vec3 half{dims.x / 2, dims.z / 2, dims.y / 2};
                    if (crossesChute(before, p.position, half, j["chute"], c["floor_z"].get<float>())) {
                        guided_ = true;
                        guideTime_ = 0;
                        fallStart_ = p.position;
                    } else if (p.position.y < c["floor_z"].get<float>() + half.y) {
                        p.position.y = c["floor_z"].get<float>() + half.y;
                        falling_.reset();
                    }
                } else {
                    guideTime_ += dt;
                    auto &path = j["chute"]["guided_centerline"];
                    float progress = glm::clamp(float(guideTime_ / .65), 0.f, 1.f) * 3;
                    int segment = std::min(2, int(progress));
                    auto a = segment == 0 ? fallStart_ : fromAuthoring(vec3(path[segment]));
                    auto b = fromAuthoring(vec3(path[segment + 1]));
                    p.position = glm::mix(a, b, ease(progress - segment));
                    if (guideTime_ + 1e-8 >= .65) {
                        p.position = fromAuthoring({j["chute"]["delivery_xy"][0],
                                                    j["chute"]["delivery_xy"][1], j["chute"]["delivery_z"]});
                        if (!p.delivered) {
                            p.delivered = true;
                            ++s_.score;
                        }
                        falling_.reset();
                    }
                }
                break;
            }
    }
}
int FixedClock::steps(double dt, bool focused) {
    if (!focused) {
        accumulator_ = 0;
        return 0;
    }
    accumulator_ += std::clamp(dt, 0., .25);
    int count = std::min(5, int((accumulator_ + 1e-10) / (1. / 60)));
    accumulator_ -= count * (1. / 60);
    if (count == 5)
        accumulator_ = 0;
    return count;
}
void applyPose(Scene &scene, const RoundSnapshot &s, const DesignConfig &, glm::vec2 move) {
    auto node = [&](const std::string &name) -> Node & {
        int i = scene.find(name);
        if (i < 0)
            throw std::runtime_error("Missing required node: " + name);
        return scene.nodes[i];
    };
    node("bridge_y").local.translation.z = s.aim.y;
    node("trolley_x").local.translation.x = s.aim.x;
    node("claw_head").local.translation.y = s.headY;
    auto &cable = node("lift_cable").local;
    cable.translation.y = (1.54f + s.headY + .04f) / 2;
    cable.scale.y = (1.54f - s.headY - .04f) / .1f;
    for (int i = 0; i < 3; i++)
        node("finger_joint_" + std::to_string(i)).local.rotation =
            glm::angleAxis(glm::radians(-s.finger), glm::vec3(0, 0, 1));
    for (auto &p : s.prizes)
        node(p.id).local.translation = p.position;
    for (auto name : {"joystick_stem", "joystick_grip"}) {
        auto &n = node(name);
        n.local.rotation = glm::angleAxis(-move.x * .18f, glm::vec3(0, 0, 1)) *
                           glm::angleAxis(move.y * .18f, glm::vec3(1, 0, 0));
        if (std::string(name) == "joystick_grip") {
            n.local.translation.x = .04f + move.x * .007f;
            n.local.translation.z = .434f + move.y * .007f;
        }
    }
    node("grab_button").local.translation.y =
        s.phase >= Phase::Lowering && s.phase <= Phase::Closing ? .692f : .699f;
}
void applyReferencePose(Scene &scene, const std::string &name, const DesignConfig &d) {
    Round r(d);
    auto s = r.snapshot();
    auto &p = d.data["states"].at(name);
    s.aim = {p["xy"][0].get<float>(), -p["xy"][1].get<float>()};
    s.headY = p["head_z"];
    s.finger = p["finger_deg"];
    for (auto &prize : s.prizes)
        if (prize.id == d.data["prize_asset"]["target_id"].get<std::string>()) {
            std::string target = p["target"];
            if (target == "held")
                prize.position = {s.aim.x, s.headY + d.data["claw"]["grip_offset_z"].get<float>(), s.aim.y};
            if (target == "falling")
                prize.position = {s.aim.x, p["target_z"].get<float>(), s.aim.y};
            if (target == "delivered")
                prize.position =
                    fromAuthoring({d.data["chute"]["delivery_xy"][0], d.data["chute"]["delivery_xy"][1],
                                   d.data["chute"]["delivery_z"]});
        }
    applyPose(scene, s, d);
}
} // namespace claw
