#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#define ENV_NAME "rover"

#include "shared/common.h"
#include "rover/webserver.h"
#include "rover/ui/html.h"

// wifi variables
WiFiServer server(rover_web_port);
WiFiUDP udp;

void setup() {
  delay(3000);
  Serial.begin(115200);
  serial_log("Booted.");
  
  // wifi shield setup
  //WiFi.config(rover_ip);
  uint8_t ap_status = WiFi.beginAP(rover_ssid);
  if (ap_status != WL_AP_LISTENING) {
    serial_log("Failed to start access point, error code: %d", ap_status);
    while (true);
  }
  // begin wifi services
  server.begin();
  udp.begin(rover_udp_port);

  IPAddress ip = WiFi.localIP();
  serial_log("IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  serial_log("Webpage port: %d", rover_web_port);

  // pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
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

  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[16];
    udp.read(buf, sizeof(buf));
    Serial.println(buf);
  }
}