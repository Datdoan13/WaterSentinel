
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SH110X.h>
#include "image.h"
#include <WiFi.h>
#include "config.h"
#include "ThingSpeak.h"  // always include thingspeak header file after other header files and custom macros
#include <BlynkSimpleEsp32.h>
#define ss 5
#define rst 14
#define dio0 32
float temperature, tdsValue, pHValue;  // Khai báo các biến lưu trữ dữ liệu nhận được
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define i2c_Address 0x3c
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const int resetButtonPin = 2;
const int buttonPin1 = 12;
const int buttonPin2 = 13;
const int buttonPin3 = 15;
const int buttonPin4 = 16;

const int numImages = 3;
const unsigned long imageDuration = 2000;
unsigned long previousImageMillis = 0;
int currentImageIndex = 0;
int pagedata = 0;

WiFiClient client;  //connect wifi

String Incoming = "";
String Message = "";

byte LocalAddress = 0x01;
byte Destination_ESP32_Slave_1 = 0x02;

unsigned long previousMillis_SendMSG = 0;
const long interval_SendMSG = 1000;

unsigned long previousMillis_SendThinkSpeak = 0;
const long interval_SendThinkSpeak = 10000;

byte Slv = 0;
volatile bool button4Pressed = false;
void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();
  LoRa.write(Destination);
  LoRa.write(LocalAddress);
  LoRa.write(Outgoing.length());
  LoRa.print(Outgoing);
  LoRa.endPacket();
}
void resetESP32() {
  ESP.restart();  // Hàm khởi động lại ESP32
}
void button4ISR() {
  button4Pressed = true;
}
/////////////////////////////
void displaySensorData() {
  if (Incoming.startsWith("SL1,")) {
    // Parse received sensor data
    int comma1 = Incoming.indexOf(',');
    int comma2 = Incoming.indexOf(',', comma1 + 1);

    // String pHStr = Incoming.substring(comma1 + 1, comma2);
    String tempStr = Incoming.substring(comma2 + 1, Incoming.lastIndexOf(','));
    String tdsStr = Incoming.substring(Incoming.lastIndexOf(',') + 1);

    // pHValue = pHStr.toFloat();
    temperature = tempStr.toFloat();
    tdsValue = tdsStr.toFloat();
    send_data_to_server(temperature,tdsValue);
  }
}
void Nhietdo() {
  display.clearDisplay();
  // Set text size to 1 for the "Temperature:" label
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Temperature:");
  // Set a larger text size (e.g., 3) for the temperature value
  display.setTextSize(3);
  // Move the cursor down to align with the larger text
  display.setCursor(0, 25);
  // Display only the temperature value in a larger text size +
  display.println(String(temperature) + " C");
  display.display();
}
void TDS() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("TDS Value: ");
  display.setTextSize(2);
  // Move the cursor down to align with the larger text
  display.setCursor(0, 25);
  // Display only the temperature value in a larger text size + String(tdsValue)
  display.println(String(tdsValue) + " PPM");
  display.display();
}
void ph() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("pH Value: " + String(pHValue));
  display.display();
}

////////////////////////////

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
    Serial.println("error: message length does not match length");
    return;
  }

  if (recipient != LocalAddress) {
    Serial.println("This message is not for me.");
    return;
  }

  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Message: " + Incoming);
}
///////////////////////////////
void changeImage(int &newIndex) {
  display.clearDisplay();

  switch (newIndex) {
    case 0:
      display.drawBitmap(0, 0, image_data_nhietdo, 128, 64, 1);
      break;
    case 1:
      display.drawBitmap(0, 0, image_data_TDS, 128, 64, 1);
      break;
    case 2:
      display.drawBitmap(0, 0, image_data_PH, 128, 64, 1);
      break;
    default:
      break;
  }

  display.display();
}

void handleButtonPress() {
  int buttonState1 = digitalRead(buttonPin1);
  int buttonState2 = digitalRead(buttonPin2);

  static int lastButtonState1 = HIGH;
  static int lastButtonState2 = HIGH;

  if (buttonState1 != lastButtonState1 && buttonState1 == LOW && pagedata != 1) {
    if (currentImageIndex > 0) {
      currentImageIndex--;
      if (currentImageIndex < 0) {
        currentImageIndex = 0;
      }
    }
    Serial.println(currentImageIndex);
    changeImage(currentImageIndex);
  }

  if (buttonState2 != lastButtonState2 && buttonState2 == LOW && pagedata != 1) {
    if (currentImageIndex <= numImages - 1) {
      currentImageIndex++;
      if (currentImageIndex > 2) {
        currentImageIndex = 2;
      }
    }
    Serial.println(currentImageIndex);
    changeImage(currentImageIndex);
  }

  lastButtonState1 = buttonState1;
  lastButtonState2 = buttonState2;
}

void handleSelectAndExitButtons() {
  int buttonState3 = digitalRead(buttonPin3);
  static int lastButtonState3 = HIGH;
  if ((buttonState3 != lastButtonState3 && buttonState3 == LOW) || pagedata == 1) {
    pagedata = 1;
    if (currentImageIndex == 0) {
      Nhietdo();
    } else if (currentImageIndex == 1) {
      TDS();
    } else if (currentImageIndex == 2) {
      ph();
    }
  }
  // int buttonState4 = digitalRead(buttonPin4);
  // static int lastButtonState4 = HIGH;
  // if (buttonState3 != lastButtonState3 && buttonState3 == LOW) {
  //   changeImage(currentImageIndex);
  //   pagedata == 0;
  // }
  lastButtonState3 = buttonState3;
  // lastButtonState4 = buttonState4;
}
void setup_wifi() {
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);  // Initialize ThingSpeak
  Blynk.begin(BLYNK_AUTH_TOKEN, SSID, PASS);//Init Blynk
}

void setup() {
  Serial.begin(115200);

  LoRa.setPins(ss, rst, dio0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;
  }
  Serial.println("LoRa init succeeded");

  pinMode(resetButtonPin, INPUT_PULLUP);
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
  pinMode(buttonPin4, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin4), button4ISR, FALLING);
  if (!display.begin(i2c_Address, true)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(2000);
  display.clearDisplay();
  display.drawBitmap(0, 0, image_data_nhietdo, 128, 64, 1);
  display.display();
  previousImageMillis = millis();
  setup_wifi();
}

void sendToSlave() {
  unsigned long currentMillis_SendMSG = millis();

  if (currentMillis_SendMSG - previousMillis_SendMSG >= interval_SendMSG) {
    previousMillis_SendMSG = currentMillis_SendMSG;

    Slv++;
    if (Slv > 2) Slv = 1;

    Message = "SDS" + String(Slv);

    if (Slv == 1) {
      Serial.println();
      Serial.print("Send message to ESP32 Slave " + String(Slv));
      Serial.println(" : " + Message);
      sendMessage(Message, Destination_ESP32_Slave_1);
    }
  }
}
void connect_wifi() {
  int statusCode = 0;
  // Connect or reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SSID);
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(SSID, PASS);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected");
  }
}
void send_data_to_server(float nhietdo, float doam) {
  unsigned long currentMillis_SendThinkSpeak = millis();

  if (currentMillis_SendThinkSpeak - previousMillis_SendThinkSpeak >= interval_SendThinkSpeak) {
    previousMillis_SendThinkSpeak = currentMillis_SendThinkSpeak;
    
    //For ThingSpeak
    // set the fields with the values
    ThingSpeak.setField(NHIETDO_FIELD, nhietdo);
    ThingSpeak.setField(DOAM_FIELD, doam);
    ThingSpeak.setStatus("Ok");
    int x = ThingSpeak.writeFields(CH_ID_DEMO, CH_API_WRITE_KEY);
    if (x == 200) {
      Serial.println("Channel update successful.");
    } else {
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }

    //For Blink
    Blynk.run();
    Blynk.virtualWrite(V0, nhietdo);
    Blynk.virtualWrite(V1, doam);
  }
}

void loop() {
  sendToSlave();
  onReceive(LoRa.parsePacket());
  displaySensorData();
  handleButtonPress();
  handleSelectAndExitButtons();  // Add this function call to handle select and exit buttons
  if (digitalRead(resetButtonPin) == LOW) {
    resetESP32();
  }
  if (button4Pressed) {
    pagedata = 0;
    changeImage(currentImageIndex);  // Gọi hàm thay đổi hình ảnh
    button4Pressed = false;          // Đặt biến cờ về lại false để chuẩn bị cho lần nhấn tiếp theo
  }
  connect_wifi();
}
