#pragma once
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>
namespace claw {
using Json = nlohmann::json;
using InstanceId = std::string;
struct Transform {
    glm::vec3 translation{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};
};
glm::mat4 localMatrix(const Transform &t);
glm::vec3 fromAuthoring(glm::vec3 v);
struct Node {
    std::string name, group;
    int parent = -1, mesh = -1;
    Transform local;
};
struct Scene {
    std::vector<Node> nodes;
    int find(const std::string &name) const;
    glm::mat4 worldMatrix(int node) const;
};
struct DesignConfig {
    Json data;
};
DesignConfig loadDesign(const std::filesystem::path &path);
std::filesystem::path resourceRoot(const std::filesystem::path &executable);
std::string readText(const std::filesystem::path &path);
struct Camera {
    glm::vec3 eye{2.55f, 2.45f, 3.65f}, target{0, .93f, 0};
    float orthoScale = 2.75f;
    bool orthographic = true;
    glm::mat4 view() const;
    glm::mat4 projection(int width, int height) const;
    void preset(const Json &shot);
    void orbit(float dx, float dy);
    void zoom(float amount);
};
struct PrizeState {
    InstanceId id;
    glm::vec3 position;
    bool delivered = false;
};
std::optional<InstanceId> chooseCandidate(glm::vec3 grip, const std::vector<PrizeState> &prizes);
bool crossesChute(glm::vec3 before, glm::vec3 after, glm::vec3 halfExtents, const Json &chute, float floor);
enum class Phase { Ready, Aiming, Lowering, Closing, Lifting, Returning, Releasing, Result };
const char *phaseName(Phase phase);
struct Commands {
    glm::vec2 move{0};
    bool grab = false, reset = false, paused = false;
};
struct RoundSnapshot {
    Phase phase = Phase::Ready;
    glm::vec2 aim{0, -.1f};
    float headY = 1.46f, finger = -22;
    int score = 0, rounds = 0;
    std::optional<InstanceId> held;
    std::vector<PrizeState> prizes;
};
class Round {
    DesignConfig config_;
    RoundSnapshot s_;
    double clock_ = 0;
    bool lastGrab_ = false;
    glm::vec2 startAim_{0};
    float startHeight_ = 0;
    glm::vec3 gripLocal_{0}, fallStart_{0};
    std::optional<InstanceId> falling_;
    float fallVelocity_ = 0;
    bool guided_ = false;
    double guideTime_ = 0;
    void transition(Phase p);

  public:
    explicit Round(DesignConfig config);
    void reset();
    void setAim(glm::vec2 aim);
    void advance(double fixedDt, const Commands &commands = {});
    const RoundSnapshot &snapshot() const { return s_; }
};
class FixedClock {
    double accumulator_ = 0;

  public:
    int steps(double dt, bool focused);
};
void applyPose(Scene &, const RoundSnapshot &, const DesignConfig &, glm::vec2 move = glm::vec2(0));
void applyReferencePose(Scene &, const std::string &, const DesignConfig &);
} // namespace claw
