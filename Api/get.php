<?php
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json");
header("Access-Control-Allow-Methods: GET");
header("Access-Control-Allow-Headers: Content-Type");

// Cấu hình database
$host = '127.0.0.1';    
$db   = 'pbl3my';    
$user = 'root';     // chú ý
$pass = '';     // chú ý
$charset = 'utf8mb4';

// Nhận tham số
$start_time = isset($_GET['start_time']) ? $_GET['start_time'].' 00:00:00' : null;
$end_time = isset($_GET['end_time']) ? $_GET['end_time'].' 23:59:59' : null;

try {
    // Kết nối database
    $dsn = "mysql:host=$host;dbname=$db;charset=$charset";
    $options = [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
    ];
    $pdo = new PDO($dsn, $user, $pass, $options);

    // Kiểm tra bảng tồn tại
    $table_name = "dulieu";// chú ý
    $check_table = $pdo->prepare("SELECT 1 FROM information_schema.tables 
                                WHERE table_schema = ? AND table_name = ?");
    $check_table->execute([$db, $table_name]);


    // Xây dựng câu truy vấn
    $sql = "SELECT humidity, tempt, light, moisture, dateupdate 
            FROM `$table_name`";

    $params = [];
    
    // Thêm điều kiện thời gian nếu có
    if ($start_time !== null || $end_time !== null) {
        $conditions = [];
        
        if ($start_time !== null) {
            $conditions[] = "dateupdate >= :start_time";
            $params[':start_time'] = $start_time;
        }
        
        if ($end_time !== null) {
            $conditions[] = "dateupdate <= :end_time";
            $params[':end_time'] = $end_time;
        }
        
        if (!empty($conditions)) {
            $sql .= " WHERE " . implode(" AND ", $conditions);
        }
    } else {
        // Nếu không có điều kiện thời gian, lấy bản ghi mới nhất
        $sql .= " ORDER BY dateupdate DESC LIMIT 1";
    }

    // Thực thi truy vấn
    $stmt = $pdo->prepare($sql);
    $stmt->execute($params);

    // Xử lý kết quả
    $data = ($start_time !== null || $end_time !== null) ? $stmt->fetchAll() : $stmt->fetch();

    // Trả về kết quả
    echo json_encode([
        "status" => "success",
        "time_range" => [
            "start_time" => $start_time,
            "end_time" => $end_time
        ],
        "data" => $data,
        "timestamp" => date('Y-m-d H:i:s')
    ], JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);

} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode([
        "status" => "error",
        "message" => "Database error",
        "details" => $e->getMessage()
    ]);
}
