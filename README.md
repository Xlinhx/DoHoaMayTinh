# CLAW LAB

Máy gắp 3D C++17 / OpenGL 3.3 Core, tự sinh hình học trong C++ (`ProceduralModel`). Có ba ngón cong Bézier, tám viên nhộng dùng chung lưới, chu trình gắp trúng/hụt, camera đa góc và chơi lại. Hình ảnh dùng Blinn–Phong, kính trong suốt Fresnel và Shadow Mapping; chuyển động mô phỏng theo `FixedClock` có quy tắc, không dùng engine vật lý vật rắn đầy đủ.

## Slide thuyết trình

- **File PowerPoint gốc (18 slide):** [slides/CLAW-LAB-Thuyet-trinh.pptx](slides/CLAW-LAB-Thuyet-trinh.pptx)
- **Bản PDF xem trực tiếp:** [slides/CLAW-LAB-Thuyet-trinh.pdf](slides/CLAW-LAB-Thuyet-trinh.pdf)
- **Chi tiết phân công & ảnh xem trước:** [slides/README.md](slides/README.md)

## Học và trình bày theo ba người

Bắt đầu ở **[mục lục tài liệu](docs/README.md)**. Bộ bài giảng đánh số theo thứ tự phân công:

1. [Hình học 3D, biến đổi, phân cấp và Bézier](docs/01-hinh-hoc-va-bien-doi.md).
2. [Camera/MVP, chiếu sáng, depth buffer và shadow mapping](docs/02-camera-chieu-sang-dung-hinh.md).
3. [Texture, kính, hoạt hình và hai thuật toán 2D mở rộng](docs/03-texture-trong-suot-tuong-tac.md).

Có [bài nhập môn](docs/00-bat-dau.md), [kịch bản trình bày ba người](docs/04-trinh-bay-ba-nguoi.md) và [hướng dẫn build/kiểm thử/bảo trì](docs/05-build-kiem-thu-bao-tri.md). Bézier được tạo trong Blender rồi xuất mesh; Bresenham/Cohen–Sutherland là phần học thêm, chưa có trong game. Docs chỉ rõ công thức, ví dụ, nguồn code và giới hạn.

## Chạy

Mở `dist/claw-lab/claw_lab.exe`, hoặc giải nén `output/claw-lab-windows.zip` rồi mở executable bên trong. Giữ `assets/`, `shaders/` và executable trong cùng thư mục. Bản chơi không cần Blender, Python, compiler hoặc mạng.

WASD/mũi tên căn càng, Space gắp, R chơi lại, kéo chuột xoay, cuộn zoom, 1/2/3 đổi góc, P đổi phép chiếu, Esc thoát. Xem `docs/controls.md` cho đầy đủ.

## Build trên Windows

Cần CMake >=3.24, Ninja, MinGW g++ có C++17 và Python 3. Build đầu tiên cần mạng để tải các source release đã khóa bằng SHA-256; các lần sau dùng bản trong `third_party/`. Không cài gói global.

```powershell
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
ctest --test-dir build/debug --output-on-failure
cmake --preset windows-mingw-release
cmake --build --preset windows-mingw-release
ctest --test-dir build/release --output-on-failure
cmake --install build/release --prefix dist/claw-lab
```

GLAD đã được sinh sẵn trong `vendor/glad/`. Assets đã được xuất sẵn. Nếu chủ đích thay nguồn Blender, xuất lại bằng:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' -b mockups/claw-lab-master.blend --python tools/export_assets.py
python tools/verify_assets.py
```

## Đối chiếu và kiểm tra runtime

```powershell
build/release/claw_lab.exe --hidden --capture-all --verify --size 1400x1400 --output output/runtime/reference
build/release/claw_lab.exe --hidden --benchmark --size 1920x1080 --output output/runtime/performance
python tools/verify_runtime.py dist/claw-lab
```

`--capture-all` xuất 16 góc/tư thế liên quan tới máy từ cùng camera và dữ liệu pose của mockup; bỏ board vật liệu không thuộc gameplay. Framebuffer riêng giữ đúng 1400×1400, không bị Windows cắt theo chiều cao màn hình. `--verify` chạy mười lượt trúng/hụt bằng chính Round và Scene dùng khi chơi. `--shot 02-front`, `--no-glass`, `--no-shadow`, `--unlit`, `--no-texture` chỉ là tùy chọn CLI phục vụ phát triển; không có panel trình diễn trong giao diện.

Ảnh Blender tại `mockups/renders/`; ảnh OpenGL tại `output/runtime/reference/`. `assets/design/export-provenance.json` ràng buộc SHA-256 của manifest, scene gốc và GLB, đồng thời ghi phép so sánh vị trí đỉnh sau export. [Đặc tả hiện tại](docs/06-dac-ta-va-tham-khao.md) ghi hợp đồng thiết kế, giới hạn và nguồn đọc thêm.

## Kiến trúc thư mục & mã nguồn

- `slides/`: slide thuyết trình cuối (`CLAW-LAB-Thuyet-trinh.pptx`, `.pdf`) và ảnh xem trước.
- `docs/`: tài liệu giải thích kiến trúc đồ họa, kịch bản 3 người và đặc tả kỹ thuật.
- `src/core.h`, `src/core.cpp`: scene graph, transform, camera, luật gắp, fixed clock và state machine. Dữ liệu gameplay độc lập renderer.
- `src/assets/SceneAsset.h`: cấu trúc dữ liệu `Vertex`, `Primitive`, `Material`, `SceneAsset`.
- `src/model/ProceduralModel.h`, `src/model/ProceduralModel.cpp`: tự sinh lưới hình khối cơ bản, ngón gắp 2 đoạn Bézier quét 25 vòng và cây phân cấp cảnh.
- `src/graphics/Renderer.*`, `shaders/`: RAII GPU resources, Blinn–Phong, depth/shadow/opaque/glass passes, chụp PNG.
- `src/main.cpp`: context Windows/GLFW, input, thanh điều khiển Dear ImGui, vòng lặp và CLI kiểm chứng.
- `tests/`: các phép thử hành vi (`core_tests.cpp`) và render góc thấp (`camera_floor_render.cpp`).
- `tools/`, `mockups/`: script build/kiểm chứng và file Blender/render tham chiếu thiết kế.

Phần tự viết: logic, transforms/camera, importer adapter, renderer và shader. GLM thực hiện phép toán ma trận; GLFW tạo context/input; TinyGLTF/stb đọc dữ liệu; GPU rasterize, depth test và blend; Dear ImGui vẽ controls. Chi tiết license tại `THIRD_PARTY_NOTICES.md`.
