# Third-party notices

The application statically links GLFW 3.4 (zlib/libpng license), GLM 1.0.1 (MIT or modified MIT), GLAD 2.0.8 generated OpenGL 3.3 loader (MIT, Khronos specifications Apache-2.0), TinyGLTF 2.9.5 (MIT), nlohmann/json bundled by TinyGLTF (MIT), stb_image and stb_image_write bundled by TinyGLTF (MIT or public domain), and Dear ImGui 1.91.9b (MIT). Catch2 3.8.1 (BSL-1.0) is used by the test executable only.

Full upstream license text is in `licenses/`. Exact release URLs and SHA-256 archive hashes are in `cmake/dependencies.lock.json` in the source project. The generated loader is checked into `vendor/glad/`; rebuilding it is unnecessary.

The model, reference images, capsule geometry, seams, text geometry and woven texture are authored within this project from the shared design manifest and Blender scene. No downloaded character model, trademark artwork or animal design is included.

On Windows, the UI loads Segoe UI from the existing Windows Fonts directory. The font is not redistributed; a built-in Dear ImGui font is the fallback.
