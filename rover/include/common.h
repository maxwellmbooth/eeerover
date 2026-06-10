#pragma once
#include <Arduino.h>

constexpr char rover_ssid[] = "mrover26";
constexpr char rover_pass[] = "eeerover";

constexpr uint16_t rover_port = 4810;
constexpr uint16_t app_port = 4811;

const IPAddress rover_ip(192, 168, 1, 1);

inline void serial_log(const char* msg, ...) {
    char buf[128];

    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    Serial.print("[" ENV_NAME "] ");
    Serial.println(buf);
}