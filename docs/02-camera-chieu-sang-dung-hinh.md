# 2. Người 2 — Camera, chiếu sáng và dựng hình

Mục tiêu: theo được một đỉnh từ mô hình tới màn hình, giải thích màu bề mặt, che khuất và bóng. Thứ tự trong ảnh được giữ nguyên: camera/MVP → ánh sáng/vật liệu → depth buffer → shadow mapping.

## 2.1. Hệ tọa độ, camera và MVP

### 2.1.1. Camera không phải một mesh đang “nhìn”

Camera là dữ liệu xác định cách quan sát: vị trí `eye`, điểm hướng tới `target`, hướng lên `up`, và phép chiếu. Nó có thể không được vẽ trong scene. `Camera::view` trong [core.cpp](../src/core.cpp) gọi `glm::lookAt` để tạo **view matrix** V, đưa world space sang view space.

Có thể hình dung V là phép biến đổi ngược của tư thế camera: camera tiến phải thì toàn cảnh nhìn qua camera dịch trái. Không cần di chuyển mọi vật thể world mỗi lần người chơi kéo chuột.

`Camera::orbit` giữ khoảng cách tới target, thay yaw/pitch rồi tính lại eye. Yaw xoay quanh trục đứng; pitch nâng/hạ góc nhìn. `zoom` thay kích thước trực giao và khoảng cách camera trong các giới hạn. Khi nhìn gần thẳng đứng, `view` chọn up thay thế để tránh hướng nhìn song song up làm hệ cơ sở suy biến.

### 2.1.2. Chuỗi biến đổi đầy đủ

Với p_local=(x,y,z,1), vertex shader [lit.vert](../shaders/lit.vert) thực hiện:

```text
p_world = M * p_local
p_view  = V * p_world
p_clip  = P * p_view
gl_Position = p_clip
p_ndc = p_clip.xyz / p_clip.w
```

M là world matrix từng node, V chung cho camera, P là projection matrix. Vector cột khiến công thức gộp thành `P*V*M*p_local`. Phép chia cho w được pipeline thực hiện sau clipping; shader không cần tự chia trước khi gán `gl_Position`.

**Clip space** dùng tọa độ thuần nhất để cắt primitive theo thể tích nhìn. **NDC** là tọa độ sau chia w, trong miền nhìn OpenGL thông thường mỗi trục thuộc [−1,1]. `glViewport` chuyển x/y sang tọa độ framebuffer, depth range mặc định chuyển z sang [0,1]. Điểm NDC (0,0) ở giữa viewport, không phải góc trái trên. Một điểm ngoài clip volume không nhất thiết làm cả tam giác biến mất: primitive có thể được cắt tại biên.

Ví dụ viewport 800×600 bắt đầu (0,0): NDC x=0,5 cho x cửa sổ gần 600; NDC y=0 cho y gần 300. Đây là tọa độ hình học trước các quy tắc chọn pixel sample. Ảnh PNG thường có hàng đầu ở trên, còn `glReadPixels` đọc từ dưới; `Renderer::capture` đảo hàng để ảnh không lộn ngược.

### 2.1.3. Phối cảnh và trực giao

Phối cảnh mô phỏng vật xa trông nhỏ: khi nhìn dọc −Z trong view space, x_ndc tỷ lệ với `x/(-z)` sau hệ số FOV/aspect. Hai vật bằng nhau ở độ sâu gấp đôi thường có kích thước chiếu bằng một nửa. Trực giao bỏ sự thu nhỏ theo độ sâu, phù hợp đối chiếu hình học và căn vật phẩm.

`Camera::projection` dùng `glm::perspective` với vertical FOV 34°, near 0,02 m và far 100 m; trực giao dùng `glm::ortho`, độ cao nhìn là `orthoScale`, độ rộng bằng độ cao nhân aspect `width/height`. Giá trị kích thước viewport được chặn tối thiểu 1 trong phép tính để tránh chia zero khi minimize; renderer bỏ vẽ framebuffer rỗng.

**FOV** là góc mở; tăng FOV thấy rộng hơn và vật ở giữa thường nhỏ hơn nếu giữ camera. **Near/far** là hai mặt phẳng cắt, không phải “độ xa máy được render sắc nét”. Chọn near quá nhỏ làm phân bổ độ chính xác depth phối cảnh kém hơn ở xa. Chuyển trực giao/phối cảnh không biến đổi mesh hoặc game state.

### 2.1.4. Từ tam giác tới fragment

GPU lắp các vertex thành tam giác, clipping theo clip volume, rồi **rasterization** xác định các sample bị primitive phủ. Thuộc tính như UV và normal được nội suy tới fragment; với shader mặc định, nội suy có hiệu chỉnh phối cảnh. Điều này giúp texture không bị trượt hình khi mặt nghiêng ra xa.

MSAA 4× lưu nhiều mẫu coverage/depth trên một pixel để làm cạnh bớt răng cưa. Nó không tự tăng độ phân giải texture hoặc tạo thêm chi tiết geometry. Framebuffer chụp tham chiếu có resolve từ multisample trước khi đọc PNG.

**Demo:** 1 góc chéo, 2 trước, 3 trên, P đổi projection; kéo chuột và zoom. Chỉ vào `Camera::view`, `Camera::projection`, rồi `gl_Position` trong vertex shader để nối thao tác với toán.

## 2.2. Chiếu sáng và vật liệu

### 2.2.1. Bề mặt sáng theo hướng nào?

Trong [lit.frag](../shaders/lit.frag), n là normal đơn vị, l hướng từ mặt về phía đèn, v hướng từ mặt về phía camera. Tích vô hướng n·l biểu diễn mức hướng về đèn. `max(n·l,0)` bỏ phần âm để mặt quay lưng không nhận diffuse trực tiếp từ đèn đó.

Mô hình cơ bản thường chia thành:

```text
ambient:  ka * Ia
diffuse:  kd * Id * max(n·l,0)
specular Phong:       ks * Is * max(r·v,0)^s
specular Blinn–Phong: ks * Is * max(n·h,0)^s, h=normalize(l+v)
```

r là hướng phản xạ của tia tới; h là halfway vector. Phong và Blinn–Phong dùng hai cách tính highlight khác nhau. Mũ s lớn làm highlight hẹp; s nhỏ làm highlight rộng. Cũng cần phân biệt **Phong reflection model** với **Phong shading**: một bên là mô hình ánh sáng, một bên thường chỉ nội suy normal và tính ánh sáng theo fragment. Dự án tính màu theo fragment và dùng dạng specular Blinn–Phong.

Ví dụ n=(0,1,0), l=(0,1,0): diffuse factor=1. Nếu l nghiêng 60° so với n, factor=0,5. Khi l ở phía đối diện n, diffuse bằng 0 sau max. Đây là mô hình xấp xỉ, không mô phỏng đầy đủ ánh sáng tán xạ nhiều lần.

### 2.2.2. Shader thật khác công thức giáo khoa ra sao?

Shader có key direction `normalize(vec3(3,5,4))`, fill direction `normalize(vec3(-3,2,1))`, ambient phụ thuộc n.y, specular theo halfway vector, emission và tone mapping. `roughness` được ánh xạ sang shininess từ 140 xuống 7; `metallic` trộn màu specular từ màu xám sang base color. Đây là cách điều khiển hình ảnh, **không phải PBR bảo toàn năng lượng** chỉ vì glTF có field roughness/metallic.

Vải có roughness cao nên highlight dịu; thép roughness thấp và metallic cao; nhựa nằm giữa theo manifest. Cùng geometry, đổi tham số material có thể cho cảm giác chất liệu khác. Muốn giống kim loại thật hơn cần BRDF phù hợp, môi trường phản xạ, có thể cần IBL; shader hiện tại chưa có các phần đó.

Ambient là phần sáng nền được đặt thủ công, không phải kết quả global illumination. Emission làm bề mặt tự sáng trong shader, chưa đồng nghĩa nó chiếu sáng lên mọi vật khác. Đèn area trong ảnh Blender không được coi là shader/runtime light đã được import tương đương.

### 2.2.3. Vì sao normal dùng ma trận riêng?

Vị trí chịu ma trận M; normal cần `N=transpose(inverse(mat3(M)))`. Chỉ dùng M cho normal khi scale không đều có thể làm normal không còn vuông góc với mặt. Vertex shader áp N, fragment shader chuẩn hóa lại.

Ví dụ normal n=(1,1,0), tiếp tuyến t=(1,−1,0), ban đầu n·t=0. Scale X gấp 2: t'=(2,−1,0). Nếu normal cũng nhân scale thì n'=(2,1,0), tích vô hướng bằng 3, sai vuông góc. N cho n'=(0.5,1,0), tích với t' bằng 0. Ví dụ dùng dấu chấm thập phân để không lẫn với dấu phân cách tọa độ. Đó là lý do toán học; không chỉ một dòng copy từ tài liệu.

### 2.2.4. Màu tuyến tính và hiển thị

Các mã hex là màu sRGB, không phải cường độ ánh sáng tuyến tính. Texture base color được GPU đọc qua định dạng sRGB để giải mã sang tuyến tính. Material factor trong GLB đã ở miền tuyến tính. Ánh sáng nên tính trong miền này; sau đó tone mapping nén cường độ rồi shader hiện tại dùng lũy thừa 1/2,2 để xấp xỉ gamma hiển thị.

Xấp xỉ gamma 2,2 không chính xác hoàn toàn như hàm chuyển sRGB từng đoạn. Kính/studio còn có nhánh thẩm mỹ riêng; vì vậy không tuyên bố toàn pipeline màu là physically exact. Không thêm bật `GL_FRAMEBUFFER_SRGB` tùy tiện khi shader đã encode vì có thể encode hai lần.

**Câu hỏi tự kiểm:** nếu tăng shininess, highlight rộng hay hẹp? Vì sao camera di chuyển làm specular đổi nhưng diffuse của một hướng đèn cố định có thể không đổi?

## 2.3. Depth buffer và che khuất

### 2.3.1. Vì sao không vẽ xa trước gần sau cho mọi vật?

Hai tam giác có thể giao nhau hoặc che nhau ở từng pixel. Sắp xếp cả object không giải quyết mọi trường hợp. **Depth buffer** lưu độ sâu tại từng sample; fragment mới so sánh độ sâu với giá trị đã lưu. Nếu vượt kiểm tra, nó có thể ghi màu và depth mới.

Renderer bật `GL_DEPTH_TEST`, xóa depth mỗi frame/pass và bật `glDepthMask(GL_TRUE)` cho vật đục. Code dùng depth function mặc định của context, `GL_LESS`, và clear depth mặc định 1. Với pipeline này, giá trị nhỏ hơn thắng; không nên nói “Z world nhỏ hơn luôn thắng”, vì so sánh là depth sau phép chiếu/viewport, không phải trực tiếp world.z.

Ví dụ pixel đang giữ depth=0,4. Fragment có depth=0,6 bị loại; fragment=0,2 được giữ. Với phối cảnh, depth không tuyến tính theo khoảng cách tới camera. Hai mặt gần đồng phẳng có thể bị **z-fighting** vì độ chính xác hữu hạn.

### 2.3.2. Depth test khác depth write

Depth test quyết định fragment có qua so sánh hay không; depth write quyết định có cập nhật bộ nhớ depth hay không. `glDepthMask(GL_FALSE)` không tắt kiểm tra depth. Điều này quan trọng cho kính: kính vẫn cần bị khung máy phía trước che, nhưng không nên chặn sai lớp kính vẽ sau bằng một giá trị depth đã ghi. Người 3 tiếp nối phần này ở 3.2.

Culling lại là kỹ thuật khác: loại mặt theo hướng front/back thay vì khoảng cách. Renderer hiện tắt `GL_CULL_FACE`; không được diễn giải việc che khuất hiện tại là do back-face culling. Shader studio tự discard mặt dưới/mặt bên để bỏ hộp nền authoring khỏi góc nhìn thấp.

### 2.3.3. Nền và môi trường bên ngoài máy

Lỗi đã sửa cho thấy nền phẳng và mesh sàn lớn có thể tạo một đường chân trời đổi màu, hoặc mặt dưới sàn che máy khi orbit xuống. Giải pháp hiện tại giữ góc thấp, dùng mặt trên studio như shadow catcher, bỏ mặt bên/mặt dưới, đồng bộ màu hiển thị với clear color và fade bóng ở góc sát sàn/xa máy.

Đây là một **studio nền liền**, không phải căn phòng có tường/trời vật lý đầy đủ. Máy, kính và các vật thật vẫn tham gia depth bình thường. Đừng lấy việc shader bỏ mặt dưới studio để suy mọi vật đều xuyên được qua nhau.

## 2.4. Shadow mapping

### 2.4.1. Một camera khác từ phía đèn

Depth test của camera trả lời “camera thấy mặt nào?”. Bóng hỏi “đèn thấy mặt đang xét không?”. Shadow mapping thực hiện hai lượt:

1. Từ phía đèn, vẽ vật đục vào depth texture. Mỗi texel lưu độ sâu bề mặt gần đèn nhất trong phép chiếu đó.
2. Từ camera người chơi, lấy vị trí world của fragment, biến đổi sang clip của đèn, chia w và đưa về [0,1]. So độ sâu hiện tại với depth texture để biết có vật chắn trước nó không.

`depth.vert` dùng `lightVP*model*position`; `depth.frag` không ghi màu. `Renderer` tạo depth texture 2048×2048, FBO chỉ có depth attachment, đặt draw/read buffer là `GL_NONE`, kiểm framebuffer completeness. Shadow camera dùng trực giao phù hợp với ánh sáng định hướng xấp xỉ.

### 2.4.2. Bias và lọc

Nếu depth shadow map là 0,30, fragment có depth theo đèn 0,50 thì có vật trước nó; nếu gần 0,30 thì có thể là chính bề mặt đã ghi depth. Sai số số học/raster khiến tự so sánh này tạo **shadow acne**.

Shader trừ bias trước so sánh, với bias phụ thuộc normal–light angle; pass shadow còn có polygon offset. Bias quá nhỏ còn acne, quá lớn làm bóng tách vật (**peter-panning**). Không có một bias thần kỳ đúng mọi kích thước scene và góc.

PCF 3×3 lấy chín phép so sánh quanh vị trí shadow rồi trung bình kết quả để làm mềm biên. Nó trung bình kết quả sáng/tối, không phải lấy trung bình chín depth trước rồi so một lần. PCF không biến directional shadow thành mô phỏng area light vật lý.

### 2.4.3. Giới hạn và demo

Kính không ghi vào shadow pass hiện tại nên không tạo bóng truyền sáng nhuộm màu. Studio không cast shadow nhưng nhận shadow qua shader riêng. Vật ngoài light frustum trả shadow bằng 0; shadow resolution hữu hạn có thể làm biên răng cưa hoặc rung khi di chuyển. Độ mềm và tone màu sẽ khác EEVEE/AgX của mockup.

Demo bằng một lượt di chuyển/nâng càng rồi quan sát bóng. Khi cần A/B cho phát triển, CLI `--no-shadow` có sẵn; không có panel kỹ thuật trong giao diện chơi. Đọc `shadowAmount` và block shadow pass trong Renderer, nối rõ depth từ đèn khác depth framebuffer của camera.

Chuyển lời: “Đã có ánh sáng và che khuất. Người 3 giải thích UV/texture, màu qua kính và cách trạng thái game tạo chuyển động.” Xem [3. Texture, trong suốt và tương tác](03-texture-trong-suot-tuong-tac.md).
