<?php

// =========================
// CORS headers
// =========================
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=utf-8");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

// =========================
// Database configuration
// =========================
$host = '127.0.0.1';
$db   = 'pbl3_iot';
$user = 'root';
$pass = '';
$charset = 'utf8mb4';

date_default_timezone_set('Asia/Ho_Chi_Minh');

// =========================
// Các tham số bắt buộc
// =========================
$required_params = [
    'temp',
    'humidity',
    'moisture',
    'light'
];

$missing = [];

foreach ($required_params as $param) {
    if (!isset($_GET[$param]) || $_GET[$param] === '') {
        $missing[] = $param;
    }
}

if (!empty($missing)) {
    http_response_code(400);

    echo json_encode([
        "status" => "error",
        "message" => "Thieu tham so bat buoc: " . implode(', ', $missing)
    ]);

    exit;
}

// =========================
// Kiểm tra dữ liệu có phải số
// =========================
foreach ($required_params as $param) {

    if (!is_numeric($_GET[$param])) {

        http_response_code(400);

        echo json_encode([
            "status" => "error",
            "message" => "Tham so '$param' phai la so, nhan duoc: '" . $_GET[$param] . "'"
        ]);

        exit;
    }
}

// =========================
// Chuyển dữ liệu sang kiểu float
// =========================
$temperature   = (float)$_GET['temp'];
$humidity      = (float)$_GET['humidity'];
$soil_moisture = (float)$_GET['moisture'];
$light         = (float)$_GET['light'];

// =========================
// Kiểm tra khoảng giá trị
// =========================
$range_errors = [];

// Nhiệt độ
if ($temperature < -40 || $temperature > 85) {
    $range_errors[] =
        "temp phai trong khoang -40 den 85 (C)";
}

// Độ ẩm không khí
if ($humidity < 0 || $humidity > 100) {
    $range_errors[] =
        "humidity phai trong khoang 0 den 100 (%)";
}

// Độ ẩm đất
if ($soil_moisture < 0 || $soil_moisture > 100) {
    $range_errors[] =
        "moisture phai trong khoang 0 den 100 (%)";
}

// Cường độ ánh sáng
if ($light < 0 || $light > 200000) {
    $range_errors[] =
        "light phai trong khoang 0 den 200000 (lux)";
}

if (!empty($range_errors)) {

    http_response_code(400);

    echo json_encode([
        "status" => "error",
        "message" => "Du lieu vuot nguong hop le",
        "errors" => $range_errors
    ]);

    exit;
}

// =========================
// Kết nối MySQL
// =========================
$dsn = "mysql:host=$host;dbname=$db;charset=$charset";

$options = [
    PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    PDO::ATTR_EMULATE_PREPARES   => false,
];

try {

    $pdo = new PDO($dsn, $user, $pass, $options);

    // =========================
    // Tên bảng
    // =========================
    $table_name = "dulieu";

    // Kiểm tra bảng có tồn tại không
    $stmt = $pdo->prepare("
        SELECT 1
        FROM information_schema.tables
        WHERE table_schema = ?
        AND table_name = ?
        LIMIT 1
    ");

    $stmt->execute([$db, $table_name]);

    if (!$stmt->fetchColumn()) {
        throw new PDOException(
            "Table $table_name does not exist"
        );
    }

    // =========================
    // INSERT dữ liệu
    // =========================
    $sql = "INSERT INTO `$table_name` (
                temp,
                humidity,
                moisture,
                light
            ) VALUES (?, ?, ?, ?)";

    $stmt = $pdo->prepare($sql);

    $stmt->execute([
        $temperature,
        $humidity,
        $soil_moisture,
        $light
    ]);

    // =========================
    // Trả kết quả JSON
    // =========================
    $response = [
        "status" => "thanh cong",
        "message" => "Chen xong du lieu",

        "data" => [
            "temp"     => $temperature,
            "humidity" => $humidity,
            "moisture" => $soil_moisture,
            "light"    => $light
        ]
    ];

    echo json_encode(
        $response,
        JSON_UNESCAPED_UNICODE
    );

} catch (PDOException $e) {

    http_response_code(500);

    echo json_encode([
        "status" => "error",
        "message" => "Database error: " . $e->getMessage()
    ]);
}

?>