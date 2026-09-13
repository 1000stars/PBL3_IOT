#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// CẤU HÌNH WIFI & SERVER
// ============================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://your-server.com/api/data";

const int   HTTP_TIMEOUT_MS   = 5000;
const int   HTTP_MAX_RETRY    = 3;

// ============================================================
// NỐI CHÂN
// ============================================================
#define PIN_SOIL_MOISTURE 34   // Analog - ADC1_CH6 (input-only)
#define PIN_DHT11          4   // Digital 1-wire
#define PIN_LIGHT_SENSOR  35   // Analog - ADC1_CH7 (input-only)
#define PIN_RELAY_PUMP    23   // Digital output
#define PIN_OLED_SDA      21
#define PIN_OLED_SCL      22

#define DHTTYPE DHT11
DHT dht(PIN_DHT11, DHTTYPE);

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================================
// NGƯỠNG & THAM SỐ HỆ THỐNG
// ============================================================
int soilMoistureThreshold = 30;      // %
const int ADC_SAMPLES     = 15;      // số lần lấy mẫu để trung bình (giảm nhiễu ADC)

// ---- Non-blocking timing: mỗi tác vụ có mốc thời gian riêng ----
unsigned long lastSensorReadTime = 0;
unsigned long lastOledUpdateTime = 0;
unsigned long lastHttpSendTime   = 0;
unsigned long lastWifiCheckTime  = 0;

const unsigned long SENSOR_READ_INTERVAL = 2000;   // DHT11 không nên đọc nhanh hơn 2s
const unsigned long OLED_UPDATE_INTERVAL = 1000;
const unsigned long HTTP_SEND_INTERVAL   = 10000;
const unsigned long WIFI_CHECK_INTERVAL  = 5000;

// ============================================================
// 4. CẤU TRÚC DỮ LIỆU CẢM BIẾN DÙNG CHUNG
// ============================================================
struct SensorData {
  float soilMoisture   = 0;
  float temperature    = 0;
  float humidity       = 0;
  float lightIntensity = 0;
  bool  pumpStatus     = false;
  bool  dhtValid       = false; // để biết dữ liệu DHT11 có đáng tin không
};

SensorData currentData;

// ================================================================
//                              SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_RELAY_PUMP, OUTPUT);
  digitalWrite(PIN_RELAY_PUMP, LOW);

  // ESP32 ADC: nếu cảm biến ra gần 3.3V ở đầu dải, có thể cần chỉnh attenuation
  analogSetAttenuation(ADC_11db); // cho phép đọc tới ~3.3V

  dht.begin();

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Khong tim thay man hinh OLED");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Tram Nong Nghiep");
    display.println("Dang khoi dong...");
    display.display();
  }

  connectWiFi();

  // (thành viên khác - tự thêm phần cứng của mình đi nhé):
  // keypadInit();
  // jsnSr04tInit();
  // hallSensorInit();
}

// ================================================================
//                    LOOP (non-blocking, không dùng delay())
// ================================================================
void loop() {
  unsigned long now = millis();

  if (now - lastWifiCheckTime >= WIFI_CHECK_INTERVAL) {
    lastWifiCheckTime = now;
    checkWiFiReconnect();
  }

  if (now - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
    lastSensorReadTime = now;
    readAllSensors();
    controlPump();
  }

  if (now - lastOledUpdateTime >= OLED_UPDATE_INTERVAL) {
    lastOledUpdateTime = now;
    updateOLED();
  }

  if (now - lastHttpSendTime >= HTTP_SEND_INTERVAL) {
    lastHttpSendTime = now;
    sendDataHTTP();
  }

  // (thành viên khác - nên viết theo kiểu non-blocking tương tự,
  // dùng millis() riêng cho từng tác vụ, tránh delay() làm chậm cả hệ thống):
  // handleKeypadInput();
  // checkWaterLevel();
  // checkHallSensors();
}

// ================================================================
//                        KẾT NỐI WIFI
// ================================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Dang ket noi WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300); // chỉ chấp nhận delay() ở đây vì đang trong pha kết nối ban đầu
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nDa ket noi WiFi. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nKhong the ket noi WiFi, se thu lai sau.");
  }
}

void checkWiFiReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
}

// ================================================================
//              ĐỌC CẢM BIẾN (có oversampling giảm nhiễu ADC)
// ================================================================
void readAllSensors() {
  currentData.soilMoisture   = readSoilMoisture();
  currentData.lightIntensity = readLightIntensity();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Loi doc DHT11, giu gia tri cu");
    currentData.dhtValid = false;
  } else {
    currentData.temperature = t;
    currentData.humidity    = h;
    currentData.dhtValid    = true;
  }
}

int readAnalogAveraged(int pin) {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200); // giãn cách nhỏ giữa các lần đọc ADC
  }
  return sum / ADC_SAMPLES;
}

float readSoilMoisture() {
  int raw = readAnalogAveraged(PIN_SOIL_MOISTURE);
  // hiệu chỉnh 2 mốc khô hoàn toàn / ướt bão hòa theo cảm biến thực tế đo được
  float percent = map(raw, 4095, 1200, 0, 100);//nhớ đổi cho đúng với giá trị của censor
  return constrain(percent, 0, 100);
}

float readLightIntensity() {
  int raw = readAnalogAveraged(PIN_LIGHT_SENSOR);
  // nếu chuyển sang module I2C chuẩn lux (VD BH1750) thì thay hàm này
  float percent = map(raw, 0, 4095, 0, 100);//nhớ đổi cho đúng với giá trị của censor
  return constrain(percent, 0, 100);
}

// ================================================================
//              ĐIỀU KHIỂN BƠM (relay)
// ================================================================
void controlPump() {
  if (!currentData.dhtValid && currentData.soilMoisture == 0) {
    return; // dữ liệu chưa đáng tin, không ra quyết định vội
  }

  if (currentData.soilMoisture < soilMoistureThreshold) {
    digitalWrite(PIN_RELAY_PUMP, HIGH);
    currentData.pumpStatus = true;
  } else {
    digitalWrite(PIN_RELAY_PUMP, LOW);
    currentData.pumpStatus = false;
  }
  // thành viên phụ trách tưới có thể thêm thời gian bật tối đa (auto-off)
  // và cooldown giữa 2 lần bật để bảo vệ bơm.
}

// ================================================================
//              HIỂN THỊ OLED
// ================================================================
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("== AGRI STATION ==");
  display.printf("Do am dat: %.1f%%\n", currentData.soilMoisture);
  display.printf("Nhiet do : %.1f C\n", currentData.temperature);
  display.printf("Do am khong khi : %.1f%%\n", currentData.humidity);
  display.printf("Anh sang : %.1f%%\n", currentData.lightIntensity);
  display.printf("Bom      : %s\n", currentData.pumpStatus ? "BAT" : "TAT");
  display.display();
  //(thành viên keypad): tạo hàm updateOLEDMenu() riêng cho chế độ cài đặt.
}

// ================================================================
//         GỬI DỮ LIỆU LÊN SERVER QUA HTTP (dùng ArduinoJson + retry)
// ================================================================
void sendDataHTTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Chua co WiFi, bo qua gui du lieu.");
    return;
  }

  // Build JSON payload bằng ArduinoJson (an toàn hơn nối chuỗi tay)
  StaticJsonDocument<256> doc;
  doc["soil"]        = round(currentData.soilMoisture * 10) / 10.0;
  doc["temperature"] = round(currentData.temperature * 10) / 10.0;
  doc["humidity"]    = round(currentData.humidity * 10) / 10.0;
  doc["light"]       = round(currentData.lightIntensity * 10) / 10.0;
  doc["pump"]        = currentData.pumpStatus;
  // các thành viên khác thêm field vào doc, ví dụ:
  // doc["waterDistance"] = waterDistance;
  // doc["hall1"] = hall1State;
  // doc["hall2"] = hall2State;

  String payload;
  serializeJson(doc, payload);

  bool success = false;
  for (int attempt = 1; attempt <= HTTP_MAX_RETRY && !success; attempt++) {
    success = sendDataPOST(payload);
    if (!success && attempt < HTTP_MAX_RETRY) {
      Serial.printf("Gui that bai, thu lai lan %d/%d...\n", attempt + 1, HTTP_MAX_RETRY);
      delay(500);
    }
  }

  if (!success) {
    Serial.println("Gui lai di.");
    // TODO: có thể lưu payload vào buffer/SPIFFS để gửi bù sau khi có mạng lại
  }
}

bool sendDataPOST(const String& payload) {
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT_MS);

  int httpCode = http.POST(payload);
  bool ok = (httpCode >= 200 && httpCode < 300);

  if (httpCode > 0) {
    Serial.println("[POST] Ma phan hoi: " + String(httpCode));
    Serial.println("[POST] Phan hoi: " + http.getString());
  } else {
    Serial.println("[POST] Loi gui du lieu: " + http.errorToString(httpCode));
  }

  http.end();
  return ok;
}

// Nếu server dùng GET thay vì POST, dùng hàm này thay cho sendDataPOST() ở trên
bool sendDataGET(const String& queryString) {
  HTTPClient http;
  http.begin(String(SERVER_URL) + "?" + queryString);
  http.setTimeout(HTTP_TIMEOUT_MS);

  int httpCode = http.GET();
  bool ok = (httpCode >= 200 && httpCode < 300);

  if (httpCode > 0) {
    Serial.println("[GET] Ma phan hoi: " + String(httpCode));
    Serial.println("[GET] Phan hoi: " + http.getString());
  } else {
    Serial.println("[GET] Loi gui du lieu: " + http.errorToString(httpCode));
  }

  http.end();
  return ok;
}
