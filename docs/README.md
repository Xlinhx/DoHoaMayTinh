# Mục lục học và trình bày CLAW LAB

Bộ tài liệu này giải thích từ nền tảng đến mã nguồn của máy gắp 3D. Có thể đọc mà chưa học OpenGL; mỗi chương bắt đầu bằng câu hỏi thực tế, sau đó mới đưa thuật ngữ, công thức, ví dụ và vị trí code. Đọc [00 — Bắt đầu](00-bat-dau.md) trước khi chia nhóm để ba người thống nhất từ vựng.

## Thứ tự theo ba người

1. [01 — Hình học 3D và phép biến đổi](01-hinh-hoc-va-bien-doi.md): mesh → tịnh tiến/quay/co giãn → local/world và phân cấp → Bézier. Đây là phần của **người 1**, giữ thứ tự ảnh phân công.
2. [02 — Camera, chiếu sáng và dựng hình](02-camera-chieu-sang-dung-hinh.md): camera/MVP → Blinn–Phong và vật liệu → depth buffer → shadow mapping. Đây là phần của **người 2**; bổ sung rasterization, normal matrix và nền/sàn để nối đủ pipeline.
3. [03 — Texture, trong suốt và tương tác](03-texture-trong-suot-tuong-tac.md): UV/texture → alpha blending → hoạt hình/state machine → Bresenham và Cohen–Sutherland. Đây là phần của **người 3**; hai thuật toán 2D là bài học mở rộng, chưa tích hợp vào game.
4. [04 — Phân công và trình bày trước thầy](04-trinh-bay-ba-nguoi.md): thời lượng gợi ý, thao tác, chỗ mở code, câu hỏi thường gặp và các điểm cần nói chính xác.
5. [05 — Build, kiểm thử và quản lý dự án](05-build-kiem-thu-bao-tri.md): chạy game, dựng lại, đối chiếu asset, đóng gói, gitignore và quy tắc dọn file.
6. [06 — Đặc tả hiện tại và nguồn đọc thêm](06-dac-ta-va-tham-khao.md): kích thước, ID, giới hạn kỹ thuật, tham số và nguồn sơ cấp.
7. [Điều khiển khi chơi](controls.md): tra nhanh phím/nút, quy tắc trúng/hụt và reset.

## Đọc theo một chức năng

- Máy có hình dạng như thế nào? Đọc 1.1; vì sao tám vật phẩm giống nhau? Đọc 1.1.4 và 6.1.
- Càng đi theo ray và khép ngón ra sao? Đọc 1.2–1.4, rồi 3.3.
- Điểm 3D xuất hiện trên màn hình bằng cách nào? Đọc 0.3 và 2.1.
- Vì sao bề mặt sáng/tối, có highlight? Đọc 2.2. Vì sao vật che nhau? Đọc 2.3.
- Bóng và kính khác nhau thế nào? Đọc 2.4 và 3.2.
- Gắp được vật nào, vì sao không nhảy vị trí hoặc cộng điểm lặp? Đọc 3.3.4–3.3.6.
- Hai thuật toán 2D có thật trong game không? Đọc 3.4: có giải thích, ví dụ và giả mã; chưa có CPU implementation hay demo runtime trong sản phẩm.

## Cách dùng tài liệu có trách nhiệm

**Đã triển khai** nghĩa là có code đang được app gọi. **Authoring** nghĩa là việc thực hiện trong Blender/Python trước khi chạy app. **Mở rộng** là kiến thức để học tiếp, không phải thành tích cài đặt. Ba nhãn này đặc biệt quan trọng ở Bézier, mô hình chiếu sáng và thuật toán 2D.

Các chương dẫn tới file thật và tên hàm để tìm bằng Ctrl+F. Không khóa số dòng vì số dòng đổi khi format code. Ảnh Blender là thiết kế; chạy ứng dụng mới kiểm được tương tác. Kết quả test chỉ chứng minh những điều đã được assert, không thay thế rubric của giảng viên.

Trong ZIP chạy game, các bài Markdown vẫn đọc offline được; liên kết tới `src/`, `tools/` hoặc nguồn `mockups/` cần mở bộ source dự án đầy đủ. Package chơi không chứa toàn bộ môi trường authoring.
