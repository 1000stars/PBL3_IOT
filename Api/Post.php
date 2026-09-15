<?php

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
	http_response_code(405);
	header('Allow: POST');
	echo json_encode(['ok' => false, 'error' => 'Only POST is allowed']);
	exit;
}

require __DIR__ . '/config.php';

$rawBody = file_get_contents('php://input');
$payload = json_decode($rawBody ?: '', true);

if (!is_array($payload)) {
	http_response_code(400);
	echo json_encode(['ok' => false, 'error' => 'Request body must be valid JSON']);
	exit;
}

$requiredFields = [
	'device_tag',
	'temp_c',
	'humidity_air_pct',
	'soil_moisture_pct',
	'light_lux',
];

foreach ($requiredFields as $field) {
	if (!array_key_exists($field, $payload) || !is_numeric($payload[$field]) && $field !== 'device_tag') {
		if ($field === 'device_tag' && is_string($payload[$field] ?? null) && trim($payload[$field]) !== '') {
			continue;
		}

		http_response_code(422);
		echo json_encode(['ok' => false, 'error' => "Missing or invalid field: {$field}"]);
		exit;
	}
}

$deviceTag = trim((string) $payload['device_tag']);
$temperature = (float) $payload['temp_c'];
$airHumidity = (float) $payload['humidity_air_pct'];
$soilMoisture = (float) $payload['soil_moisture_pct'];
$lightLux = (float) $payload['light_lux'];

if ($airHumidity < 0 || $airHumidity > 100 || $soilMoisture < 0 || $soilMoisture > 100 || $lightLux < 0) {
	http_response_code(422);
	echo json_encode(['ok' => false, 'error' => 'Sensor values are out of range']);
	exit;
}

try {
	$statement = $pdo->prepare(
		'INSERT INTO iot_readings_7k
			(device_tag, temp_c, humidity_air_pct, soil_moisture_pct, light_lux, measured_at)
		 VALUES
			(:device_tag, :temp_c, :humidity_air_pct, :soil_moisture_pct, :light_lux, :measured_at)'
	);
	$statement->execute([
		':device_tag' => $deviceTag,
		':temp_c' => $temperature,
		':humidity_air_pct' => $airHumidity,
		':soil_moisture_pct' => $soilMoisture,
		':light_lux' => $lightLux,
		':measured_at' => date('Y-m-d H:i:s'),
	]);

	http_response_code(201);
	echo json_encode(['ok' => true, 'id' => (int) $pdo->lastInsertId()]);
} catch (PDOException $exception) {
	http_response_code(500);
	echo json_encode(['ok' => false, 'error' => 'Could not save sensor reading']);
}
