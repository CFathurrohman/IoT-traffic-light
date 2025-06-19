#include <Arduino.h>

// Gunakan UART1: RX = 19 (menerima dari pusat TX), TX = 18 (jika mau balas)
HardwareSerial SerialUART(1);

// Buffer untuk simpan JSON mentah dari pusat
String receivedJson = "";
String uartBuffer = "";

void setup() {
  Serial.begin(115200);  // Debug USB, boleh dihapus kalau tidak perlu
  SerialUART.begin(9600, SERIAL_8N1, 19, 18); // RX=19, TX=18

  Serial.println("ESP32 Jembatan UART Siap...");
}

void loop() {
  // Baca per karakter
  while (SerialUART.available()) {
    char c = SerialUART.read();
    if (c == '\n') {
      receivedJson = uartBuffer;
      uartBuffer = "";

      // Debug: Tampilkan kalau mau cek
      Serial.println("JSON diterima & disimpan:");
      Serial.println(receivedJson);
    } else {
      uartBuffer += c;
    }
  }
}
