# 4. Kịch bản trình bày ba người

## 4.1. Thứ tự trong ảnh có phù hợp không?

Có. Người 1 giải thích dữ liệu và vị trí; người 2 biến dữ liệu thành hình ảnh; người 3 giải thích chi tiết bề mặt, kính và quá trình tương tác. Cấu trúc này có quan hệ đầu vào–đầu ra rõ, tránh ba người kể lại toàn dự án ba lần.

Cần tinh chỉnh bốn điểm:

1. Người 2 giới thiệu nhanh cả pipeline trước camera để người nghe biết vertex/fragment ở đâu; dành phần UV và blending cho người 3. Normal được người 1 định nghĩa về hình học, người 2 dùng vào ánh sáng, không cần lặp nguyên phần định nghĩa.
2. Gọi đúng **Blinn–Phong**, không chỉ nói Phong như ảnh. Giải thích công thức Phong như nền tảng so sánh, sau đó chỉ halfway vector trong shader thật.
3. Bézier là phần tạo hình trong Blender; không nói C++ tính curve mỗi frame. Chỉ vào auto handles, hai đoạn bậc ba và mesh export để thể hiện nhóm hiểu nguồn hình dạng.
4. Hai thuật toán 2D trong ảnh chưa được tích hợp. Có thể học/trình bày như kiến thức mở rộng, nhưng nếu thầy yêu cầu “tự cài và chạy trong đồ án” thì tài liệu hoặc giả mã chưa đáp ứng tiêu chí đó. Rubric lớp chưa có trong dự án nên không thể tự kết luận đủ yêu cầu học phần.

## 4.2. Phân thời gian để không lệch tải

Ví dụ nhóm có 18 phút, dùng 1 phút mở đầu, mỗi người 5 phút và 2 phút tổng kết/demo kết quả. Đây là gợi ý, không phải thời lượng đã được thầy duyệt. Nếu chỉ có 12 phút, mỗi người khoảng 3 phút và đưa thuật toán 2D vào phần hỏi đáp/tài liệu; không cố đọc hết chương.

Người 3 có nhiều chủ đề, nên không bắt họ vừa chạy toàn demo vừa giải thích mọi công thức 2D trong cùng 5 phút. Người 1 có thể thao tác game khi người 2 nói, người 2 mở code khi người 3 nói. Cả ba vẫn cần hiểu toàn bộ đường đi input → state → transform → renderer → ảnh để trả lời câu hỏi vượt phần mình.

## 4.3. Chuẩn bị trước giờ trình bày

Build Release, chạy CTest và `camera_floor_regression`; mở executable trong package từ vị trí sẽ dùng khi trình bày. Thử một lượt trúng, một lượt hụt, R giữa chu trình, orbit thấp, đổi projection. Đọc [controls](controls.md) để không nhầm reset scene với đổi góc camera.

Mở sẵn các file `core.cpp`, `lit.vert`, `lit.frag`, `Renderer.cpp`, `build_reference.py`, `curve-control-points.json`. Dùng tìm tên hàm thay vì kéo tới một số dòng đã cũ. Phóng chữ đủ đọc. Không thay asset/shader trực tiếp trong package chính ngay trước buổi nói; nếu dùng tùy chọn A/B, chạy ở cửa sổ phát triển hoặc bản sao.

Ảnh mockup giúp so geometry; ảnh runtime giúp giải thích kết quả. Đừng mở ảnh rồi gọi đó là game đang chạy. Package không cần mạng; nguồn tham khảo và docs nên có sẵn offline.

## 4.4. Mở đầu chung — khoảng một phút

Giới thiệu sản phẩm bằng một việc người chơi làm được: “Người chơi điều khiển càng trong một máy gắp 3D. Chương trình giữ tám vật phẩm có danh tính riêng, chạy được lượt trúng, hụt và chơi lại.” Nói rõ nền tảng C++17/OpenGL 3.3 Core, mô hình nguồn Blender, chuyển động/quy tắc gắp có kiểm soát.

Sau đó giao câu hỏi cho ba phần: máy được tạo/đặt ở đâu; từ đó dựng thành ảnh thế nào; và chi tiết/kính/lượt chơi được xử lý ra sao. Không cần liệt kê mọi thư viện ngay trong lời mở đầu.

## 4.5. Người 1 — khoảng năm phút

**Bước 1:** chỉ khung, ray, capsule và càng trên máy. Giải thích mesh gồm vertex/triangle, normal và UV; mesh chung khác transform từng instance. Mở `Vertex`, `Node::mesh` và đoạn `glDrawElements` khi cần chứng minh đường dữ liệu.

**Bước 2:** WASD ở góc Trên rồi Space ở góc chéo. Giải thích T/R/S bằng một điểm và thứ tự `T*R*S`; dùng ví dụ quay 90° rồi dịch khác dịch rồi quay.

**Bước 3:** theo hierarchy bridge → trolley → head → joint → finger trong `applyPose` và `Scene::worldMatrix`. Chỉ rõ tên Y theo authoring đổi sang Z runtime; không đổi trục GLB hai lần.

**Bước 4:** mở JSON control points, trình bày B(t), t=0/1 và auto handles Blender. Nhấn mạnh mesh ngón không đổi giữa mở/khép, chỉ joint quay.

Kết phần bằng câu nối: “Ta đã có các điểm và normal trong world space. Người 2 sẽ đưa chúng qua camera và shader để thành hình trên màn hình.”

## 4.6. Người 2 — khoảng năm phút

**Bước 1:** đổi 1/2/3/P, dùng một điểm để giải thích M→V→P→clip→NDC→viewport. Phân biệt projection và camera pose; trực giao không làm vật xa thu nhỏ.

**Bước 2:** chỉ highlight thép và bề mặt vải. Mở normal matrix trong `lit.vert`, `normalize(l+v)` và shininess trong `lit.frag`. Gọi mô hình xấp xỉ Blinn–Phong, không nhận là PBR.

**Bước 3:** chọn góc có khung che capsule. Giải thích depth test và depth write bằng hai giá trị 0,2/0,6. GPU thực hiện so depth; chương trình thiết lập pass/state và shader.

**Bước 4:** di chuyển càng, quan sát bóng, mở depth pass rồi `shadowAmount`. Giải thích hai góc nhìn, bias và PCF. Nếu được hỏi môi trường, giải thích studio shadow catcher liền màu, không phải căn phòng vật lý.

Câu nối: “Fragment đã có màu và độ sâu; người 3 sẽ giải thích màu đó lấy từ ảnh ra sao, trộn qua kính thế nào và các pose thay đổi theo lượt chơi.”

## 4.7. Người 3 — khoảng năm phút, thuật toán có thể kéo dài

**Bước 1:** giải thích UV như bản trải, texel khác pixel, mipmap giảm nhiễu khi vật ở xa. Chỉ đoạn base-color texture trong shader và phân biệt texture vải với seam/stitch geometry.

**Bước 2:** dùng công thức source-over RGB, ví dụ đỏ/xanh để chứng minh thứ tự trộn quan trọng. Mở sort kính và `glDepthMask(GL_FALSE)`. Nêu sort theo node là xấp xỉ, không refraction.

**Bước 3:** chạy một lượt trúng hoặc hụt. Mở `FixedClock::steps`, `Round::advance`, `chooseCandidate`: vì sao không phụ thuộc số frame, không cộng điểm mỗi frame và không đổi ID. Đầu gắp chỉ tịnh tiến nên giữ offset là đủ trong hợp đồng hiện tại.

**Bước 4:** nếu thời gian cho phép, trình bày Bresenham bằng đoạn (0,0)→(5,3), rồi Cohen–Sutherland với cửa 0…10 / 0…8. Gắn nhãn “bài học mở rộng chưa tích hợp” trước khi nói để người nghe không hiểu nhầm đây là demo CPU của app.

## 4.8. Các câu hỏi cần tự trả lời được

**Vì sao không dựng toàn bộ máy bằng C++?** Hình học authoring trong Blender giúp giữ đúng thiết kế, runtime tự quản transform, game và render. Nhóm phải phân định phần modeling với phần thuật toán lập trình, không nhận toàn bộ Blender là engine tự viết.

**Ba ngón có phải ba mesh khác?** Dùng chung mesh ngón, khác hierarchy/azimuth và joint transform. Khác draw call không đồng nghĩa khác hình học.

**Bắt vật phẩm bằng va chạm vật lý thật không?** Không. Chọn gần nhất theo anchor/radius/height, giữ offset, rơi đơn giản và guide trong chute. Có kiểm crossing aperture nhưng không có mô phỏng vải/ma sát hay contact solver.

**Vì sao ảnh Blender đẹp khác OpenGL?** Renderer, tone mapping và ánh sáng khác. Geometry/ID được đối chiếu riêng; không dùng pixel-perfect shading làm bằng chứng hình học đúng.

**Bézier và hai thuật toán 2D đã tự cài chưa?** Bézier dùng Blender để author mesh và xuất control points, không có CPU evaluator C++ riêng. Hai thuật toán 2D đang có bài học/giả mã, chưa có implementation trong game. Đây là câu cần trả lời thẳng, không lảng sang “GPU cũng rasterize/clip”.

**Game dùng OpenGL hay thư viện làm hết?** Nhóm viết flow, adapter loader, scene/game/camera và shader/render passes. GLFW lo cửa sổ/context/input; GLM tính toán; TinyGLTF/stb giải mã asset; ImGui vẽ controls; GPU rasterize/depth/blend. Tên thư viện không thay cho hiểu thuật toán, nhưng dùng thư viện cũng không xóa phần nhóm tự thiết kế.

## 4.9. Kết quả học tập và phần còn mở

Sau bài nói, người 1 cần tự dự đoán được transform một child; người 2 giải thích một fragment sáng/che/bóng ra sao; người 3 trace được UV→sample và input→state→pose→score. Mỗi người nên có một ví dụ số và một giới hạn kỹ thuật, không chỉ video đẹp.

Phần cần xác nhận với rubric: có bắt buộc tự cài thuật toán 2D, tự evaluate curve ở CPU, phiên bản/legacy OpenGL hay thư viện bị giới hạn không. Đó là yêu cầu học phần, không thể suy từ sản phẩm chơi được hoặc từ ảnh chia nhóm. Nội dung này không tự thêm tính năng mới vào giao diện game.
