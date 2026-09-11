Phân chia vai trò

Member 1: Arduino & Firmware  
Vai trò: lập trình vi điều khiển đọc cảm biến, gửi dữ liệu qua HTTP (GET/POST) lên server. Làm việc các file .ino
  Cách test độc lập:
Dùng Serial Monitor để kiểm tra dữ liệu cảm biến đọc được có đúng không trước khi gửi đi.
Gửi thử HTTP request tới một endpoint giả (webhook.site hoặc một file PHP đơn giản chỉ in dữ liệu nhận được) để chắc chắn định dạng gửi đúng như API contract.

Member 2: API (GET/POST) & Database
Vai trò: thiết kế database, xây dựng các API nhận dữ liệu từ Arduino và cung cấp dữ liệu cho frontend. Làm việc với file (get.php, post.php, config.php)
  Cách test độc lập:
Thống nhất trước "API contract" (endpoint, tham số, response mẫu dạng {status, message, data}).
Dùng Postman hoặc curl để gửi request giả lập, không cần chờ Arduino hay frontend thật.

Member 3: Frontend & giao diện tương tác người dùng
Vai trò: xây dựng giao diện hiển thị dữ liệu thiết bị, tương tác điều khiển (nếu có). Làm việc với file index.html, style.css, Jason.js
  Cách test độc lập:
Dùng dữ liệu JSON giả (mock data) đúng theo API contract để code trước, không cần chờ người 2 xong API thật.
Sau đó thay mock data bằng API thật để kiểm tra tích hợp.

Member 4: Đăng nhập & xác thực (API auth)
Vai trò:
Giao diện login: 1 form nhập ID (username) và mật khẩu.
API xác thực: nhận ID + mật khẩu, kiểm tra đúng/sai trong DB, trả về kết quả (thành công → tạo session, thất bại → báo lỗi).
Không có: phân quyền admin/user, đổi mật khẩu, tạo tài khoản mới, quên mật khẩu.
Cách test độc lập:
Tạo sẵn  tài khoản mẫu thẳng trong DB
Gọi API login qua Postman với 3 trường hợp: đúng ID + đúng mật khẩu, đúng ID + sai mật khẩu, ID không tồn tại — kiểm tra response và việc tạo session có đúng không.
Test giao diện: nhập đúng → chuyển trang (redirect) thành công; nhập sai → hiện thông báo lỗi rõ ràng.
