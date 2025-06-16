#include <HardwareSerial.h>

const int DATA_LENGTH = 6;
int dataValues[DATA_LENGTH];

// ===== Inisialisasi UART2 =====
void initUART(int baudrate, int rxPin, int txPin) {
  Serial2.begin(baudrate, SERIAL_8N1, rxPin, txPin);
}

// ===== Fungsi parsing nilai CSV ke array dataValues =====
bool terimaDataCSV() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    int startIdx = 0;
    int found = 0;

    for (int i = 0; i <= data.length(); i++) {
      if (data.charAt(i) == ',' || i == data.length()) {
        String valStr = data.substring(startIdx, i);
        if (found < DATA_LENGTH) {
          dataValues[found] = valStr.toInt();
          found++;
          startIdx = i + 1;
        } else {
          break;
        }
      }
    }
    return (found == DATA_LENGTH);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  initUART(9600, 18, 19);
}

void loop() {
  if (terimaDataCSV()) {
  }
}
