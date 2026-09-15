<?php

declare(strict_types=1);

// Replace these values, or provide the matching environment variables.
$dbHost = getenv('IOT_DB_HOST') ?: '127.0.0.1';
$dbName = getenv('IOT_DB_NAME') ?: 'iot_demo';
$dbUser = getenv('IOT_DB_USER') ?: 'root';
$dbPassword = getenv('IOT_DB_PASSWORD') ?: '';

$dsn = "mysql:host={$dbHost};dbname={$dbName};charset=utf8mb4";

try {
	$pdo = new PDO($dsn, $dbUser, $dbPassword, [
		PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
		PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
		PDO::ATTR_EMULATE_PREPARES => false,
	]);
} catch (PDOException $exception) {
	http_response_code(500);
	header('Content-Type: application/json; charset=utf-8');
	echo json_encode(['ok' => false, 'error' => 'Database connection failed']);
	exit;
}
