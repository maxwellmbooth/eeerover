#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#define ENV_NAME "controller"

#include "shared/common.h"

// pin definitions
constexpr int test_button = 5;

// wifi variables
WiFiUDP udp;

void sendCommand(const char* cmd) {
  udp.beginPacket(rover_ip, rover_udp_port);
  udp.write(cmd);
  udp.endPacket();
}

void setup() {
  delay(3000);
  Serial.begin(115200);
  serial_log("Booted.");
  
  // wifi shield setup
  WiFi.begin(rover_ssid);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    attempts++;
    serial_log("Connection to rover attempt #%d (status: %d)", attempts, WiFi.status());

    if (attempts > 20) {
      serial_log("Connection to rover timeout.");
      while(true);
    }
  }
  serial_log("Connected to rover.");

  // begin wifi services
  udp.begin(controller_udp_port);

  // pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(test_button, INPUT);
}

void loop() {
  serial_log("%d", digitalRead(test_button));
  if (digitalRead(test_button) == HIGH) {
    sendCommand("0");
  }
  delay(500);
}