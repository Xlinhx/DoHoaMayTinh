# 6. Đặc tả hiện tại và nguồn đọc thêm

## 6.1. Thiết kế chuẩn và các giá trị cần giữ

Nguồn có thẩm quyền: [manifest thiết kế](../assets/design/design-source.json) và bộ sinh hình học C++ [ProceduralModel.cpp](../src/model/ProceduralModel.cpp).

Thân máy rộng 0,90 m, sâu 0,76 m; tổng sâu gồm console 0,91 m; cao 1,80 m. Sàn khoang z=0,70 m trong authoring. Ray X giới hạn [−0,27;0,27], ray Y authoring [−0,18;0,22], đầu gắp z trong [1,00;1,46]. Runtime đổi trục đúng một lần `(x,y,z)→(x,z,−y)`.

Ba ngón cách nhau 120°, góc mở −22°, đóng 0° theo thiết kế.

Vật phẩm chuẩn `prize_capsule_01`: kích thước authoring 0,146×0,118×0,124 m, màu hồng, một đường may và các nút chỉ (`stitches`), không tai/mặt/logo. Tám instance `prize_01`…`prize_08` chia sẻ dữ liệu mesh; target tham chiếu là `prize_03`.

Palette theo ID: cream `#F3EBDD`, teal `#196C67`, orange `#ED773D`, steel `#B6C2C2`, dark `#173B3A`, prize `#E8B4A6`, seam `#A96051`; các giá trị roughness/metallic và LED/kính xem trong manifest và `ProceduralModel.cpp`.

## 6.2. Ranh giới kỹ thuật

Ứng dụng Windows dùng C++17, OpenGL 3.3 Core/GLSL 330, CMake/Ninja/MinGW. GLFW làm context/input; GLAD nạp hàm OpenGL; GLM làm toán; nlohmann/json đọc manifest; Dear ImGui vẽ controls; Catch2 chạy tests. License nằm tại [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) và `licenses/`.

Không có backend, mạng khi chơi, tài khoản, rigid-body solver, mô phỏng vải/ma sát, PBR đầy đủ, refraction, thuật toán 2D tự cài trong runtime hoặc panel giảng bài. Camera cho orbit góc thấp; studio dùng shadow catcher cùng nền chứ không mô phỏng căn phòng bên ngoài. UI chỉ phục vụ chơi.

### 6.2.1. Tại sao chia sẻ mesh chưa đủ chứng minh giữ nguyên mọi thứ?

Shared mesh bắt lỗi thay hình học; cùng material IDs bắt lỗi liên kết bề mặt; ID xuyên lượt bắt thay vật phẩm; camera pose cố định bắt so sai góc. Các phép kiểm bổ sung nhau. Không dùng một fingerprint đơn lẻ để khẳng định tất cả đúng tuyệt đối.

### 6.2.2. Kiểm chứng hiện có và giới hạn

`claw_tests` kiểm logic/procedural model, `camera_floor_regression` kiểm pixel nền/occlusion tại các góc lấy mẫu, các Python tool kiểm runtime/render modes/docs. Runtime qua trúng/hụt không thay thế kiểm UI/focus/resize bằng cửa sổ thật. Chưa có rubric lớp và chưa kiểm mọi GPU; không kết luận đáp ứng mọi tiêu chí môn học.

## 6.3. Nguồn sơ cấp để đọc tiếp

Các nguồn giúp kiểm định nghĩa và quy tắc API, không thay việc mở code dự án. Đọc đúng phiên bản OpenGL 3.3 Core ứng dụng dùng.

1. [OpenGL 3.3 Core Specification — Khronos](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf): tra Vertex Shaders, Primitive Clipping, Rasterization, Texturing và Per-Fragment Operations. Đây là đặc tả API, không phải chứng minh game dùng thuật toán CPU cụ thể.
2. [GLFW quick guide](https://www.glfw.org/docs/latest/quick_guide.html): context, function loader, loop và window/framebuffer events. Đọc song song struct `Window` trong `main.cpp`; cửa sổ GLFW khác renderer tự viết.
3. [The Bresenham Line-Drawing Algorithm — University of Helsinki](https://www.cs.helsinki.fi/group/goa/mallinnus/lines/bresenh.html): dẫn xuất phép chọn E/NE và chuyển sai số sang số nguyên. Bản cơ bản giới hạn một octant; chương 3 thêm giả mã tổng quát và điều kiện kiểm.

## 6.4. Lộ trình học sau bản hiện tại

Người 1 có thể học đạo hàm/tính liên tục Bézier, adaptive tessellation và parameterization theo độ dài cung. Một cải tiến có ý nghĩa là tự evaluate cùng control points rồi đo sai lệch với mesh tham chiếu; đó là công việc mới.

Người 2 có thể học reversed-Z/precision, physically based BRDF, environment mapping, HDR linear framebuffer và cascaded shadows. Trước khi thêm hiệu ứng, nên giải thích được normal matrix, perspective divide và hai depth passes.

Người 3 có thể tự cài/test tám octant Bresenham, Cohen–Sutherland với parallel/boundary cases rồi so Liang–Barsky. Học thêm premultiplied alpha, blending tuyến tính và collision/constraint solver trước khi gọi chuyển động có easing là vật lý.

Các mục là đề xuất học tiếp. Chưa thêm implementation, dependency hay giao diện mới chỉ để phục vụ tài liệu.
