#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define RX2_PIN 16
#define TX2_PIN 17

const char* ssid = "LAMBDA_3";
const char* password = "lambda453";
const char* serverUrl = "http://api/traffic/store"; // isi dengan server


unsigned long lastTestTime = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  WiFi.begin(ssid, password);
  Serial.print("🔌 Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected.");
  Serial.print("📶 RSSI: ");
  Serial.println(WiFi.RSSI());
}

void kirimKeServer(const String& data) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi tidak terhubung!");
    return;
  }

  HTTPClient http;
  int httpResponseCode = -1;
  int retry = 0;

  while (retry < 3 && httpResponseCode <= 0) {
    Serial.println("📡 Mengirim data ke server...");
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    httpResponseCode = http.POST(data);

    if (httpResponseCode > 0) {
      Serial.print("✅ Response code: ");
      Serial.println(httpResponseCode);
      Serial.println("📥 Server response: " + http.getString());
    } else {
      Serial.print("❌ Gagal mengirim. Kode: ");
      Serial.println(httpResponseCode);
      retry++;
      delay(1000);
    }

    http.end();
  }

  Serial.print("📶 RSSI: ");
  Serial.println(WiFi.RSSI());
}

void prosesData(const String& jsonString) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.println("❌ Parsing JSON gagal:");
    Serial.println(error.c_str());

    Serial.println("🔍 Dump byte data:");
    for (size_t i = 0; i < jsonString.length(); i++) {
      Serial.print((uint8_t)jsonString[i]);
      Serial.print(" ");
    }
    Serial.println();
    return;
  }

  Serial.println("✅ JSON berhasil di-parse:");
  for (JsonObject obj : doc.as<JsonArray>()) {
    String jalur = obj["jalur"];
    int kendaraan = obj["jumlah_kendaraan"];
    int hijau = obj["durasi_lampu_hijau"];
    Serial.printf("🔸 Jalur: %s | Kendaraan: %d | Hijau: %d detik\n",
                  jalur.c_str(), kendaraan, hijau);
  }

  kirimKeServer(jsonString);
}

void loop() {

  // === Mode real ===
  if (serial2.available()) {
    char buffer[512];
    size_t len = Serial2.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';  

    String receivedData(buffer);

    Serial.println("\n📨 Data diterima dari ESP Master:");
    Serial.println(receivedData);

    prosesData(receivedData);
  }

  delay(100);
}