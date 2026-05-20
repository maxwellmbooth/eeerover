#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#define ENV_NAME "rover"

#include "common.h"

// pin mappings
constexpr uint32_t pin_ldir = 9;
constexpr uint32_t pin_len = 8;
constexpr uint32_t pin_rdir = 12;
constexpr uint32_t pin_ren = 11;

// test variables
bool toggle_led = LOW;

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
  analogWriteResolution(10);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(pin_len, OUTPUT);
  pinMode(pin_ldir, OUTPUT);
  pinMode(pin_ren, OUTPUT);
  pinMode(pin_rdir, OUTPUT);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[32];
    memset(buf, 0, sizeof(buf));
    udp.read(buf, sizeof(buf));

    char *end;
    float throttle = strtof(buf, &end);
    float steering = strtof(end + 1, nullptr);

    float left = constrain(throttle + steering, -1.0f, 1.0f); // differential steering
    float right = constrain(throttle - steering, -1.0f, 1.0f);

    digitalWrite(pin_ldir, (left > 0.0f) ? LOW : HIGH);
    analogWrite(pin_len, abs(left * 1023.0f));

    digitalWrite(pin_rdir, (right > 0.0f) ? HIGH : LOW);
    analogWrite(pin_ren, abs(right * 1023.0f));
  }
  delay(1);
}