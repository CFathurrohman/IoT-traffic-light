#include <WiFi.h>
#include <esp_now.h>
#include "SevSeg.h"
SevSeg sevseg;

// esp pusat
uint8_t macPusat[] = {0xC0, 0x5D, 0x89, 0xDD, 0x44, 0xC8};
int waktuHijauB = 15;
int kendaraanB = 0;

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
int deciSeconds = waktuHijauB * 10; // Mulai dari 15 detik = 150 x 0.1s
bool isMerah = false;   // Awalnya lampu merah menyala

// ==============================================================
// VOID SETUP
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  // Setup UART2 di pin 18 (TX) & 19 (RX)
  Serial2.begin(9600, SERIAL_8N1, 19, 18); // RX, TX
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
  detectVehicleIfGreen();
  updateTimerDisplay();
  sevseg.refreshDisplay();
}
// ==============================================================
void setupSevenSegment() {
  byte numDigits = 2;
  byte digitPins[] = {13, 12}; // Hanya pakai 2 digit ds1, ds2
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

void updateTimerDisplay() {
  if (millis() - timer >= 100) {
    timer += 100;
    deciSeconds--;

    if (deciSeconds < 0) {
      // Ganti status lampu setiap 15 detik
      isMerah = !isMerah;
      deciSeconds = 150;

      digitalWrite(LED_MERAH, isMerah ? HIGH : LOW);
      digitalWrite(LED_HIJAU, isMerah ? LOW : HIGH);
    }

    int seconds = deciSeconds / 10;
    sevseg.setNumber(seconds); // Tampilkan sisa detik
  }
}

void detectVehicleIfGreen() {
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
  }

  // Cek pergantian siklus
  if (millis() - cycleStartTime >= waktuHijauB * 1000) {
    Serial.print("[JALUR B] ");
    Serial.print(waktuHijauB);
    Serial.print(" detik hijau selesai. Total Kendaraan: ");
    Serial.println(vehicleCount);

    kirimKendaraanKePusat(vehicleCount); // Kirim data ke pusat

    vehicleCount = 0;
    isMerah = true; // Ganti ke merah
    digitalWrite(LED_MERAH, HIGH);
    digitalWrite(LED_HIJAU, LOW);
    deciSeconds = 150; // 15 detik merah
    cycleStartTime = millis();
  }
}

// ===== Callback ESP-NOW =====
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(int)) {
    memcpy(&waktuHijauB, data, sizeof(int));
    Serial.print("Diterima waktu hijau dari pusat: ");
    Serial.println(waktuHijauB);
  } else {
    Serial.println("Data tidak valid!");
  }
}

void kirimKendaraanKePusat(int jumlah) {
  esp_now_send(macPusat, (uint8_t *)&jumlah, sizeof(jumlah));
  Serial.print("Jumlah kendaraan dikirim ke pusat: ");
  Serial.println(jumlah);
}