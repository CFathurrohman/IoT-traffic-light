// =================================================================
// =    KODE MASTER CONTROLLER DENGAN DIAGNOSTIK LENGKAP           =
// =================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>

// MAC ADDRESS 
uint8_t macLajurA[] = {0x3C, 0x8A, 0x1F, 0xAE, 0xB4, 0xC0};
uint8_t macLajurB[] = {0x3C, 0x8A, 0x1F, 0xAE, 0x81, 0xFC};
uint8_t macLajurC[] = {0x8C, 0x4F, 0x00, 0x37, 0x38, 0x50};

// Definisi Logika Lampu
#define CYCLE_TIME 120
#define LOST_TIME_PER_PHASE 4
#define NUM_PHASES 3
#define SUM_Y_NORMALIZED 0.90
#define MIN_GREEN_TIME 10
#define MAX_GREEN_TIME 80

// === VARIABEL GLOBAL ===
int siklus = 0;
int kendaraanA = 0, kendaraanB = 0, kendaraanC = 0;
volatile bool dataDiterimaA = false, dataDiterimaB = false, dataDiterimaC = false;
HardwareSerial SerialUART(1);
bool pesanMenungguDicetak = true;

// === DEKLARASI FUNGSI ===
void jalankanSiklusLaluLintasLengkap();
float getVC(float kendaraan) {
  float s = 600;
  float q = (kendaraan / 20.0) * 3600.0;
  return q / s;
}

void hitungDurasiLampu(int& hijauA, int& hijauB, int& hijauC) {
  Serial.println("--- Memulai Perhitungan Durasi Lampu ---");
  Serial.printf("[Kalkulasi] Input Kendaraan: A=%d, B=%d, C=%d\n", kendaraanA, kendaraanB, kendaraanC);

  if (siklus == 0) {
    hijauA = hijauB = hijauC = (CYCLE_TIME - (LOST_TIME_PER_PHASE * NUM_PHASES)) / NUM_PHASES;
    Serial.printf("[Kalkulasi] Siklus awal (Siklus #%d). Durasi hijau dibagi rata: %d detik.\n", siklus + 1, hijauA);
    Serial.println("--- Perhitungan Durasi Selesai ---");
    return;
  }
  
  int totalLostTime = LOST_TIME_PER_PHASE * NUM_PHASES;
  int waktuEfektif = CYCLE_TIME - totalLostTime;
  Serial.printf("[Kalkulasi] Waktu Siklus Efektif: %d detik\n", waktuEfektif);

  float vcA = getVC(kendaraanA);
  float vcB = getVC(kendaraanB);
  float vcC = getVC(kendaraanC);
  Serial.printf("[Kalkulasi] V/C Ratio: A=%.4f, B=%.4f, C=%.4f\n", vcA, vcB, vcC);

  float sumVC = vcA + vcB + vcC;
  if (sumVC == 0) {
    hijauA = hijauB = hijauC = waktuEfektif / 3;
    Serial.println("[Kalkulasi] Tidak ada kendaraan. Durasi hijau dibagi rata.");
    hijauA = constrain(hijauA, MIN_GREEN_TIME, MAX_GREEN_TIME);
    hijauB = constrain(hijauB, MIN_GREEN_TIME, MAX_GREEN_TIME);
    hijauC = constrain(hijauC, MIN_GREEN_TIME, MAX_GREEN_TIME);
    Serial.println("--- Perhitungan Durasi Selesai ---");
    return;
  }
  
  float yA = (vcA / sumVC) * SUM_Y_NORMALIZED;
  float yB = (vcB / sumVC) * SUM_Y_NORMALIZED;
  float yC = (vcC / sumVC) * SUM_Y_NORMALIZED;
  Serial.printf("[Kalkulasi] Rasio Hijau Awal (y): yA=%.4f, yB=%.4f, yC=%.4f\n", yA, yB, yC);
  
  float bobotMin = (float)MIN_GREEN_TIME / waktuEfektif;
  float totalPenyesuaian = 0;
  bool adjust = false;
  if (yA < bobotMin) { totalPenyesuaian += (bobotMin - yA); yA = bobotMin; adjust = true; }
  if (yB < bobotMin) { totalPenyesuaian += (bobotMin - yB); yB = bobotMin; adjust = true; }
  if (yC < bobotMin) { totalPenyesuaian += (bobotMin - yC); yC = bobotMin; adjust = true; }
  
  if (adjust && totalPenyesuaian > 0) {
    Serial.printf("[Kalkulasi] Penyesuaian MIN_GREEN_TIME diperlukan. Total penyesuaian: %.4f\n", totalPenyesuaian);
    float totalVC = 0;
    if (yA > bobotMin) totalVC += vcA;
    if (yB > bobotMin) totalVC += vcB;
    if (yC > bobotMin) totalVC += vcC;
    if (totalVC > 0) {
      if (yA > bobotMin) yA -= totalPenyesuaian * (vcA / totalVC);
      if (yB > bobotMin) yB -= totalPenyesuaian * (vcB / totalVC);
      if (yC > bobotMin) yC -= totalPenyesuaian * (vcC / totalVC);
    }
    Serial.printf("[Kalkulasi] Rasio Hijau Setelah Penyesuaian: yA=%.4f, yB=%.4f, yC=%.4f\n", yA, yB, yC);
  }
  
  float eA = yA * waktuEfektif;
  float eB = yB * waktuEfektif;
  float eC = yC * waktuEfektif;
  
  hijauA = (int)eA;
  hijauB = (int)eB;
  hijauC = (int)eC;
  
  float dA = eA - hijauA;
  float dB = eB - hijauB;
  float dC = eC - hijauC;
  struct Remainder { float decimal; int* waktu; };
  Remainder data[] = {{dA, &hijauA}, {dB, &hijauB}, {dC, &hijauC}};
  for (int i = 0; i < 2; i++) {
    for (int j = i + 1; j < 3; j++) {
      if (data[j].decimal > data[i].decimal) {
        Remainder temp = data[i]; data[i] = data[j]; data[j] = temp;
      }
    }
  }
  int sisa = waktuEfektif - (hijauA + hijauB + hijauC);
  Serial.printf("[Kalkulasi] Sisa detik dari pembulatan: %d\n", sisa);
  int i = 0;
  while (sisa > 0) {
    (*data[i % NUM_PHASES].waktu)++;
    sisa--;
    i++;
  }
  
  Serial.printf("[Kalkulasi] Durasi Hijau Sebelum Constrain: A=%d, B=%d, C=%d\n", hijauA, hijauB, hijauC);
  hijauA = constrain(hijauA, MIN_GREEN_TIME, MAX_GREEN_TIME);
  hijauB = constrain(hijauB, MIN_GREEN_TIME, MAX_GREEN_TIME);
  hijauC = constrain(hijauC, MIN_GREEN_TIME, MAX_GREEN_TIME);
  Serial.println("--- Perhitungan Durasi Selesai ---");
}

String getLaneStatusJson(char targetLajur, char aktifLajur, int waktu_hijau_aktif) {
  StaticJsonDocument<256> doc;
  doc["lajur"] = String(targetLajur);
  if (targetLajur == aktifLajur) {
    doc["waktu_hijau"] = waktu_hijau_aktif;
    doc["status_hijau"] = "HIGH";
    doc["status_merah"] = "LOW";
    doc["status_kuning"] = "LOW";
  } else {
    doc["waktu_hijau"] = 0;
    doc["status_hijau"] = "LOW";
    doc["status_merah"] = "HIGH";
    doc["status_kuning"] = "LOW";
  }
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

String getLaneTransitionJson(char targetLajur, char lajur_sekarang_hijau) {
  StaticJsonDocument<256> doc;
  doc["lajur"] = String(targetLajur);
  if (targetLajur == lajur_sekarang_hijau) {
    doc["status_hijau"] = "LOW";
    doc["status_merah"] = "LOW";
    doc["status_kuning"] = "HIGH";
  } else {
    doc["status_hijau"] = "LOW";
    doc["status_merah"] = "HIGH";
    doc["status_kuning"] = "LOW";
  }
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

String jsonPengirimanKeDb(int hA, int hB, int hC) {
  StaticJsonDocument<512> doc;
  JsonArray summaryArray = doc.to<JsonArray>();
  JsonObject laneA_summary = summaryArray.add<JsonObject>();
  laneA_summary["jalur"] = "A";
  laneA_summary["jumlah_kendaraan"] = kendaraanA;
  laneA_summary["durasi_lampu_hijau"] = hA;
  JsonObject laneB_summary = summaryArray.add<JsonObject>();
  laneB_summary["jalur"] = "B";
  laneB_summary["jumlah_kendaraan"] = kendaraanB;
  laneB_summary["durasi_lampu_hijau"] = hB;
  JsonObject laneC_summary = summaryArray.add<JsonObject>();
  laneC_summary["jalur"] = "C";
  laneC_summary["jumlah_kendaraan"] = kendaraanC;
  laneC_summary["durasi_lampu_hijau"] = hC;
  String jsonReport;
  serializeJson(doc, jsonReport);
  return jsonReport;
}

void kirimKeSemuaLajur(String jsonA, String jsonB, String jsonC) {
  esp_now_send(macLajurA, (uint8_t*)jsonA.c_str(), jsonA.length());
  esp_now_send(macLajurB, (uint8_t*)jsonB.c_str(), jsonB.length());
  esp_now_send(macLajurC, (uint8_t*)jsonC.c_str(), jsonC.length());
  delay(50);
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("[KIRIM] Status pengiriman ke %s: %s\n", macStr, status == ESP_NOW_SEND_SUCCESS ? "Berhasil" : "Gagal");
}

// Fungsi onDataRecv dengan log diagnostik
void onDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len) {
  const uint8_t* macPengirim = esp_now_info->src_addr;

  if (len != sizeof(int)) {
    Serial.printf("[TERIMA] Menerima data dengan ukuran salah (%d bytes)\n", len);
    return;
  }
  
  int jumlah_kendaraan = 0;
  memcpy(&jumlah_kendaraan, incomingData, sizeof(jumlah_kendaraan));

  if (memcmp(macPengirim, macLajurA, 6) == 0 && !dataDiterimaA) {
    Serial.printf("[DIAGNOSTIK onDataRecv] Lajur A diterima. Nilai: %d. Menulis ke variabel kendaraanA...\n", jumlah_kendaraan);
    kendaraanA = jumlah_kendaraan;
    dataDiterimaA = true;
    Serial.printf(">>> [TERIMA] Data valid diterima dari Lajur A (...%02X): %d kendaraan\n", macPengirim[5], kendaraanA);
  } 
  else if (memcmp(macPengirim, macLajurB, 6) == 0 && !dataDiterimaB) {
    Serial.printf("[DIAGNOSTIK onDataRecv] Lajur B diterima. Nilai: %d. Menulis ke variabel kendaraanB...\n", jumlah_kendaraan);
    kendaraanB = jumlah_kendaraan;
    dataDiterimaB = true;
    Serial.printf(">>> [TERIMA] Data valid diterima dari Lajur B (...%02X): %d kendaraan\n", macPengirim[5], kendaraanB);
  } 
  else if (memcmp(macPengirim, macLajurC, 6) == 0 && !dataDiterimaC) {
    Serial.printf("[DIAGNOSTIK onDataRecv] Lajur C diterima. Nilai: %d. Menulis ke variabel kendaraanC...\n", jumlah_kendaraan);
    kendaraanC = jumlah_kendaraan;
    dataDiterimaC = true;
    Serial.printf(">>> [TERIMA] Data valid diterima dari Lajur C (...%02X): %d kendaraan\n", macPengirim[5], kendaraanC);
  }
  else {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", macPengirim[0], macPengirim[1], macPengirim[2], macPengirim[3], macPengirim[4], macPengirim[5]);
    Serial.printf(">>> [TERIMA] Data DIABAIKAN (duplikat/tak dikenal) dari MAC %s. Jumlah data: %d\n", macStr, jumlah_kendaraan);
  }
}

void prosesSiklusLampu(int hijauA, int hijauB, int hijauC) {
  String laneA_json, laneB_json, laneC_json;
  
  Serial.println("\n--- Memulai Proses Siklus Lampu di Lajur ---");

  // Fase A Hijau
  Serial.printf("\n[Proses] Fase A: HIJAU selama %d detik\n", hijauA);
  laneA_json = getLaneStatusJson('A', 'A', hijauA);
  laneB_json = getLaneStatusJson('B', 'A', hijauA);
  laneC_json = getLaneStatusJson('C', 'A', hijauA);
  kirimKeSemuaLajur(laneA_json, laneB_json, laneC_json);
  delay(hijauA * 1000);

  // Transisi A Kuning
  Serial.printf("[Proses] Fase A: Transisi KUNING selama %d detik\n", LOST_TIME_PER_PHASE);
  laneA_json = getLaneTransitionJson('A', 'A');
  laneB_json = getLaneTransitionJson('B', 'A');
  laneC_json = getLaneTransitionJson('C', 'A');
  kirimKeSemuaLajur(laneA_json, laneB_json, laneC_json);
  delay(LOST_TIME_PER_PHASE * 1000);

  // Fase B Hijau
  Serial.printf("\n[Proses] Fase B: HIJAU selama %d detik\n", hijauB);
  laneA_json = getLaneStatusJson('A', 'B', hijauB);
  laneB_json = getLaneStatusJson('B', 'B', hijauB);
  laneC_json = getLaneStatusJson('C', 'B', hijauB);
  kirimKeSemuaLajur(laneA_json, laneB_json, laneC_json);
  delay(hijauB * 1000);

  // Transisi B Kuning
  Serial.printf("[Proses] Fase B: Transisi KUNING selama %d detik\n", LOST_TIME_PER_PHASE);
  laneA_json = getLaneTransitionJson('A', 'B');
  laneB_json = getLaneTransitionJson('B', 'B');
  laneC_json = getLaneTransitionJson('C', 'B');
  kirimKeSemuaLajur(laneA_json, laneB_json, laneC_json);
  delay(LOST_TIME_PER_PHASE * 1000);
  
  // Fase C Hijau
  Serial.printf("\n[Proses] Fase C: HIJAU selama %d detik\n", hijauC);
  laneA_json = getLaneStatusJson('A', 'C', hijauC);
  laneB_json = getLaneStatusJson('B', 'C', hijauC);
  laneC_json = getLaneStatusJson('C', 'C', hijauC);
  kirimKeSemuaLajur(laneA_json, laneB_json, laneC_json);
  delay(hijauC * 1000);

  // Transisi C Kuning
  Serial.printf("[Proses] Fase C: Transisi KUNING selama %d detik\n", LOST_TIME_PER_PHASE);
  laneA_json = getLaneTransitionJson('A', 'C');
  laneB_json = getLaneTransitionJson('B', 'C');
  laneC_json = getLaneTransitionJson('C', 'C');
  kirimKeSemuaLajur(laneA_json, laneB_json, laneC_json);
  delay(LOST_TIME_PER_PHASE * 1000);
  
  Serial.println("\n--- Proses Siklus Lampu di Lajur Selesai ---");
}

bool apakahSiklusBisaDimulai() {
  return (dataDiterimaA && dataDiterimaB && dataDiterimaC) || (siklus == 0);
}

void inisialisasiSiklusBaru() {
  Serial.printf("\n\n=== MEMULAI SIKLUS KE-%d ===\n", siklus + 1);
  pesanMenungguDicetak = true;
}

void laporkanHasilDanKirimKeDb(int hA, int hB, int hC) {
  Serial.println("\n--- Hasil Perhitungan Final ---");
  Serial.printf("Kendaraan: [A:%d, B:%d, C:%d]\n", kendaraanA, kendaraanB, kendaraanC);
  Serial.printf("Durasi Hijau: [A:%d detik, B:%d detik, C:%d detik]\n", hA, hB, hC);
  Serial.println("-----------------------------");
  
  String dataKirimDb = jsonPengirimanKeDb(hA, hB, hC);
  Serial.println("\n--- Mengirim Laporan JSON ke DB (via UART) ---");
  Serial.println(dataKirimDb);
  SerialUART.println(dataKirimDb);
  Serial.println("--------------------------------------------\n");
}

void resetDataUntukSiklusBerikutnya() {
  Serial.println("=== Mereset data untuk pengumpulan siklus berikutnya. ===");
  kendaraanA = 0;
  kendaraanB = 0;
  kendaraanC = 0;
  dataDiterimaA = false;
  dataDiterimaB = false;
  dataDiterimaC = false;
  Serial.println("[Reset] Nilai kendaraan A, B, C dan flag 'dataDiterima' telah direset.");
}

void akhiriSiklusSaatIni() {
  siklus++;
  Serial.printf("=== SIKLUS KE-%d SELESAI ===\n", siklus);
}

void jalankanSiklusLaluLintasLengkap() {
  if (!apakahSiklusBisaDimulai()) {
    if (pesanMenungguDicetak) {
      Serial.printf("\n=== MENUNGGU DATA SIKLUS KE-%d ===\n", siklus + 1);
      Serial.printf("Status Penerimaan: A=%s, B=%s, C=%s\n", dataDiterimaA ? "OK" : "BELUM", dataDiterimaB ? "OK" : "BELUM", dataDiterimaC ? "OK" : "BELUM");
      pesanMenungguDicetak = false; 
    }
    return;
  }

  if (siklus > 0) { 
    Serial.println("\n>>> [SISTEM] Semua data lajur telah diterima. Memulai proses siklus baru.");
  }

  inisialisasiSiklusBaru(); 
  
  // Cek nilai dan status flag TEPAT SEBELUM perhitungan
  Serial.printf("\n[DIAGNOSTIK FINAL CHECK] Nilai sebelum hitung: A=%d, B=%d, C=%d\n", kendaraanA, kendaraanB, kendaraanC);
  Serial.printf("[DIAGNOSTIK FINAL CHECK] Flags sebelum hitung: A=%s, B=%s, C=%s\n\n", dataDiterimaA ? "true" : "false", dataDiterimaB ? "true" : "false", dataDiterimaC ? "true" : "false");

  int hA, hB, hC;
  hitungDurasiLampu(hA, hB, hC);
  
  laporkanHasilDanKirimKeDb(hA, hB, hC);
  
  resetDataUntukSiklusBerikutnya(); 
  
  prosesSiklusLampu(hA, hB, hC);
  
  akhiriSiklusSaatIni();
}

void setupPeersEspNow() {
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  memcpy(peerInfo.peer_addr, macLajurA, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("[Setup] Gagal menambahkan peer Lajur A");
    return;
  }
  memcpy(peerInfo.peer_addr, macLajurB, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("[Setup] Gagal menambahkan peer Lajur B");
    return;
  }
  memcpy(peerInfo.peer_addr, macLajurC, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("[Setup] Gagal menambahkan peer Lajur C");
    return;
  }
  Serial.println("[Setup] Peer Lajur A, B, dan C berhasil ditambahkan.");
}

void setupWiFiDanEspNow() {
  WiFi.mode(WIFI_STA);
  Serial.print("[Setup] MAC Address Master: ");
  Serial.println(WiFi.macAddress());
  if (esp_now_init() != ESP_OK) {
    Serial.println("[Setup] Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("[Setup] ESP-NOW berhasil diinisialisasi.");
}

void setup() {
  Serial.begin(115200);
  SerialUART.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("\n====== MASTER CONTROLLER START ======");

  setupWiFiDanEspNow();
  setupPeersEspNow();
  
  Serial.println("\nSetup selesai. Controller siap.");
}

void loop() {
  jalankanSiklusLaluLintasLengkap();
  delay(100); 
}