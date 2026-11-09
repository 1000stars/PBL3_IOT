#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>     // MQTT
#include <ArduinoJson.h>      // Xu ly JSON lenh dieu khien
#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ==================================================
// 1. CAU HINH WIFI VA SERVER
// ==================================================
const char* WIFI_SSID = "HUY HOANG";
const char* WIFI_PASSWORD = "16022005";

// Thay IP_ADDRESS bang IPv4 cua may tinh chay XAMPP
// Kiem tra IPv4 laptop dang chay XAMPP truoc khi su dung HTTP
const char* SERVER_URL = "http://192.168.1.12/Api/Post.php";

// ==================================================
// 1B. CAU HINH MQTT (THU NGHIEM - BROKER CONG KHAI)
// ==================================================
// broker.emqx.io:1883 KHONG ma hoa, ai cung co the publish/subscribe.
// Chi demo de hieu MQTT. KHONG dieu khien relay that tu Broker cong khai.
const char* MQTT_BROKER = "broker.emqx.io";
const uint16_t MQTT_PORT = 1883;

// Theo yeu cau nhom: chi su dung 2 topic control va trangthai.
// CANH BAO: broker cong khai, topic ngan de bi trung voi nguoi khac!
// Chi dung de thu truyen nhan, khong dieu khien bom that qua broker nay.
const char* MQTT_CONTROL_TOPIC = "control";
const char* MQTT_STATUS_TOPIC  = "trangthai";

// AN TOAN: de false khi thu bang broker.emqx.io!
// Khi chuyen sang MQTT broker rieng co xac thuc va kiem tra mach
// moi can nhac bat true de nhan lenh dieu khien relay thuc te.
const bool MQTT_ALLOW_REMOTE_RELAY = false;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000UL;
const unsigned long MQTT_STATUS_SETTLE_DELAY = 5000UL;
// Yeu cau leader: ON thi giu MANUAL_ON cho den khi OFF.
// Neu MAT MQTT/Wi-Fi, quay lai AUTO de tranh bom chay khong the tat tu xa.

const int STATUS_VOLTAGE_ADC_PIN = 32;
const float STATUS_VOLTAGE_ON_THRESHOLD = 2.5f;
const float STATUS_VOLTAGE_MAX = 5.0f;
const unsigned long STATUS_VOLTAGE_SAMPLE_INTERVAL = 2000UL;

// ==================================================
// 2. CAU HINH CHAN
// ==================================================
#define SOIL_PIN    34
#define DHT_PIN     4
#define RELAY_PIN   23

#define DHT_TYPE DHT11

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// ==================================================
// 3. KHOI TAO THIET BI
// ==================================================
DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;

Adafruit_SH1106G display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// ==================================================
// 4. CAU HINH HE THONG
// ==================================================

// Nguong do am dat de bat bom
const float SOIL_THRESHOLD = 30.0;

// Hieu chuan cam bien do am dat
// Can do gia tri thuc te cua cam bien de hieu chuan chinh xac
const int SOIL_DRY_RAW = 4095;
const int SOIL_WET_RAW = 1200;

// Chu ky doc va gui du lieu
const unsigned long SENSOR_INTERVAL = 60000UL;     // 60 giay
const unsigned long PUMP_CHECK_INTERVAL = 2000UL;  // 2 giay
const unsigned long OLED_INTERVAL = 1000UL;        // 1 giay
const unsigned long HTTP_INTERVAL = SENSOR_INTERVAL; // Gui moi khi co mau cam bien moi

// Neu relay kich muc LOW, doi HIGH thanh LOW va nguoc lai
const int RELAY_ON  = HIGH;
const int RELAY_OFF = LOW;

// ==================================================
// 5. BIEN DU LIEU
// ==================================================
float temperature = NAN;
float humidity = NAN;
float soilMoisture = 0;
float lightLux = NAN;

int soilRaw = 0;

bool oledReady = false;
bool lightSensorReady = false;
bool dhtReady = false;
bool wifiReady = false;
bool pumpState = false;
bool soilValid = false;

unsigned long lastSensorRead = 0;
unsigned long lastPumpCheck = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastHTTPSend = 0;

// Dung WifiClient (khong TLS) chi cho demo Broker cong khai 1883.
WiFiClient mqttTransport;
PubSubClient mqttClient(mqttTransport);

enum PumpControlMode { PUMP_AUTO, PUMP_MANUAL_ON };
PumpControlMode pumpMode = PUMP_AUTO;
unsigned long lastMQTTConnectAttempt = 0;
bool mqttStatusPending = false;
unsigned long mqttStatusChangedAt = 0;
float statusVoltage = NAN;
bool statusVoltageValid = false;
unsigned long lastStatusVoltageRead = 0;
bool mqttLinkWasConnected = false;
// ==================================================
// 6. DOC DO AM DAT
// ==================================================
float readSoilMoisture() {
  soilRaw = analogRead(SOIL_PIN);

  // Gia tri ADC cao: dat kho
  // Gia tri ADC thap: dat am
  float moisture = map(
    soilRaw,
    SOIL_DRY_RAW,
    SOIL_WET_RAW,
    0,
    100
  );

  moisture = constrain(moisture, 0, 100);

  soilValid = true;

  Serial.print("[SOIL] ADC = ");
  Serial.print(soilRaw);
  Serial.print(" | Do am dat = ");
  Serial.print(moisture, 1);
  Serial.println("%");

  return moisture;
}

// ==================================================
// 7. DOC CAM BIEN
// ==================================================
void readSensors() {
  Serial.println("\n========== DOC CAM BIEN ==========");

  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (!isnan(newTemp) && !isnan(newHumidity)) {
    temperature = newTemp;
    humidity = newHumidity;
    dhtReady = true;

    Serial.print("[DHT11] Nhiet do: ");
    Serial.print(temperature, 1);
    Serial.println(" C");

    Serial.print("[DHT11] Do am khong khi: ");
    Serial.print(humidity, 1);
    Serial.println(" %");
  } else {
    Serial.println("[LOI] Khong doc duoc DHT11!");
  }

  soilMoisture = readSoilMoisture();

  if (lightSensorReady) {
    float lux = lightMeter.readLightLevel();

    if (lux >= 0) {
      lightLux = lux;

      Serial.print("[BH1750] Anh sang: ");
      Serial.print(lightLux, 1);
      Serial.println(" lux");
    } else {
      Serial.println("[LOI] Doc BH1750 that bai!");
    }
  } else {
    Serial.println("[CANH BAO] BH1750 chua san sang!");
  }

  Serial.println("==================================");
}

// ==================================================
// 8. DIEU KHIEN MAY BOM
// ==================================================
void controlPump() {
  // Theo leader: MANUAL_ON bat bom lien tuc cho den lenh OFF.
  // OFF => AUTO, KHONG phai tat bom cuong buc.
  bool shouldPump = (pumpMode == PUMP_MANUAL_ON) ||
                    (pumpMode == PUMP_AUTO && soilValid && (soilMoisture < SOIL_THRESHOLD));

  if (shouldPump != pumpState) {
    pumpState = shouldPump;
    digitalWrite(RELAY_PIN, pumpState ? RELAY_ON : RELAY_OFF);
    Serial.print("[RELAY] May bom: ");
    Serial.println(pumpState ? "BAT" : "TAT");
    mqttStatusChangedAt = millis();
    mqttStatusPending = true;
  }
}

void updateStatusVoltage() {
  const uint32_t millivolts = analogReadMilliVolts(STATUS_VOLTAGE_ADC_PIN);
  statusVoltage = millivolts / 1000.0f;
  statusVoltageValid = isfinite(statusVoltage) && statusVoltage <= STATUS_VOLTAGE_MAX;
  if (!statusVoltageValid) statusVoltage = NAN;
}

// ==================================================
// 8A. GIAO THUC MQTT: NHAN LENH TREN control / PHAN HOI TREN trangthai
// ==================================================
// set: trang thai relay; PAYLOAD: trang thai dien ap tai GPIO32.
void mqttPublishStatus() {
  if (!mqttClient.connected()) return;

  JsonDocument doc;
  const bool relayActuallyHigh = (digitalRead(RELAY_PIN) == RELAY_ON);
  doc["set"] = relayActuallyHigh ? "ON" : "OFF";
  if (!statusVoltageValid || !isfinite(statusVoltage)) {
    doc["PAYLOAD"] = "UNKNOWN";
  } else if (statusVoltage < STATUS_VOLTAGE_ON_THRESHOLD) {
    doc["PAYLOAD"] = "ON";
  } else if (statusVoltage <= STATUS_VOLTAGE_MAX) {
    doc["PAYLOAD"] = "OFF";
  } else {
    doc["PAYLOAD"] = "UNKNOWN";
  }

  char output[256];
  size_t used = serializeJson(doc, output, sizeof(output));
  if (used == 0 || used >= sizeof(output) - 1) {
    Serial.println("[MQTT] Loi serialize trang thai");
    return;
  }
  if (mqttClient.publish(MQTT_STATUS_TOPIC, output, false)) {
    Serial.print("[MQTT TX] ");
    Serial.print(MQTT_STATUS_TOPIC);
    Serial.print(" -> ");
    Serial.println(output);
    mqttStatusPending = false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, MQTT_CONTROL_TOPIC) != 0) return;
  Serial.print("[MQTT RX] "); Serial.print(topic); Serial.print(" -> ");
  for (unsigned int i=0; i<length; ++i) Serial.print((char)payload[i]);
  Serial.println();

  // Theo leader: subscribe topic control; JSON {"switch":"ON"} / {"switch":"OFF"}.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    return;
  }
  String command = doc["switch"] | "";
  command.trim();
  command.toUpperCase();

  if (command != "ON" && command != "OFF") {
    return;
  }

  // Khong bat relay tu cong khai broker.emqx.io, ke ca khi co nguoi bat nham flag.
  if (!MQTT_ALLOW_REMOTE_RELAY || strcmp(MQTT_BROKER, "broker.emqx.io") == 0) {
    return;
  }

  if (command == "ON") {
    pumpMode = PUMP_MANUAL_ON;
  } else { // "OFF" => AUTO, doc do am dat de quyet dinh bom.
    pumpMode = PUMP_AUTO;
  }
  controlPump();
}

void mqttConnectIfNeeded(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;
  if (lastMQTTConnectAttempt != 0 &&
      now - lastMQTTConnectAttempt < MQTT_RECONNECT_INTERVAL) return;
  lastMQTTConnectAttempt = now;

  // Client ID rieng cho moi ESP32; khong dung trung Client ID voi MQTTX.
  String clientId = "esp32-pbl3-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL), HEX);
  Serial.print("[MQTT] Ket noi ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[MQTT] CONNECTED");
    if (mqttClient.subscribe(MQTT_CONTROL_TOPIC, 0)) {
      Serial.print("[MQTT] SUBSCRIBED: ");
      Serial.println(MQTT_CONTROL_TOPIC);
    } else {
      Serial.println("[MQTT] LOI dang ky topic control");
    }
    mqttLinkWasConnected = true;
  } else {
    Serial.print("[MQTT] Loi ket noi, state = ");
    Serial.println(mqttClient.state());
  }
}

void mqttService(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) mqttConnectIfNeeded(now);
  if (!mqttClient.connected()) {
    // Neu broker bi mat khi dang ON thu cong, tro lai AUTO (fail-safe).
    if (mqttLinkWasConnected && pumpMode == PUMP_MANUAL_ON) {
      pumpMode = PUMP_AUTO;
      controlPump();
    }
    mqttLinkWasConnected = false;
    return;
  }
  mqttClient.loop();
  if (mqttStatusPending &&
      millis() - mqttStatusChangedAt >= MQTT_STATUS_SETTLE_DELAY) {
    mqttPublishStatus();
  }
}

// ==================================================
// 9. CAP NHAT OLED SH1106
// ==================================================

// Ve bieu tuong WiFi, (cx, cy) la cham tron ben duoi
void drawWifiIcon(int cx, int cy, bool connected) {
  display.fillCircle(cx, cy - 1, 1, SH110X_WHITE);

  // 0xA = hai cung phan tu tren (trai + phai)
  display.drawCircleHelper(cx, cy, 4, 0xA, SH110X_WHITE);
  display.drawCircleHelper(cx, cy, 7, 0xA, SH110X_WHITE);
  display.drawCircleHelper(cx, cy, 10, 0xA, SH110X_WHITE);

  // Mat ket noi: gach cheo
  if (!connected) {
    display.drawLine(cx - 11, 0, cx + 11, 12, SH110X_BLACK);   // vien den de tach net
    display.drawLine(cx - 11, 1, cx + 11, 13, SH110X_BLACK);
    display.drawLine(cx - 11, 0, cx + 11, 11, SH110X_WHITE);   // net gach chinh
  }
}

void updateOLED() {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  // ---------- Header ----------
  display.setCursor(0, 2);
  display.print("Nhom 11");

  drawWifiIcon(115, 11, WiFi.status() == WL_CONNECTED);

  // Duong net dut
  for (int x = 0; x < 128; x += 4) {
    display.drawFastHLine(x, 13, 2, SH110X_WHITE);
  }

  // ---------- Cot trai: cam bien ----------
  display.setCursor(0, 17);
  display.print("Soil : ");
  if (soilValid) {
    display.print(soilMoisture, 0);
    display.print("%");
  } else {
    display.print("--");
  }

  display.setCursor(0, 29);
  display.print("Temp : ");
  if (!isnan(temperature)) {
    display.print(temperature, 1);
    display.print("C");
  } else {
    display.print("--");
  }

  display.setCursor(0, 41);
  display.print("Humi : ");
  if (!isnan(humidity)) {
    display.print(humidity, 0);
    display.print("%");
  } else {
    display.print("--");
  }

  display.setCursor(0, 53);
  display.print("Light: ");
  if (!isnan(lightLux)) {
    display.print(lightLux, 0);
  } else {
    display.print("--");
  }

  // ---------- Cot phai: trang thai bom ----------
  display.drawRoundRect(84, 17, 44, 34, 3, SH110X_WHITE);
  display.setCursor(94, 21);
  display.print("Pump");

  display.setTextSize(2);
  display.setCursor(pumpState ? 94 : 88, 32);
  display.print(pumpState ? "ON" : "OFF");
  display.setTextSize(1);

  display.display();
}

// ==================================================
// 10. GUI DU LIEU LEN SERVER BANG HTTP GET
// ==================================================
void sendDataHTTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi mat ket noi, khong gui du lieu.");
    return;
  }

  if (!dhtReady || !soilValid || isnan(lightLux)) {
    Serial.println("[HTTP] Du lieu cam bien chua day du.");
    return;
  }

  HTTPClient http;

  String url = String(SERVER_URL);
  url += "?temp=" + String(temperature, 1);
  url += "&humidity=" + String(humidity, 1);
  url += "&moisture=" + String(soilMoisture, 1);
  url += "&light=" + String(lightLux, 1);

  Serial.println("\n========== GUI HTTP ==========");
  Serial.print("[HTTP] URL: ");
  Serial.println(url);

  http.setTimeout(5000);

  if (http.begin(url)) {
    int httpCode = http.GET();

    Serial.print("[HTTP] Ma phan hoi: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String response = http.getString();

      Serial.print("[HTTP] Phan hoi server: ");
      Serial.println(response);
    } else {
      Serial.print("[HTTP] Loi: ");
      Serial.println(http.errorToString(httpCode));
    }

    http.end();
  } else {
    Serial.println("[HTTP] Khong khoi tao duoc ket noi HTTP!");
  }

  Serial.println("==============================");
}

// ==================================================
// 11. KET NOI WI-FI
// ==================================================
void connectWiFi() {
  Serial.println("\n========== KET NOI WIFI ==========");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] Dang ket noi");

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < 20000UL) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;

    Serial.println("\n[WiFi] Ket noi thanh cong!");
    Serial.print("[WiFi] IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiReady = false;
    Serial.println("\n[WiFi] Ket noi that bai!");
    Serial.println("[WiFi] Kiem tra ten va mat khau Wi-Fi.");
  }

  Serial.println("==================================");
}

// ==================================================
// 12. KHOI TAO OLED
// ==================================================
void initOLED() {
  Serial.println("\n[OLED] Khoi tao SH1106 tai 0x3C...");

  if (display.begin(OLED_ADDR, true)) {
    oledReady = true;

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.setCursor(25, 20);
    display.println("HELLO");
    display.setTextSize(1);
    display.setCursor(20, 45);
    display.println("ESP32 IoT");
    display.display();

    Serial.println("[OLED] Khoi tao thanh cong!");
    Serial.println("[OLED] Driver SH1106, dia chi 0x3C.");
  } else {
    oledReady = false;
    Serial.println("[OLED] LOI khoi tao OLED!");
    Serial.println("[OLED] Kiem tra day SDA, SCL va nguon.");
  }
}

// ==================================================
// 13. SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("==================================");
  Serial.println("   HE THONG GIAM SAT NONG SAN IoT");
  Serial.println("==================================");

  // Cau hinh relay truoc khi khoi dong cac thiet bi
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // ADC ESP32
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  pinMode(STATUS_VOLTAGE_ADC_PIN, INPUT);
  analogSetPinAttenuation(STATUS_VOLTAGE_ADC_PIN, ADC_11db);
  updateStatusVoltage();

  // Khoi tao cam bien
  dht.begin();

  Wire.begin(21, 22);
  Wire.setClock(100000);

  // Khoi tao OLED SH1106
  initOLED();

  // Khoi tao BH1750
  Serial.println("[BH1750] Khoi tao cam bien anh sang...");

  lightSensorReady = lightMeter.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE
  );

  if (lightSensorReady) {
    Serial.println("[BH1750] Khoi tao thanh cong!");
  } else {
    Serial.println("[BH1750] LOI khoi tao!");
    Serial.println("[BH1750] Kiem tra dia chi va day I2C.");
  }

  // Ket noi Wi-Fi
  connectWiFi();

  // MQTT: no local web server required; MQTTX va ESP32 ket noi cung broker.
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(2); // Han che block khi Broker khong phan hoi
  mqttClient.setBufferSize(512);

  // Doc cam bien ngay khi khoi dong
  readSensors();
  controlPump();

  // Hien thi du lieu
  updateOLED();

  // Khoi tao moc thoi gian
  unsigned long now = millis();
  lastSensorRead = now;
  lastPumpCheck = now;
  lastOLEDUpdate = now;
  lastHTTPSend = now;
  lastStatusVoltageRead = now;

  Serial.println("\n[SETUP] Khoi dong hoan tat!");
}

// ==================================================
// 14. LOOP
// ==================================================
void loop() {
  unsigned long now = millis();

  // Tu ket noi lai Wi-Fi neu bi mat ket noi
  if (WiFi.status() != WL_CONNECTED) {
    wifiReady = false;

    static unsigned long lastReconnect = 0;

    if (now - lastReconnect >= 10000UL) {
      lastReconnect = now;

      Serial.println("[WiFi] Thu ket noi lai...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  } else {
    wifiReady = true;
  }

  // Doc lai toan bo cam bien moi 60 giay
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }

  // Kiem tra do am dat va dieu khien bom moi 2 giay
  if (now - lastPumpCheck >= PUMP_CHECK_INTERVAL) {
    lastPumpCheck = now;

    soilMoisture = readSoilMoisture();
    controlPump();
  }

  // Doc dien ap GPIO32 theo chu ky.
  if (now - lastStatusVoltageRead >= STATUS_VOLTAGE_SAMPLE_INTERVAL) {
    lastStatusVoltageRead = now;
    updateStatusVoltage();
  }

  // Cap nhat giao dien OLED moi 1 giay
  if (now - lastOLEDUpdate >= OLED_INTERVAL) {
    lastOLEDUpdate = now;
    updateOLED();
  }

  // Gui du lieu len server moi 60 giay (theo HTTP_INTERVAL)
  if (now - lastHTTPSend >= HTTP_INTERVAL) {
    lastHTTPSend = now;
    sendDataHTTP();
  }

  // Xu ly MQTT sau cac tac vu cam bien/HTTP.
  mqttService(millis());

  delay(10);
}
