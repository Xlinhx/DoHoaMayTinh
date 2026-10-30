# Chơi CLAW LAB

Mở `claw_lab.exe`. Máy có tám vật phẩm và càng ba ngón. Điểm chỉ tăng khi vật phẩm đi qua cửa thả tới khay nhận.

- WASD hoặc mũi tên: căn càng trên hai trục của máy, không theo hướng camera.
- Space hoặc nút **Gắp**: bắt đầu một lượt. Giữ Space không tạo lượt lặp.
- Khi tự hạ/gắp/nâng/thả, input di chuyển bị khóa; vẫn xoay camera được.
- R hoặc **Chơi lại**: khôi phục cả tám vật phẩm, hủy lượt hiện tại và xóa điểm. Camera giữ góc đang xem.
- Kéo chuột trái/phải: orbit; cuộn: zoom.
- 1 / **Góc chéo**, 2 / **Trước**, 3 / **Trên**: camera định sẵn. Góc trên mở mái/kính để thấy cơ cấu và vị trí vật phẩm.
- P hoặc nút **Phối cảnh / Trực giao**: đổi phép chiếu.
- Chuyển sang ứng dụng khác: tự tạm dừng. Esc: thoát.

Khi lượt kết thúc, tiếp tục WASD để căn lượt mới. Vật phẩm đã nhận vẫn giữ nguyên ID và nằm trong khay; nó không đủ điều kiện gắp lần nữa. **Chơi lại** khôi phục toàn bộ scene.

Để thử trúng dễ lặp lại: từ trạng thái ban đầu, giữ D khoảng 0,58 giây và S khoảng 0,17 giây để đến vật phẩm giữa bên phải (`prize_03`, tọa độ authoring 0,14 / 0,06). Dùng góc Trên để căn chính xác rồi Space. Để thử hụt, căn một vùng trống. Các tọa độ này chỉ phục vụ ghi chú phát triển; ứng dụng không có panel kỹ thuật.

Mô phỏng chọn vật phẩm gần nhất trong bán kính 45 mm và sai khác cao độ tối đa 35 mm, không ngẫu nhiên. Chuyển động trong chute được dẫn theo đường thiết kế, không mô phỏng vải, ma sát hay rigid body.
