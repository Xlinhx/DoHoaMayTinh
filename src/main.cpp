#include "assets/GltfLoader.h"
#include "core.h"
#include "graphics/Renderer.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <windows.h>
using namespace claw;
namespace {
std::filesystem::path executablePath() {
    std::vector<wchar_t> buffer(32768);
    DWORD n = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (!n || n == buffer.size())
        throw std::runtime_error("Cannot find executable path");
    return std::filesystem::path(std::wstring(buffer.data(), n));
}
struct Window {
    GLFWwindow *handle = nullptr;
    Window(int width, int height, bool hidden) {
        glfwSetErrorCallback(
            [](int code, const char *message) { std::cerr << "GLFW " << code << ": " << message << '\n'; });
        if (!glfwInit())
            throw std::runtime_error("GLFW initialization failed; a desktop OpenGL driver is required.");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_VISIBLE, !hidden);
        handle = glfwCreateWindow(width, height, "CLAW LAB", nullptr, nullptr);
        if (!handle) {
            glfwTerminate();
            throw std::runtime_error("Cannot create OpenGL 3.3 Core context. Update the GPU driver.");
        }
        glfwMakeContextCurrent(handle);
        if (!gladLoadGL(glfwGetProcAddress)) {
            glfwDestroyWindow(handle);
            glfwTerminate();
            throw std::runtime_error("GLAD failed to load OpenGL 3.3");
        }
        glfwSwapInterval(hidden ? 0 : 1);
        glEnable(GL_MULTISAMPLE);
    }
    ~Window() {
        if (handle)
            glfwDestroyWindow(handle);
        glfwTerminate();
    }
};
struct UI {
    UI(GLFWwindow *w) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        wchar_t systemDirectory[MAX_PATH];
        GetWindowsDirectoryW(systemDirectory, MAX_PATH);
        auto font = std::filesystem::path(systemDirectory) / "Fonts/segoeui.ttf";
        io.Fonts->AddFontFromFileTTF(font.string().c_str(), 18.f, nullptr,
                                     io.Fonts->GetGlyphRangesVietnamese());
        auto &s = ImGui::GetStyle();
        s.WindowRounding = 14;
        s.FrameRounding = 7;
        s.WindowBorderSize = 0;
        s.FramePadding = {13, 8};
        s.ItemSpacing = {8, 7};
        s.WindowPadding = {16, 12};
        s.Colors[ImGuiCol_WindowBg] = {.98f, .97f, .94f, .97f};
        s.Colors[ImGuiCol_Text] = {.09f, .23f, .22f, 1};
        s.Colors[ImGuiCol_Button] = {.89f, .91f, .87f, 1};
        s.Colors[ImGuiCol_ButtonHovered] = {.79f, .86f, .81f, 1};
        s.Colors[ImGuiCol_ButtonActive] = {.65f, .77f, .71f, 1};
        ImGui_ImplGlfw_InitForOpenGL(w, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");
    }
    ~UI() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};
struct CaptureTarget {
    unsigned fbo = 0, color = 0, depth = 0;
    CaptureTarget(int width, int height, bool enabled) {
        if (!enabled)
            return;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &color);
        glBindRenderbuffer(GL_RENDERBUFFER, color);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Capture framebuffer incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    ~CaptureTarget() {
        if (depth)
            glDeleteRenderbuffers(1, &depth);
        if (color)
            glDeleteRenderbuffers(1, &color);
        if (fbo)
            glDeleteFramebuffers(1, &fbo);
    }
};
std::string fingerprint(const SceneAsset &a) {
    uint64_t hash = 14695981039346656037ull;
    auto add = [&](const void *p, size_t n) {
        auto b = static_cast<const unsigned char *>(p);
        for (size_t i = 0; i < n; i++) {
            hash ^= b[i];
            hash *= 1099511628211ull;
        }
    };
    for (auto &m : a.meshes)
        for (auto &p : m.primitives) {
            add(p.vertices.data(), p.vertices.size() * sizeof(Vertex));
            add(p.indices.data(), p.indices.size() * sizeof(unsigned));
            add(&p.material, sizeof(int));
        }
    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
}
Json identity(const SceneAsset &a) {
    Json out;
    out["nodes"] = a.scene.nodes.size();
    out["meshes"] = a.meshes.size();
    out["geometry_material_fingerprint_fnv1a"] = fingerprint(a);
    out["prizes"] = Json::array();
    for (int i = 1; i <= 8; i++) {
        std::string id = "prize_0" + std::to_string(i);
        int root = a.scene.find(id);
        if (root < 0)
            throw std::runtime_error("Missing prize root: " + id);
        Json children = Json::array();
        for (auto &n : a.scene.nodes)
            if (n.parent == root && n.mesh >= 0)
                children.push_back({{"mesh", n.mesh}, {"name", n.name}});
        out["prizes"].push_back({{"id", id}, {"parts", children}});
    }
    return out;
}
} // namespace
int main(int argc, char **argv) {
    bool automated = false;
    try {
        auto root = resourceRoot(executablePath());
        std::filesystem::path output = root / "captures";
        bool hidden = false, captureAll = false, verify = false, benchmark = false;
        std::string shotName;
        int width = 1366, height = 768, frames = 0;
        RenderOptions options;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            auto value = [&]() {
                if (++i >= argc)
                    throw std::runtime_error("Missing value for " + arg);
                return std::string(argv[i]);
            };
            if (arg == "--resource-root")
                root = std::filesystem::u8path(value());
            else if (arg == "--output")
                output = std::filesystem::u8path(value());
            else if (arg == "--hidden")
                hidden = true;
            else if (arg == "--capture-all")
                captureAll = true;
            else if (arg == "--verify")
                verify = true;
            else if (arg == "--benchmark")
                benchmark = true;
            else if (arg == "--shot")
                shotName = value();
            else if (arg == "--frames")
                frames = std::stoi(value());
            else if (arg == "--size") {
                auto size = value();
                auto pos = size.find('x');
                width = std::stoi(size.substr(0, pos));
                height = std::stoi(size.substr(pos + 1));
            } else if (arg == "--no-glass")
                options.glass = false;
            else if (arg == "--no-shadow")
                options.shadow = false;
            else if (arg == "--unlit")
                options.lit = false;
            else if (arg == "--no-texture")
                options.texture = false;
            else
                throw std::runtime_error("Unknown argument: " + arg);
        }
        automated = captureAll || verify || benchmark || frames > 0 || !shotName.empty();
        if (width < 1 || height < 1 || width > 4096 || height > 4096)
            throw std::runtime_error("Invalid window size (1..4096)");
        auto design = loadDesign(root / "assets/design/design-source.json");
        auto asset = GltfLoader::load(root / "assets/models/claw-lab-r1.glb");
        Window window(width, height, hidden);
        auto w = window.handle;
        std::string gpu = reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
                    version = reinterpret_cast<const char *>(glGetString(GL_VERSION)),
                    vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        std::cout << "GPU: " << vendor << " / " << gpu << " / " << version << '\n'
                  << "ASSET: " << asset.scene.nodes.size() << " nodes, " << asset.meshes.size()
                  << " shared meshes\n"
                  << std::flush;
        Renderer renderer(asset, root / "shaders");
        CaptureTarget target(width, height, captureAll || verify || benchmark || !shotName.empty());
        Camera camera;
        Round round(design);
        auto playerCamera = [&](int index) {
            camera.preset(design.data["shots"][index]);
            options.hide.clear();
            if (index == 0) {
                camera.orthoScale = 2.45f;
                camera.target.y = .75f;
            }
            if (index == 1) {
                camera.orthoScale = 2.45f;
                camera.eye.y = camera.target.y = .78f;
            }
            if (index == 4) {
                camera.orthoScale = 1.55f;
                options.hide = {"roof", "glass"};
            }
        };
        playerCamera(0);
        auto initialFingerprint = fingerprint(asset);
        Json report = {{"renderer", gpu},
                       {"vendor", vendor},
                       {"opengl", version},
                       {"size", {width, height}},
                       {"asset", identity(asset)}};
        auto draw = [&](const RenderOptions &o) {
            if (!target.fbo)
                glfwGetFramebufferSize(w, &width, &height);
            renderer.draw(asset.scene, camera, width, height, o, target.fbo);
        };
        if (captureAll || !shotName.empty()) {
            report["reference_captures"] = Json::array();
            for (auto &shot : design.data["shots"]) {
                std::string id = shot["id"];
                if (id == "17-materials")
                    continue;
                if (!shotName.empty() && id != shotName)
                    continue;
                auto o = options;
                if (shot.contains("hide"))
                    o.hide = shot["hide"].get<std::vector<std::string>>();
                if (shot.contains("isolate"))
                    o.isolate = shot["isolate"];
                applyReferencePose(asset.scene, shot["state"], design);
                camera.preset(shot);
                for (int i = 0; i < 3; i++) {
                    draw(o);
                    if (i == 2)
                        renderer.capture(output / (id + ".png"), width, height);
                    glfwSwapBuffers(w);
                    glfwPollEvents();
                }
                report["reference_captures"].push_back(id);
            }
            if (report["reference_captures"].empty())
                throw std::runtime_error("Unknown reference shot: " + shotName);
        }
        if (verify) {
            camera.preset(design.data["shots"][0]);
            if (width != height)
                camera.orthoScale = 2.45f;
            report["rounds"] = Json::array();
            for (int trial = 0; trial < 10; trial++) {
                round.reset();
                bool hit = trial % 2 == 0;
                round.setAim(hit ? glm::vec2(.14f, -.06f) : glm::vec2(-.27f, -.22f));
                round.advance(1. / 60, {{0, 0}, true});
                std::set<int> seen;
                Json trace = Json::array();
                for (int tick = 0; tick < 600; tick++) {
                    round.advance(1. / 60);
                    auto &s = round.snapshot();
                    applyPose(asset.scene, s, design);
                    int p = int(s.phase);
                    if (seen.insert(p).second) {
                        trace.push_back(phaseName(s.phase));
                        draw(options);
                        if (trial == 0)
                            renderer.capture(output / (std::string("live-") + phaseName(s.phase) + ".png"),
                                             width, height);
                        glfwSwapBuffers(w);
                        glfwPollEvents();
                    }
                }
                auto &s = round.snapshot();
                if (s.phase != Phase::Result || s.score != (hit ? 1 : 0) || s.prizes.size() != 8 ||
                    fingerprint(asset) != initialFingerprint)
                    throw std::runtime_error("Runtime round/identity verification failed");
                report["rounds"].push_back({{"trial", trial + 1},
                                            {"hit", hit},
                                            {"score", s.score},
                                            {"phase_trace", trace},
                                            {"target_id", s.prizes[2].id},
                                            {"delivered", s.prizes[2].delivered}});
            }
            report["runtime_rounds_passed"] = 10;
        }
        if (benchmark) {
            round.reset();
            applyPose(asset.scene, round.snapshot(), design);
            camera.preset(design.data["shots"][0]);
            camera.orthoScale = 2.45f;
            std::vector<double> times;
            for (int i = 0; i < 360; i++) {
                auto start = std::chrono::steady_clock::now();
                draw(options);
                glFinish();
                auto end = std::chrono::steady_clock::now();
                if (i >= 60)
                    times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
                glfwSwapBuffers(w);
                glfwPollEvents();
            }
            std::sort(times.begin(), times.end());
            report["benchmark"] = {{"samples", times.size()},
                                   {"warmup", 60},
                                   {"p50_ms", times[times.size() / 2]},
                                   {"p95_ms", times[times.size() * 95 / 100]},
                                   {"method", "CPU submission + glFinish, excludes swap/vsync"}};
        }
        if (captureAll || verify || benchmark || !shotName.empty()) {
            std::filesystem::create_directories(output);
            std::ofstream f(output / "verification.json");
            f << std::setw(2) << report << '\n';
            std::cout << report.dump(2) << '\n';
            return 0;
        }
        UI ui(w);
        auto &io = ImGui::GetIO();
        FixedClock clock;
        double previous = glfwGetTime(), scroll = 0;
        bool focused = true, lastReset = false, lastProjection = false;
        int frame = 0;
        bool pendingGrab = false;
        glfwSetWindowUserPointer(w, &scroll);
        glfwSetScrollCallback(w, [](GLFWwindow *window, double, double y) {
            *static_cast<double *>(glfwGetWindowUserPointer(window)) += y;
        });
        while (!glfwWindowShouldClose(w)) {
            glfwPollEvents();
            double now = glfwGetTime(), dt = now - previous;
            previous = now;
            bool currentFocus = glfwGetWindowAttrib(w, GLFW_FOCUSED) != 0;
            bool active = currentFocus && focused;
            focused = currentFocus;
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            Commands commands;
            auto down = [&](int key) { return glfwGetKey(w, key) == GLFW_PRESS; };
            commands.move = {float(down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT)) -
                                 float(down(GLFW_KEY_A) || down(GLFW_KEY_LEFT)),
                             float(down(GLFW_KEY_S) || down(GLFW_KEY_DOWN)) -
                                 float(down(GLFW_KEY_W) || down(GLFW_KEY_UP))};
            pendingGrab |= ImGui::IsKeyPressed(ImGuiKey_Space, false);
            commands.grab = down(GLFW_KEY_SPACE);
            bool reset = down(GLFW_KEY_R) || ImGui::IsKeyPressed(ImGuiKey_R, false);
            commands.reset = reset && !lastReset;
            lastReset = reset;
            if (down(GLFW_KEY_ESCAPE) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                glfwSetWindowShouldClose(w, true);
            bool one = down(GLFW_KEY_1) || ImGui::IsKeyPressed(ImGuiKey_1, false),
                 two = down(GLFW_KEY_2) || ImGui::IsKeyPressed(ImGuiKey_2, false),
                 three = down(GLFW_KEY_3) || ImGui::IsKeyPressed(ImGuiKey_3, false);
            if (one)
                playerCamera(0);
            if (two)
                playerCamera(1);
            if (three)
                playerCamera(4);
            bool proj = down(GLFW_KEY_P) || ImGui::IsKeyPressed(ImGuiKey_P, false);
            if (proj && !lastProjection)
                camera.orthographic = !camera.orthographic;
            lastProjection = proj;
            auto display = io.DisplaySize;
            float barWidth = std::min(display.x - 32, 1020.f);
            ImGui::SetNextWindowPos({(display.x - barWidth) / 2, display.y - 104});
            ImGui::SetNextWindowSize({barWidth, 88});
            ImGui::Begin("controls", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings);
            auto &s = round.snapshot();
            const char *labels[] = {"Sẵn sàng",  "Căn vị trí", "Đang hạ càng", "Đang gắp",
                                    "Đang nâng", "Về cửa thả", "Đang thả",     "Hoàn tất"};
            ImGui::TextUnformatted(labels[int(s.phase)]);
            ImGui::SameLine();
            ImGui::TextDisabled("  ·  %d lượt  /  %d vật phẩm", s.rounds, s.score);
            ImGui::SameLine(barWidth - 365);
            if (ImGui::SmallButton("Góc chéo"))
                playerCamera(0);
            ImGui::SameLine();
            if (ImGui::SmallButton("Trước"))
                playerCamera(1);
            ImGui::SameLine();
            if (ImGui::SmallButton("Trên"))
                playerCamera(4);
            ImGui::SameLine();
            if (ImGui::SmallButton(camera.orthographic ? "Phối cảnh" : "Trực giao"))
                camera.orthographic = !camera.orthographic;
            ImGui::TextDisabled("WASD / mũi tên: di chuyển    ·    Kéo chuột: xoay    ·    Cuộn: zoom");
            ImGui::SameLine(barWidth - 260);
            bool ready = s.phase == Phase::Ready || s.phase == Phase::Aiming || s.phase == Phase::Result;
            ImGui::BeginDisabled(!ready);
            ImGui::PushStyleColor(ImGuiCol_Button, {.10f, .42f, .40f, 1});
            ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
            if (ImGui::Button("Gắp  [Space]", {140, 32}))
                pendingGrab = true;
            ImGui::PopStyleColor(2);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Chơi lại  [R]", {110, 32}))
                commands.reset = true;
            ImGui::End();
            if (!active) {
                ImGui::SetNextWindowPos({display.x / 2, 28}, ImGuiCond_Always, {.5f, 0});
                ImGui::Begin("pause", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::TextUnformatted("Tạm dừng");
                ImGui::End();
            }
            if (!io.WantCaptureMouse) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
                    ImGui::IsMouseDragging(ImGuiMouseButton_Right))
                    camera.orbit(-io.MouseDelta.x * .006f, io.MouseDelta.y * .006f);
                if (scroll) {
                    camera.zoom(float(scroll));
                    scroll = 0;
                }
            } else
                scroll = 0;
            int steps = clock.steps(dt, active);
            if (commands.reset) {
                round.reset();
                pendingGrab = false;
            }
            for (int i = 0; i < steps; i++) {
                auto step = commands;
                step.reset = false;
                step.grab = commands.grab || (pendingGrab && i == 0);
                round.advance(1. / 60, step);
                if (i == 0)
                    pendingGrab = false;
            }
            applyPose(asset.scene, round.snapshot(), design, commands.move);
            draw(options);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (frames > 0 && ++frame >= frames) {
                renderer.capture(output / "play.png", width, height);
                glfwSetWindowShouldClose(w, true);
            }
            glfwSwapBuffers(w);
            if (width == 0 || height == 0)
                glfwWaitEventsTimeout(.05);
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "CLAW LAB ERROR: " << e.what() << '\n';
        if (!automated) {
            MessageBoxA(nullptr, e.what(), "CLAW LAB - Startup error", MB_OK | MB_ICONERROR);
        }
        return 1;
    }
}
