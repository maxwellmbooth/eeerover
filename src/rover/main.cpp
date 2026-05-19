#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#define ENV_NAME "rover"

#include "shared/common.h"

// wifi variables
WiFiUDP udp;

void setup() {
  delay(3000);
  Serial.begin(115200);
  serial_log("Booted.");
  
  // wifi shield setup
  WiFi.noLowPowerMode();
  //WiFi.config(rover_ip);
  WiFi.begin(controller_ssid);
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    attempts++;
    serial_log("Connection failed, attempt #%d (status: %d)", attempts, WiFi.status());

    if (attempts >= 20) {
      serial_log("Connection to controller timeout.");
      while(true);
    }
  }
  serial_log("Connected to controller.");
  
  // begin wifi services
  udp.begin(rover_udp_port);

  // pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[16];
    udp.read(buf, sizeof(buf));
    Serial.println(buf);
  }
  delay(1);
}