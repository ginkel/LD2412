/*
  This program sets the serial baud rate of the HLK-LD2410
  presence sensor.

  Use these two lines to the correct values:
  [line 43] #define CURRENT_BAUD_RATE LD2410_BAUD_RATE
  [line 44] #define NEW_BAUD_RATE 115200

  #define SERIAL_BAUD_RATE sets the serial monitor baud rate

  Communication with the sensor is handled by the
  "MyLD2410" library Copyright (c) Iavor Veltchev 2024

  Use only hardware UART at the default baud rate 256000,
  or change the #define LD2410_BAUD_RATE to match your sensor.
  For ESP32 or other boards that allow dynamic UART pins,
  modify the RX_PIN and TX_PIN defines

  Connection diagram:
  Arduino/ESP32 RX  -- TX LD2410
  Arduino/ESP32 TX  -- RX LD2410
  Arduino/ESP32 GND -- GND LD2410
  Provide sufficient power to the sensor Vcc (200mA, 5-12V)
*/
#if defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_LEONARDO)
//ARDUINO_SAMD_NANO_33_IOT RX_PIN is D1, TX_PIN is D0 
//ARDUINO_AVR_LEONARDO RX_PIN(RXI) is D0, TX_PIN(TXO) is D1 
#define sensorSerial Serial1
#elif defined(ARDUINO_XIAO_ESP32C3) || defined(ARDUINO_XIAO_ESP32C6)
//RX_PIN is D7, TX_PIN is D6
#define sensorSerial Serial0
#elif defined(ESP32)
//Other ESP32 device - choose available GPIO pins
#define sensorSerial Serial1
#if defined(ARDUINO_ESP32S3_DEV)
#define RX_PIN 18
#define TX_PIN 17
#else
#define RX_PIN 16
#define TX_PIN 17
#endif
#else
#error "This sketch only works on ESP32, Arduino Nano 33IoT, and Arduino Leonardo (Pro-Micro)"
#endif

// User defines
// #define DEBUG_MODE
#define CURRENT_BAUD_RATE LD2410_BAUD_RATE
#define NEW_BAUD_RATE 115200
#define SERIAL_BAUD_RATE 115200

// Change the communication baud rate here, if necessary
// #define LD2410_BAUD_RATE 256000
#include "MyLD2410.h"

#ifdef DEBUG_MODE
MyLD2410 sensor(sensorSerial, true);
#else
MyLD2410 sensor(sensorSerial);
#endif

uint32_t BAUDS[9]{ 0, 9600, 19200, 38400, 57600, 115200, 230400, 256000, 460800 };

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
#if defined(ARDUINO_XIAO_ESP32C3) || defined(ARDUINO_XIAO_ESP32C6) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_LEONARDO)
  sensorSerial.begin(CURRENT_BAUD_RATE);
#else
  sensorSerial.begin(CURRENT_BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
#endif
  delay(2000);
  Serial.println(__FILE__);

  byte newBaudIndex = 0;
  for (byte i = 1; i < 9; i++)
    if (NEW_BAUD_RATE == BAUDS[i]) {
      newBaudIndex = i;
      break;
    }

  if (!newBaudIndex) {
    Serial.print("Invalid NEW_BAUD_RATE value: ");
    Serial.println(NEW_BAUD_RATE);
    while (true) {}
  }

  if (!sensor.begin()) {
    Serial.print("Failed to communicate with the sensor at ");
    Serial.print(CURRENT_BAUD_RATE);
    Serial.println(" baud...");
    while (true) {}
  }
  Serial.println("Established communication with the sensor at ");
  Serial.print(CURRENT_BAUD_RATE);
  Serial.println(" baud!");

  Serial.print("Changing the baud to ");
  Serial.println(NEW_BAUD_RATE);

  if (!sensor.setBaud(newBaudIndex)) {
    Serial.print(" ***Failed! The baud rate is still ");
    Serial.println(CURRENT_BAUD_RATE);
    while (true) {}
  }
  Serial.print(" +Sucess!\nYour sensor is rebooting with the new baud rate ");
  Serial.println(NEW_BAUD_RATE);

  sensorSerial.end();
  delay(1000);

#if defined(ARDUINO_XIAO_ESP32C3) || defined(ARDUINO_XIAO_ESP32C6) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_LEONARDO)
  sensorSerial.begin(NEW_BAUD_RATE);
#else
  sensorSerial.begin(NEW_BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
#endif
  delay(1000);
  if (!sensor.begin()) {
    Serial.print("Failed to communicate with the sensor at ");
    Serial.print(NEW_BAUD_RATE);
    Serial.println(" baud...");
  } else {
    Serial.println("Established communication with the sensor at ");
    Serial.print(NEW_BAUD_RATE);
    Serial.println(" baud!");
  }
}

void loop() {
}