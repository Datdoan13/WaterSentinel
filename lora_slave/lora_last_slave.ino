#include <SPI.h>
#include <LoRa.h>
#include "OneWire.h"
#include "DallasTemperature.h"

#define DS18B20PIN 15
#define TdsSensorPin 27  // Thay thế LED_1_Pin bằng pin TDS sensor
#define ss 5
#define rst 14
#define dio0 2
// Biến và hằng số cho lọc trung bình
#define FILTER_SIZE 5                // Kích thước cửa sổ lọc trung bình
int temperatureBuffer[FILTER_SIZE];  // Mảng lưu trữ giá trị nhiệt độ
int tdsBuffer[FILTER_SIZE];          // Mảng lưu trữ giá trị TDS
int tempBufferIndex = 0;             // Chỉ số hiện tại trong mảng lưu trữ giá trị nhiệt độ
int tdsBufferIndex = 0;              // Chỉ số hiện tại trong mảng lưu trữ giá trị TDS
OneWire oneWire(DS18B20PIN);
DallasTemperature DS18B20(&oneWire);

String Incoming = "";
String Message = "";

byte LocalAddress = 0x02;
byte Destination_Master = 0x01;

int h = 0;
float t = 0.0;

unsigned long previousMillis_UpdateDHT11 = 0;
const long interval_UpdateDHT11 = 2000;

// Khai báo các biến liên quan đến đo TDS
#define VREF 3.3
#define SCOUNT 30

int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;

void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();
  LoRa.write(Destination);
  LoRa.write(LocalAddress);
  LoRa.write(Outgoing.length());
  LoRa.print(Outgoing);
  LoRa.endPacket();
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;

  int recipient = LoRa.read();
  byte sender = LoRa.read();
  byte incomingLength = LoRa.read();

  Incoming = "";

  while (LoRa.available()) {
    Incoming += (char)LoRa.read();
  }

  if (incomingLength != Incoming.length()) {
    Serial.println();
    Serial.println("error: message length does not match length");
    return;
  }

  if (recipient != LocalAddress) {
    Serial.println();
    Serial.println("This message is not for me.");
    return;
  }

  Serial.println();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Message: " + Incoming);

  Processing_incoming_data();
}

void Processing_incoming_data() {
  if (Incoming == "SDS1") {
    // Đo TDS và cập nhật giá trị tdsValue
    readTDS();

    Message = "SL1," + String(h) + "," + String(t) + "," + String(tdsValue);
    Serial.println();
    Serial.println("Send message to Master");
    Serial.print("Message: ");
    Serial.println(Message);

    sendMessage(Message, Destination_Master);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TdsSensorPin, INPUT);  // Cấu hình pin TDS sensor
  delay(100);
  DS18B20.begin();
  LoRa.setPins(ss, rst, dio0);
  Serial.println();
  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;
  }
  Serial.println("LoRa init succeeded.");
}

void loop() {
  unsigned long currentMillis = millis();

  // Đo giá trị TDS mỗi 2 giây
  if (currentMillis - previousMillis_UpdateDHT11 >= interval_UpdateDHT11) {
    previousMillis_UpdateDHT11 = currentMillis;
    DS18B20.requestTemperatures();
    t = DS18B20.getTempCByIndex(0);

    // Đo TDS và cập nhật giá trị tdsValue
    readTDS();

    if (isnan(h) || isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      h = 0;
      t = 0.0;
    }
  }

  onReceive(LoRa.parsePacket());
}

// Hàm đo TDS
void readTDS() {
  static unsigned long analogSampleTimepoint = millis();

  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) {
      analogBufferIndex = 0;
    }
  }

  temperature = DS18B20.getTempCByIndex(0);

  static unsigned long printTimepoint = millis();
  updateBuffer(tdsBuffer, tdsValue, tdsBufferIndex);
  tdsValue = getMedianNum(tdsBuffer, FILTER_SIZE);
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();

    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

      averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;

      float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
      float compensationVoltage = averageVoltage / compensationCoefficient;

      tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
    }
  }
}


// Hàm lấy giá trị trung bình từ mảng
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}
////////////////////////
void updateBuffer(int *buffer, int value, int &index) {
  buffer[index] = value;
  index = (index + 1) % FILTER_SIZE;
}