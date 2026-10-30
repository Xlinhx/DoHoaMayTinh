# Bộ tham chiếu máy gắp thú

Mở index.html để xem máy và đổi góc. application-mockup.html xem trước chu trình bằng ảnh tĩnh. Không phải game OpenGL đã chạy.

Nguồn chung: design-source.json, build_reference.py và claw-lab-master.blend. Giữ cùng mesh/material/instance qua mọi góc và pose. SVG kích thước chỉ phục vụ dựng máy, không thuộc giao diện chơi.

Tái tạo: chạy build_reference.py bằng Blender như lệnh trong docstring; sau đó python build_gallery.py và python verify_reference.py. Cần Blender 5.2.2 và Python/Node cho kiểm tra. Không cần MCP hoặc mạng khi mở mockup.

Ứng dụng C++/OpenGL đã có riêng ở `dist/claw-lab/`. Các HTML trong thư mục này vẫn chỉ là bộ tham chiếu ảnh tĩnh. Xem [mục lục học tập](../docs/README.md) và [hướng dẫn tái tạo/kiểm chứng](../docs/05-build-kiem-thu-bao-tri.md).
