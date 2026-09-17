<?php
// CORS headers
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

// Thông tin kết nối database
$host = '127.0.0.1';
$db   = 'pbl3';
$user = 'root';// chú ý
$pass = '';// chú ý
$charset = 'utf8mb4';

date_default_timezone_set('Asia/Ho_Chi_Minh');
$current_time = date('Y-m-d H:i:s');

// ----- Validate: kiểm tra đủ tham số bắt buộc -----
$required_params = ['temperature', 'humidity', 'soil_moisture', 'light'];
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

// ----- Validate: kiểm tra tham số là số hợp lệ -----
foreach ($required_params as $param) {
    if (!is_numeric($_GET[$param])) {
        http_response_code(400);
        echo json_encode([
            "status" => "error",
            "message" => "Tham so '$param' phai la so, nhan duoc: '{$_GET[$param]}'"
        ]);
        exit;
    }
}

$temperature   = (float)$_GET['temperature'];
$humidity      = (float)$_GET['humidity'];
$soil_moisture = (float)$_GET['soil_moisture']; // đơn vị %
$light         = (float)$_GET['light'];         // đơn vị lux

// ----- Validate: kiểm tra range hợp lý -----
$range_errors = [];
if ($temperature < -40 || $temperature > 85) {
    $range_errors[] = "temperature phai trong khoang -40 den 85 (C)";
}
if ($humidity < 0 || $humidity > 100) {
    $range_errors[] = "humidity phai trong khoang 0 den 100 (%)";
}
if ($soil_moisture < 0 || $soil_moisture > 100) {
    $range_errors[] = "soil_moisture phai trong khoang 0 den 100 (%)";
}
if ($light < 0 || $light > 200000) {
    $range_errors[] = "light phai trong khoang 0 den 200000 (lux)";
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

// PDO connection
$dsn = "mysql:host=$host;dbname=$db;charset=$charset";
$options = [
    PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    PDO::ATTR_EMULATE_PREPARES   => false,
];

try {
    $pdo = new PDO($dsn, $user, $pass, $options);

    // Check if node table exists
    $table_name = "dulieu";// chú ý
    $stmt = $pdo->prepare("SELECT 1 FROM information_schema.tables WHERE table_schema = ? AND table_name = ? LIMIT 1");
    $stmt->execute([$db, $table_name]);

    if (!$stmt->fetchColumn()) {
        throw new PDOException("Table $table_name does not exist");
    }

    // Insert data into node table
    $sql = "INSERT INTO `$table_name` (
            temperature,
            humidity,
            soil_moisture,
            light,
            update_time
        ) VALUES (?, ?, ?, ?, ?)";

    $stmt = $pdo->prepare($sql);

    $stmt->execute([
        $temperature,
        $humidity,
        $soil_moisture,
        $light,
        $current_time
    ]);

    $response = [
        "status" => "thanh cong",
        "message" => "chen xong du lieu",
        "timestamp" => $current_time,
        "data" => [
            "temperature"   => $temperature,
            "humidity"      => $humidity,
            "soil_moisture" => $soil_moisture,
            "light"         => $light
        ]
    ];

    echo json_encode($response);

} catch (PDOException $e) {
    echo json_encode([
        "status" => "error",
        "message" => "Database error: " . $e->getMessage(),
        "sql_error_info" => $e->errorInfo ?? null
    ]);
}
?>