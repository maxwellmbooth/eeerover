#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h>

void send_response(WiFiClient& client, const char* contentType, const char* body);