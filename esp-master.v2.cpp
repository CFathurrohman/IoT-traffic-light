#include <WiFi.h>
#include <esp_now.h>

// ===== MAC ADDRESS ======
uint8_t macA[] = {0x3C, 0x8A, 0x1F, 0xAE, 0xB4, 0xC0}; // Kelompok 5
uint8_t macB[] = {0x3C, 0x8A, 0x1F, 0xAE, 0x81, 0xFC}; // Kelompok 4
uint8_t macC[] = {0x8C, 0x4F, 0x00, 0x37, 0x38, 0x50}; // Kelompok 1

// ===== Variabel kendaraan =====
int kendaraanA = 0;
int kendaraanB = 0;
int kendaraanC = 0;

// ===== Hasil Waktu Hijau =====
struct HasilWaktuHijau {
  int hijauA;
  int hijauB;
  int hijauC;
};

// ===== Struktur data kirim untuk ESP-NOW =====
struct HijauPacket {
  int hijauA;
  int hijauB;
  int hijauC;
};

// ===== Fungsi pembantu =====
float getVC(int kendaraan)
{
  float s = 600.0;
  float q = (kendaraan / 20.0) * 3600.0;
  return q / s;
}

// ===== Fungsi utama =====
HasilWaktuHijau hitungWaktuHijau(int kendaraanA, int kendaraanB, int kendaraanC)
{
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
  if (yA < bobotMinimal)
  {
    totalPenyesuaian += (bobotMinimal - yA);
    yA = bobotMinimal;
    adjustA = true;
  }
  if (yB < bobotMinimal)
  {
    totalPenyesuaian += (bobotMinimal - yB);
    yB = bobotMinimal;
    adjustB = true;
  }
  if (yC < bobotMinimal)
  {
    totalPenyesuaian += (bobotMinimal - yC);
    yC = bobotMinimal;
    adjustC = true;
  }

  float totalVC_sisa = 0;
  if (!adjustA)
    totalVC_sisa += vcA;
  if (!adjustB)
    totalVC_sisa += vcB;
  if (!adjustC)
    totalVC_sisa += vcC;

  if (totalVC_sisa > 0 && totalPenyesuaian > 0)
  {
    if (!adjustA)
      yA -= totalPenyesuaian * (vcA / totalVC_sisa);
    if (!adjustB)
      yB -= totalPenyesuaian * (vcB / totalVC_sisa);
    if (!adjustC)
      yC -= totalPenyesuaian * (vcC / totalVC_sisa);
  }

  int hijauA = round(yA * waktuEfektif);
  int hijauB = round(yB * waktuEfektif);
  int hijauC = round(yC * waktuEfektif);

  int totalHijau = hijauA + hijauB + hijauC;
  int sisa = waktuEfektif - totalHijau;

  if (sisa > 0)
  {
    if (yA >= yB && yA >= yC)
      hijauA += sisa;
    else if (yB >= yA && yB >= yC)
      hijauB += sisa;
    else
      hijauC += sisa;
  }
  else if (sisa < 0)
  {
    if (yA >= yB && yA >= yC)
      hijauA += sisa;
    else if (yB >= yA && yB >= yC)
      hijauB += sisa;
    else
      hijauC += sisa;
  }

  return {hijauA, hijauB, hijauC};
}

// ===== Callback ESP-NOW =====
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  if (len == sizeof(int))
  {
    int kendaraan = 0;
    memcpy(&kendaraan, data, sizeof(int));

    if (memcmp(info->src_addr, macA, 6) == 0)
    {
      kendaraanA = kendaraan;
    }
    else if (memcmp(info->src_addr, macB, 6) == 0)
    {
      kendaraanB = kendaraan;
    }
    else if (memcmp(info->src_addr, macC, 6) == 0)
    {
      kendaraanC = kendaraan;
    }

    Serial.printf("A: %d | B: %d | C: %d\n", kendaraanA, kendaraanB, kendaraanC);

    if (kendaraanA > 0 && kendaraanB > 0 && kendaraanC > 0)
    {
      HasilWaktuHijau hasil = hitungWaktuHijau(kendaraanA, kendaraanB, kendaraanC);

      HijauPacket packet = {hasil.hijauA, hasil.hijauB, hasil.hijauC};

      esp_now_send(macA, (uint8_t *)&packet, sizeof(packet));
      esp_now_send(macB, (uint8_t *)&packet, sizeof(packet));
      esp_now_send(macC, (uint8_t *)&packet, sizeof(packet));

      Serial.printf("HijauA: %d | HijauB: %d | HijauC: %d -> DIKIRIM KE SEMUA\n", packet.hijauA, packet.hijauB, packet.hijauC);

      Serial2.printf("%d,%d,%d,%d,%d,%d\n", packet.hijauA, packet.hijauB, packet.hijauC, kendaraanA, kendaraanB, kendaraanC);

      kendaraanA = kendaraanB = kendaraanC = 0;
    }
  }
  else
  {
    Serial.println("Data tidak valid!");
  }
}

// ===== Setup =====
void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  Serial2.begin(9600, SERIAL_8N1, 19, 18);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW INIT GAGAL");
    while (true)
      ;
  }

  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peerA = {};
  peerA.channel = 0;
  peerA.encrypt = false;
  memcpy(peerA.peer_addr, macA, 6);
  esp_now_add_peer(&peerA);

  esp_now_peer_info_t peerB = {};
  peerB.channel = 0;
  peerB.encrypt = false;
  memcpy(peerB.peer_addr, macB, 6);
  esp_now_add_peer(&peerB);

  esp_now_peer_info_t peerC = {};
  peerC.channel = 0;
  peerC.encrypt = false;
  memcpy(peerC.peer_addr, macC, 6);
  esp_now_add_peer(&peerC);

  Serial.println("ESP-NOW + UART2 Siap");
}

// ===== Loop =====
void loop()
{
  delay(10);
}
