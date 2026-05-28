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

constexpr uint32_t pin_mag = 1;

// communication variables
IPAddress app_ip(0, 0, 0, 0);
bool app_connected = false;

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
  serial_log("Metro IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  serial_log("UDP port: %d", rover_port);

  // pin setup
  analogWriteResolution(10);
  analogReadResolution(12);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(pin_len, OUTPUT);
  pinMode(pin_ldir, OUTPUT);
  pinMode(pin_ren, OUTPUT);
  pinMode(pin_rdir, OUTPUT);
  
  pinMode(pin_mag, INPUT);
}

void loop() {
  // receive packets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[512];
    memset(buf, 0, sizeof(buf));
    udp.read(buf, sizeof(buf));

    char* end1;
    char* end2;

    float throttle = strtof(buf, &end1);
    float steering = strtof(end1 + 1, &end2);
    int info = atoi(end2 + 1); 

    float left = constrain(throttle + steering, -1.0f, 1.0f); // differential steering
    float right = constrain(throttle - steering, -1.0f, 1.0f);

    digitalWrite(pin_ldir, (left > 0.0f) ? LOW : HIGH);
    analogWrite(pin_len, abs(left * 1023.0f));

    digitalWrite(pin_rdir, (right > 0.0f) ? HIGH : LOW);
    analogWrite(pin_ren, abs(right * 1023.0f));

    if (info == 1) {
      serial_log("App connected.");
      app_connected = true;

      app_ip = udp.remoteIP();
      serial_log("User IP address: %d.%d.%d.%d", app_ip[0], app_ip[1], app_ip[2], app_ip[3]);
    } else if (info == 2) {
      serial_log("App disconnected.");
      app_connected = false;
    }

  }

  // send packets
  if (app_connected) {
    udp.beginPacket(app_ip, app_port);
    udp.print("test");
    udp.endPacket();
  }
  int val = analogRead(pin_mag);
  //serial_log("%d", val);

  delay(1);
}