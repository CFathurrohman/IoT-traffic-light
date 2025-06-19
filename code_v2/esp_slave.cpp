#include <WiFi.h>
#include <esp_now.h>
#include "SevSeg.h"
// Library JSON
#include <ArduinoJson.h> 
#include <Arduino.h>
// End library JSON
SevSeg sevseg;
StaticJsonDocument<200> doc;

// esp pusat
uint8_t macPusat[] = {0xC0, 0x5D, 0x89, 0xDD, 0x44, 0xC8};
int waktuHijauC = 10;
int kendaraanC = 0;
String status_hijau = "LOW";
String status_kuning = "LOW";
String status_merah = "HIGH";

// DEFINE PIN ULTRASONIC
#define TRIG_PIN 25
#define ECHO_PIN 35
#define LED_MERAH 32
#define LED_HIJAU 33

//variabel untuk menghitung jarak/detik
float duration_second;
float distance_cm;

//variabel sebagai penentu jarak ke tanah
float baselineDistance = 0.0;
bool isCalibrated = false;

//variabel pendeteksi kendaraan
bool isVehicleDetected = false;
unsigned int vehicleCount = 0;

//variabel mulainya siklus waktu
unsigned long cycleStartTime;

// Variabel global untuk timer
unsigned long timer = 0;
int deciSeconds = waktuHijauC * 10; // Mulai dari 15 detik = 150 x 0.1s
bool isMerah = false;   // Awalnya lampu merah menyala

// input dari esp master
String jsonInput;
DeserializationError error = deserializeJson(doc, jsonInput);

// ==============================================================
// VOID SETUP
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW INIT GAGAL");
    while (true);
  }
  esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t peer = {};
  peer.channel = 0;
  peer.encrypt = false;
  memcpy(peer.peer_addr, macPusat, 6);
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Gagal menambahkan peer pusat!");
  } else {
    Serial.println("Peer pusat berhasil ditambahkan.");
  }

  setupSevenSegment();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // Inisialisasi pin LED
  pinMode(LED_MERAH, OUTPUT);
  pinMode(LED_HIJAU, OUTPUT);
  digitalWrite(LED_HIJAU, HIGH);
  digitalWrite(LED_MERAH, LOW);  // Merah menyala di awal

  cycleStartTime = millis(); // MULAI TIMER CYCLE PERTAMA
}

// ==============================================================
// VOID LOOP, MEMANGGIL DAN MENJALANKAN FUNGSI
void loop() {
  // detectVehicleIfGreen();
  // while (Serial.available()){
  //   // String jsonInput = Serial.readStringUntil('\n');
  //   // DeserializationError error = deserializeJson(doc, jsonInput);

  //   if(!error){
  //     const char* lajur = doc["lajur"];
  //     if (strcmp(lajur, "C") == 0) {
  //       waktuHijauC = doc["waktu_hijau"];
  //       status_hijau = doc["status_hijau"].as<String>();
  //       status_kuning = doc["status_kuning"].as<String>();
  //       status_merah  = doc["status_merah"].as<String>();
  //       Serial.print("Waktu hijau update ke: ");
  //       Serial.print(waktuHijauC);
  //       Serial.println(" detik");
  //     } else {
  //       Serial.println("Lajur bukan C bang. Data diabaikan");
  //     }
  //   } else {
  //     Serial.println("Gagal parsing JSON.");
  //   }
  // }

  if (!isMerah){
    detectVehicleIfGreen(waktuHijauC);
  }
  updateTimerDisplay(status_hijau, status_kuning, status_merah);
  sevseg.refreshDisplay();
}
// ==============================================================
void setupSevenSegment() {
  byte numDigits = 2;
  byte digitPins[] = {12, 13}; // Hanya pakai 2 digit ds1, ds2
  byte segmentPins[] = {14, 27, 18, 22, 21, 26, 23, 19}; // a, b, c, d, e, f, g, dp
  bool resistorsOnSegments = false;
  byte hardwareConfig = COMMON_ANODE;
  bool updateWithDelays = false;
  bool leadingZeros = false;
  bool disableDecPoint = false;

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins,
               resistorsOnSegments, updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(90);
}

void updateTimerDisplay(String status_hijau, String status_kuning, String status_merah) {
  if (millis() - timer >= 100) {
    timer += 100;
    deciSeconds--;

    // Mapping status string ke HIGH/LOW langsung
    if (status_hijau == "HIGH") {
      isMerah = false;
    } else {
      isMerah = true;
    }
    digitalWrite(LED_HIJAU, status_hijau == "HIGH" ? HIGH : LOW);
    digitalWrite(LED_MERAH, status_merah == "HIGH" ? HIGH : LOW);
    // digitalWrite(LED_KUNING, status_kuning == "HIGH" ? HIGH : LOW);

    // Reset timer jika habis
    if (deciSeconds < 0) {
      deciSeconds = waktuHijauC * 10;  // Reset berdasarkan waktu hijau
    }

    int seconds = deciSeconds / 10;
    sevseg.setNumber(seconds);  // Tampilkan ke seven segment
  }
}

void detectVehicleIfGreen(int waktuLampuHijau) {
  static unsigned long lastSampleTime = 0;

  // Kalibrasi hanya sekali
  if (!isCalibrated) {
    float sum = 0.0;
    for (int i = 0; i < 10; i++) {
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);
      float duration = pulseIn(ECHO_PIN, HIGH);
      sum += 0.017 * duration;
      delay(100);
    }
    baselineDistance = sum / 10.0;
    isCalibrated = true;
    Serial.print("Baseline (tanpa kendaraan): ");
    Serial.println(baselineDistance);
  }

  // Sampling setiap 100ms
  if (millis() - lastSampleTime >= 100) {
    lastSampleTime = millis();

    // Hanya deteksi kendaraan saat lampu HIJAU
    if (!isMerah) {
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);
      float duration = pulseIn(ECHO_PIN, HIGH);
      float distance = duration * 0.034 / 2;

      static float lastDistance = distance;
      float thresholdDistance = baselineDistance - 5.0;
      const float vehicleTriggerRange = 3.0;

      if (lastDistance - distance > vehicleTriggerRange && !isVehicleDetected) {
        isVehicleDetected = true;
      }

      if (isVehicleDetected && distance >= thresholdDistance) {
        vehicleCount++;
        Serial.println("Kendaraan Terdeteksi!");
        isVehicleDetected = false;
      }
      lastDistance = distance;
    }

    // Cek pergantian siklus
    if (millis() - cycleStartTime >= waktuLampuHijau * 1000) {
      Serial.print("[JALUR C] ");
      Serial.print(waktuLampuHijau);
      Serial.print(" detik hijau selesai. Total Kendaraan: ");
      Serial.println(vehicleCount);

      kirimKendaraanKePusat(vehicleCount); // Kirim data ke pusat

      vehicleCount = 0;
      // isMerah = true; // Ganti ke merah
      // digitalWrite(LED_MERAH, HIGH);
      // digitalWrite(LED_HIJAU, LOW);
      // deciSeconds = 150; // 15 detik merah
      cycleStartTime = millis();
    }
  }

  // Cek pergantian siklus
  if (millis() - cycleStartTime >= waktuLampuHijau * 1000) {
    Serial.print("[JALUR C] ");
    Serial.print(waktuLampuHijau);
    Serial.print(" detik hijau selesai. Total Kendaraan: ");
    Serial.println(vehicleCount);

    kirimKendaraanKePusat(vehicleCount); // Kirim data ke pusat

    vehicleCount = 0;
    // isMerah = true; // Ganti ke merah
    // digitalWrite(LED_MERAH, HIGH);
    // digitalWrite(LED_HIJAU, LOW);
    // deciSeconds = 150; // 15 detik merah
    cycleStartTime = millis();
  }
}

// ===== Callback ESP-NOW =====
// void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
//   // if (len == sizeof(int)) {
//     memcpy(&jsonInput, data, sizeof(int));
//     Serial.print("Diterima waktu hijau dari pusat: ");
//     Serial.println(waktuHijauC);
//   // } else {
//   //   Serial.println("Data tidak valid!");
//   // }
// }

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  String incomingJson = "";
  for (int i = 0; i < len; i++) {
    incomingJson += (char)data[i];  // Ubah byte jadi String
  }

  Serial.println("Data JSON diterima:");
  Serial.println(incomingJson);

  DeserializationError error = deserializeJson(doc, incomingJson);
  if (!error) {
    const char* lajur = doc["lajur"];
    if (strcmp(lajur, "C") == 0) {
      if (doc.containsKey("waktu_hijau")) {
        waktuHijauC = doc["waktu_hijau"];
        deciSeconds = waktuHijauC * 10;
      }
      if (doc.containsKey("status_hijau")) {
        status_hijau = doc["status_hijau"].as<String>();
      }
      if (doc.containsKey("status_kuning")) {
        status_kuning = doc["status_kuning"].as<String>();
      }
      if (doc.containsKey("status_merah")) {
        status_merah = doc["status_merah"].as<String>();
      }

      cycleStartTime = millis();  // Mulai ulang siklus
    } else {
      Serial.println("Data bukan untuk lajur ini, diabaikan.");
    }
  } else {
    Serial.print("Gagal parsing JSON dari pusat: ");
    Serial.println(error.c_str());
  }
}


void kirimKendaraanKePusat(int jumlah) {
  esp_now_send(macPusat, (uint8_t *)&jumlah, sizeof(jumlah));
  Serial.println("Jalur C");
  Serial.print("Jumlah kendaraan dikirim ke pusat: ");
  Serial.println(jumlah);
}