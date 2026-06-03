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
constexpr uint32_t pin_ir = 9;

// communication variables
IPAddress app_ip(0, 0, 0, 0);
bool app_connected = false;

// wifi variables
WiFiUDP udp;

// sensor variables
unsigned long elapsed_time = 0;
unsigned long old_elapsed_time = 0;
volatile int ir_pulse_count = 0;

// interrupt functions
void ir_pulse_detected() {
  ir_pulse_count++;
}

// setup
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
  pinMode(pin_ir, INPUT);

  // setup interrupts
  attachInterrupt(digitalPinToInterrupt(pin_ir), ir_pulse_detected, RISING);
}

void loop() {
  elapsed_time = millis() - old_elapsed_time;

  // get ir sensor data every 200ms
  int ir_classification = 0;

  if (elapsed_time > 200) {
    noInterrupts();
    int ir_pulse_count_total = ir_pulse_count;
    ir_pulse_count = 0;
    interrupts();

    float ir_pulse_rate = (float) ir_pulse_count_total * 1000.0f / (float) elapsed_time;

    if (ir_pulse_rate > 430) {
      ir_classification = 547;
    } else if (ir_pulse_rate > 10) {
      ir_classification = 312;
    } else {
      ir_classification = 0;
    }

    old_elapsed_time = millis();
  }

  // get hall sensor data
  int mag = analogRead(pin_mag);

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

    char buf[512];
    snprintf(buf, sizeof(buf), "%d,%d", mag, ir_classification);

    udp.print(buf);
    udp.endPacket();
  }
  int val = analogRead(pin_mag);
  //serial_log("%d", val);

  delay(1);
}