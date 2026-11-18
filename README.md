Vi điều khiển: ESP32  
Cảm biến: cảm biến DHT11, Độ ẩm đất giao tiếp I2c, Cường độ ánh sáng BH1750
Màn Hình Oled 1.3
Thiết bị điều khiển: Động cơ bơm nước (bật/tắt tưới)  
Môi trường mô phỏng (giai đoạn 1): XAMPP chạy local host  
Giao tiếp sử dụng HTPP và MQTTx
Giai đoạn 2 (sau khi hoàn thiện): Đưa hệ thống lên Internet qua Cloudflare Tunnel, gắn tên miền tĩnh aaaaa
jisdjifjisjfisdji
Phân chia vai tròhdjwhfwehfuweh

Member 1: Arduino & Firmware (Khôi)  
Vai trò: lập trình vi điều khiển đọc cảm biến, gửi dữ liệu qua HTTP (GET/POST) lên server. Làm việc các file .ino  
Cách test độc lập:  
Dùng Serial Monitor để kiểm tra dữ liệu cảm biến đọc được có đúng không trước khi gửi đi.
Gửi thử HTTP request tới một endpoint giả (webhook.site hoặc một file PHP đơn giản chỉ in dữ liệu nhận được) để chắc chắn định dạng gửi đúng như API contract.

Member 2: API (GET,POST, Control, Api xác thực) & Database (Hoàng Và Phúc )   
Vai trò: thiết kế database, xây dựng các API nhận dữ liệu từ Arduino và cung cấp dữ liệu cho frontend. Làm việc với file (get.php, post.php, config.php)  
Cách test độc lập:  
Thống nhất trước "API contract" (endpoint, tham số, response mẫu dạng {status, message, data}).  
Dùng Postman hoặc curl để gửi request giả lập, không cần chờ Arduino hay frontend thật.
 php/
    ├── post.php            ← ESP32 → Database
    ├── get.php             ← Database → Web
    ├── control.php         ← Web → lệnh điều khiển
    └── login.php           ← đăng nhập  
    API xác thực: nhận ID + mật khẩu, kiểm tra đúng/sai trong DB, trả về kết quả (thành công → tạo session, thất bại → báo lỗi).    
    Không có: phân quyền admin/user, đổi mật khẩu, tạo tài khoản mới, quên mật khẩu.    
    Cách test độc lập:    
    Tạo sẵn  tài khoản mẫu thẳng trong DB    
    Gọi API login qua Postman với 3 trường hợp: đúng ID + đúng mật khẩu, đúng ID + sai mật khẩu, ID không tồn tại — kiểm tra response và việc tạo session có đúng không.    
    Test giao diện: nhập đúng → chuyển trang (redirect) thành công; nhập sai → hiện thông báo lỗi rõ ràng.
    
Member 3: Frontend & giao diện tương tác người dùng, login  ( sơn )  
Vai trò: xây dựng giao diện hiển thị dữ liệu thiết bị, tương tác điều khiển (nếu có). Làm việc với file index.html, style.css, Jason.js, Login folder 
Giao diện login: 1 form nhập ID (username) và mật khẩu.   
* Mở rộng: xem được dự báo thời tiết là lợi thế
  Cách test độc lập:  
Dùng dữ liệu JSON giả (mock data) đúng theo API contract để code trước, không cần chờ người 2 xong API thật.
Sau đó thay mock data bằng API thật để kiểm tra tích hợp.

### API trạng thái động cơ

Trang web đăng ký topic MQTT `trangthai` và chuyển trạng thái nhận được lên `/API/status.php`.
API nhận `POST` JSON dạng `{"set":"ON","payload":"OFF"}` và lưu trạng thái mới nhất vào bảng `motor_status` trong database `pbl3my` (bảng được tạo tự động). Gửi `GET /API/status.php` để đọc trạng thái hiện tại.

### Truy cập dashboard từ nhiều máy tính

- Chạy Apache, PHP và MySQL trên **một máy chủ duy nhất**; ESP32 và tất cả trình duyệt phải dùng chung máy chủ/API và database này.
- Trong cùng mạng LAN, máy tính khác mở `http://<IP-máy-chạy-XAMPP>/PBL3_IOT/web/`. Cho phép Apache qua Windows Firewall nếu cần.
- Nếu truy cập ngoài mạng LAN, trỏ domain/Cloudflare Tunnel về Apache; không mở cổng MySQL ra Internet.
- Dashboard tải lịch sử biểu đồ mỗi 60 giây và kiểm tra bản ghi cảm biến mới nhất mỗi 5 giây. Vì vậy các trình duyệt cùng hiển thị dữ liệu từ database dùng chung, thường cập nhật trong vòng 5 giây sau khi ESP32 gửi dữ liệu.
- Trang đăng nhập hiện chưa kết nối API xác thực; chỉ nên truy cập qua mạng tin cậy cho đến khi xác thực được triển khai trước khi mở website ra Internet.
