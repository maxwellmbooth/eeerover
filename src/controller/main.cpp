#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#define ENV_NAME "controller"

#include "shared/common.h"
#include "controller/webserver.h"
#include "controller/ui/html.h"

// pin definitions
constexpr int test_button = 6;

// wifi variables
WiFiServer server(controller_web_port);
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
  WiFi.noLowPowerMode();
  //WiFi.config(controller_ip);
  uint8_t ap_status = WiFi.beginAP(controller_ssid, 1);
  if (ap_status != WL_AP_LISTENING) {
    serial_log("Failed to start access point, error code: %d", ap_status);
    while (true);
  }
  serial_log("Access point started.");

  // begin wifi services
  server.begin();
  udp.begin(controller_udp_port);

  IPAddress ip = WiFi.localIP();
  serial_log("IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  serial_log("Webpage port: %d", controller_web_port);

  // pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(test_button, INPUT);
}

void loop() {
  if (digitalRead(test_button) == HIGH) {
    sendCommand("0");
  }
  
  WiFiClient client = server.available();
  if (client) {
    serial_log("Client connected");

    // wait for user to connect
    while (client.connected() && !client.available()) {
      delay(10);
    }

    // take http request in (do nothing with it so far)
    while (client.available()) {
      client.read();
    }

    // send webpage back
    send_response(client, "text/html", homepage);

    delay(1);
    client.stop();
  }
}