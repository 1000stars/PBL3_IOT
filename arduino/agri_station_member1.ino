#include <WiFi.h>
#include <HTTPClient.h>
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
const char* SERVER_URL = "http://192.168.1.12/Api/Post.php";

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
const unsigned long HTTP_INTERVAL = 600000UL;      // 10 phut

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
  // Khong tu dong bat bom neu cam bien do am chua hop le
  if (!soilValid) {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    pumpState = false;
    return;
  }

  bool shouldPump = (soilMoisture < SOIL_THRESHOLD);

  if (shouldPump != pumpState) {
    pumpState = shouldPump;

    digitalWrite(
      RELAY_PIN,
      pumpState ? RELAY_ON : RELAY_OFF
    );

    Serial.print("[RELAY] May bom: ");
    Serial.println(pumpState ? "BAT" : "TAT");
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

  // Cap nhat giao dien OLED moi 1 giay
  if (now - lastOLEDUpdate >= OLED_INTERVAL) {
    lastOLEDUpdate = now;
    updateOLED();
  }

  // Gui du lieu len server moi 10 phut
  if (now - lastHTTPSend >= HTTP_INTERVAL) {
    lastHTTPSend = now;
    sendDataHTTP();
  }

  delay(10);
}
