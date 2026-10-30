// Manual GPU regression: use the production orbit and renderer at the lowest angle.
#include "graphics/Renderer.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <vector>
int main() {
    using namespace claw;
    const std::filesystem::path root = SOURCE_ROOT;
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    auto window = glfwCreateWindow(1366, 768, "Camera regression", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress)) return 1;
    int result = 0;
    try {
        auto design = loadDesign(root / "assets/design/design-source.json");
        auto asset = GltfLoader::load(root / "assets/models/claw-lab-r1.glb");
        Renderer renderer(asset, root / "shaders");
        Camera camera;
        camera.preset(design.data["shots"][0]);
        camera.orthoScale = 2.45f;
        camera.target.y = .75f;
        for (bool ortho : {true, false}) {
          for (float pitch : {-.35f, -.05f, 0.f, .08f, .3f}) {
            camera.preset(design.data["shots"][0]);
            camera.orthoScale = 2.45f;
            camera.target.y = .75f;
            auto d = camera.eye - camera.target;
            float startPitch = std::asin(d.y / glm::length(d));
            camera.orbit(.35f, pitch - startPitch);
            camera.orthographic = ortho;
            renderer.draw(asset.scene, camera, 1366, 768);
            std::vector<unsigned char> pixels(1366*768*3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0,0,1366,768,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            for(int y=0;y<768;y++)for(int x:{8,1357})for(int c=0;c<3;c++) {
                int expected=c==2?209:217;
                if(std::abs(int(pixels[(y*1366+x)*3+c])-expected)>2)
                    throw std::runtime_error("Studio changes background across the horizon at pitch " + std::to_string(pitch));
            }
            renderer.capture(root / "output/runtime/camera-fix" /
                ((ortho ? "orthographic-" : "perspective-") + std::to_string(pitch) + ".png"), 1366, 768);
            if(ortho && pitch<0) {
                RenderOptions withoutFloor; withoutFloor.hide={"studio"};
                renderer.draw(asset.scene,camera,1366,768,withoutFloor);
                std::vector<unsigned char> unobscured(pixels.size());
                glReadPixels(0,0,1366,768,GL_RGB,GL_UNSIGNED_BYTE,unobscured.data());
                if(pixels!=unobscured)
                    throw std::runtime_error("Underside of studio floor still obscures low-angle view");
            }
          }
        }
        std::cout << "STUDIO GPU PASS: 10 camera/projection combinations; constant horizon background and no underside occlusion\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
