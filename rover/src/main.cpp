#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#define ENV_NAME "rover"

#include "common.h"

// pin definitions
constexpr int test_button = 6;

// wifi variables
WiFiUDP udp;

void sendCommand(const char* cmd) {
  udp.beginPacket(rover_ip, rover_port);
  udp.write(cmd);
  udp.endPacket();
}

void setup() {
  delay(3000);
  Serial.begin(115200);
  serial_log("Booted.");
  
  // wifi shield setup
  WiFi.noLowPowerMode();
  WiFi.config(rover_ip);
  uint8_t ap_status = WiFi.beginAP(rover_ssid, rover_pass, 6);
  if (ap_status != WL_AP_LISTENING) {
    serial_log("Failed to start access point, error code: %d", ap_status);
    while (true);
  }
  serial_log("Access point started.");

  // begin wifi services
  udp.begin(rover_port);

  IPAddress ip = WiFi.localIP();
  serial_log("IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  serial_log("UDP port: %d", rover_port);

  // pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(test_button, INPUT);
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