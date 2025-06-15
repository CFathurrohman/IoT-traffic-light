#include <WiFi.h>

// ===== STRUKTUR DATA =====
struct HasilWaktuHijau {
  int hijauA;
  int hijauB;
  int hijauC;
};

// Fungsi pembantu
float getVC(int kendaraan) {
  float s = 600.0;
  float q = (kendaraan / 20.0) * 3600.0;
  return q / s;
}

// Fungsi utama
HasilWaktuHijau hitungWaktuHijau(int kendaraanA, int kendaraanB, int kendaraanC) {
  const int CYCLE_TIME = 120;
  const int LOST_TIME_PER_PHASE = 4;
  const int NUM_PHASES = 3;
  const float SUM_Y_NORMALIZED = 0.90;
  const int MIN_GREEN_TIME = 10;

  int totalLostTime = LOST_TIME_PER_PHASE * NUM_PHASES;
  int waktuEfektif = CYCLE_TIME - totalLostTime;

  float vcA = getVC(kendaraanA);
  float vcB = getVC(kendaraanB);
  float vcC = getVC(kendaraanC);
  float sumVC = vcA + vcB + vcC;

  float yA = (vcA / sumVC) * SUM_Y_NORMALIZED;
  float yB = (vcB / sumVC) * SUM_Y_NORMALIZED;
  float yC = (vcC / sumVC) * SUM_Y_NORMALIZED;

  float bobotMinimal = (float)MIN_GREEN_TIME / waktuEfektif;
  float totalPenyesuaian = 0;

  bool adjustA = false, adjustB = false, adjustC = false;
  if (yA < bobotMinimal) { totalPenyesuaian += (bobotMinimal - yA); yA = bobotMinimal; adjustA = true; }
  if (yB < bobotMinimal) { totalPenyesuaian += (bobotMinimal - yB); yB = bobotMinimal; adjustB = true; }
  if (yC < bobotMinimal) { totalPenyesuaian += (bobotMinimal - yC); yC = bobotMinimal; adjustC = true; }

  float totalVC_sisa = 0;
  if (!adjustA) totalVC_sisa += vcA;
  if (!adjustB) totalVC_sisa += vcB;
  if (!adjustC) totalVC_sisa += vcC;

  if (totalVC_sisa > 0 && totalPenyesuaian > 0) {
    if (!adjustA) yA -= totalPenyesuaian * (vcA / totalVC_sisa);
    if (!adjustB) yB -= totalPenyesuaian * (vcB / totalVC_sisa);
    if (!adjustC) yC -= totalPenyesuaian * (vcC / totalVC_sisa);
  }

  int hijauA = round(yA * waktuEfektif);
  int hijauB = round(yB * waktuEfektif);
  int hijauC = round(yC * waktuEfektif);

  int totalHijau = hijauA + hijauB + hijauC;
  int sisa = waktuEfektif - totalHijau;

  if (sisa > 0) {
    if (yA >= yB && yA >= yC) hijauA += sisa;
    else if (yB >= yA && yB >= yC) hijauB += sisa;
    else hijauC += sisa;
  } else if (sisa < 0) {
    if (yA >= yB && yA >= yC) hijauA += sisa;
    else if (yB >= yA && yB >= yC) hijauB += sisa;
    else hijauC += sisa;
  }

  return { hijauA, hijauB, hijauC };
}

// =========== SETUP ============
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 19, 18); // RX=19, TX=18
  Serial.println("Mock test siap!");
}

// =========== LOOP ============
void loop() {
  // === Mock Data ===
  int kendaraanA = random(5, 20); // contoh random 5-20 kendaraan
  int kendaraanB = random(5, 20);
  int kendaraanC = random(5, 20);

  HasilWaktuHijau hasil = hitungWaktuHijau(kendaraanA, kendaraanB, kendaraanC);

  // Cetak ke Serial monitor
  Serial.printf("Mock: A=%d B=%d C=%d | HijauA=%d HijauB=%d HijauC=%d\n",
                kendaraanA, kendaraanB, kendaraanC,
                hasil.hijauA, hasil.hijauB, hasil.hijauC);

  // Kirim ke Serial2 (kabel ke MCU lain)
  Serial2.printf("%d,%d,%d\n", hasil.hijauA, hasil.hijauB, hasil.hijauC);

  delay(2000); // Ulang setiap 2 detik
}
