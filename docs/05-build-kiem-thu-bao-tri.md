# 5. Build, kiểm thử và quản lý dự án

## 5.1. Chạy bản chơi

Mở `dist/claw-lab/claw_lab.exe` hoặc giải nén `output/claw-lab-windows.zip`. Executable phải nằm cạnh `assets/` và `shaders/`. Có thể chạy từ working directory khác vì `resourceRoot` dựa trên đường dẫn executable, không dựa cwd.

Ứng dụng cần Windows và driver hỗ trợ OpenGL 3.3 Core. Bản chơi đã liên kết tĩnh các thư viện runtime cần thiết, không cần Blender/Python/compiler/mạng. Font Segoe UI lấy từ Windows, không phân phối kèm. Xem [controls](controls.md).

## 5.2. Build từ source

Cần MinGW g++ hỗ trợ C++17, CMake ≥3.24, Ninja và Python 3.10+ (script kiểm tra dùng `Path.is_relative_to`). Đã kiểm trên g++ 16.2.0 và Python 3.12. Build đầu tải source dependencies qua mạng; các lần sau dùng cache trong `third_party/`. Không trộn binary MSVC và MinGW.

Chạy ở thư mục gốc dự án:

```powershell
cmake --preset windows-mingw-release
cmake --build --preset windows-mingw-release
ctest --test-dir build/release --output-on-failure
```

Debug dùng `windows-mingw-debug` và `build/debug`. Dependencies được pin tag và archive SHA-256 trong [dependencies.lock.json](../cmake/dependencies.lock.json); script tải chỉ kiểm hash khi tải mới, không audit từng file cache mỗi lần configure. GLAD runtime đã sinh sẵn trong `vendor/glad/`, không cần generator Python khi build. Lock vẫn giữ thông tin bản generator để truy nguồn.

CMake hiện sinh cả `claw_lab`, `claw_tests`, `camera_floor_regression`. Sáu CTest group chạy test logic/import; GPU regression là executable riêng, phải chạy thêm nếu thay renderer/camera:

```powershell
build/release/camera_floor_regression.exe
```

Lệnh đó tạo ảnh trong `output/runtime/camera-fix/`, kiểm 10 góc/phép chiếu, nền qua chân trời và mặt dưới studio. Nó dùng chính `Renderer`/`Camera`, không phải mô hình giả.

## 5.3. Kiểm chứng theo phạm vi

- `claw_tests` / `camera_floor_regression`: `ctest --test-dir build/release --output-on-failure`. Muốn xem tổng assertion: `build/release/claw_tests.exe --reporter compact`. Số case có thể thay đổi; đọc kết quả hiện tại thay vì lấy con số cũ làm PASS.
- Render: `python tools/verify_render_modes.py` sau install; script dùng package ở `dist/claw-lab`. Kiểm các A/B light/glass/shadow có thay đổi pixel và cận cảnh còn vật phẩm mục tiêu. Cần Pillow để phân tích ảnh.
- Package: `python tools/verify_runtime.py dist/claw-lab`. Chạy 10 lượt render trúng/hụt và các lỗi thiếu `design-source.json`/shader, compile/link shader; thử Unicode path, cwd khác, OS-only PATH. Mutations nằm trong bản sao tạm ở `build/`, không sửa package gốc.
- Tài liệu: `python tools/verify_docs.py` kiểm link nội bộ và hành vi gitignore trong repo tạm; không khởi tạo Git ở dự án. Kiểm link/fence không chứng minh nội dung đúng, vẫn cần đối chiếu giải thích với source.

Đã kiểm trên Intel Iris Xe / OpenGL 3.3.0 / driver 32.0.101.7088. Các lần đo cũ ở 1920×1080 không chứng minh hiệu năng sau mọi thay đổi; nếu cần số liệu trình bày, đo lại trên máy dùng bảo vệ bằng `--benchmark`. Phép đo là CPU submission + `glFinish`, bỏ swap/vsync, 60 warm-up và 300 sample; không gọi nó là frame pacing tổng thể.

Có cảnh báo deprecation CMake từ GLM và cảnh báo optimizer trong stb_image upstream ở Release. Phân biệt chúng với lỗi compile/link/runtime; không che warning bằng khẳng định “không có cảnh báo”. Chưa xác minh trên mọi GPU hoặc máy thật thiếu OpenGL 3.3.

## 5.4. Nguồn thiết kế và ảnh kiểm chứng runtime

Tham số kích thước, vị trí 8 viên nhộng và danh sách camera chuẩn lấy từ [design-source.json](../assets/design/design-source.json); toàn bộ lưới 3D được tự sinh trong [ProceduralModel.cpp](../src/model/ProceduralModel.cpp).

```powershell
dist/claw-lab/claw_lab.exe --hidden --capture-all --size 1400x1400 --output output/runtime/reference
```

Camera tham chiếu và state pose lấy manifest; không lấy góc đang chơi ngẫu nhiên để so. `--capture-all` bỏ board vật liệu ngoài gameplay. Cửa sổ Windows có thể bị giới hạn kích thước, nhưng offscreen framebuffer bảo đảm ảnh tham chiếu 1400×1400.

## 5.5. Đóng gói

```powershell
cmake --install build/release --prefix dist/claw-lab
python tools/verify_runtime.py dist/claw-lab
python tools/package.py
```

Đóng game trước khi install để Windows nhả khóa executable. Đọc exit code từng bước: nếu install fail, ZIP tạo từ dist có thể vẫn là bản cũ. `package.py` kiểm byte trong archive, giải nén vào đường dẫn Unicode và chạy 10 lượt từ bản giải nén; không thay thế bước install thành công.

Nếu cấu trúc docs đổi tên, CMake install không tự xóa file cũ trong dist. Dọn riêng các file đã nghỉ dùng trong `dist/claw-lab/docs/`, rồi install. Không xóa cả package đang chạy. Mọi lần bàn giao cần bảo đảm SHA-256 của executable đã build bằng executable trong dist.

## 5.6. Nguồn nào cần giữ trong Git?

Dự án đã được khởi tạo Git và gắn remote `https://github.com/Xlinhx/DoHoaMayTinh.git`. `.gitignore` không tự untrack file đã được commit. `.gitattributes` giữ nguyên byte JSON canonical và giữ định dạng nhị phân cho `.pptx`/`.pdf`/ảnh; source và Markdown checkout với LF.

Giữ source C++/shader, CMake/presets/lock, `vendor/glad` dùng trực tiếp khi build, license, tests, tools, docs, `assets/design/design-source.json` và bộ [slides/](../slides/README.md) phục vụ bảo vệ.

Ignore `build/` (object/cache), `dist/` (installed package), `third_party/` (source tải lại), `output/` (ZIP/ảnh/report sinh lại), cấu hình cá nhân, Python cache và log.

## 5.7. Dọn file theo vòng đời

Các file kế hoạch cũ, bộ mockup Blender trung gian và script export `.glb` không còn dùng sau khi chuyển sang tự sinh lưới bằng C++ (`ProceduralModel.cpp`) đã được dọn gọn để giữ repo sạch, nhẹ và đồng bộ với mã nguồn hiện tại.

Khi muốn dọn thêm, kiểm consumer trước: script nào đọc file, docs nào link nó, CMake có dùng không, có nguồn để tái tạo không. Chỉ xóa sau khi thông tin vận hành đã chuyển sang tài liệu hiện hành và các lệnh build/kiểm đối chiếu vẫn chạy được. Không dùng `git clean -fdx` như một lệnh dọn mặc định.
