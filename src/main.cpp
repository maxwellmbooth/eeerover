#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>

#include "webserver.h"
#include "ui.h"

// wifi constants
char ssid[] = "morley";
uint16_t port = 80;
WiFiServer server(port);

void setup() {
  Serial.begin(115200);
  Serial.print("Booted.");
  
  // wifi shield setup
  uint8_t ap_status = WiFi.beginAP(ssid);
  if (ap_status != WL_AP_LISTENING) {
    Serial.print("Failed to start access point, error code: ");
    Serial.println(ap_status);
    while (true);
  }

  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.print("Metro IP address: ");
  Serial.println(ip);

  Serial.print("Metro webpage port: ");
  Serial.println(port);

  // pin setup
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  WiFiClient client = server.available();
  if (client) {

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