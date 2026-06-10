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

constexpr uint32_t pin_age = 0; // not used but just to reserve (pin 1 also used for tx)
constexpr uint32_t pin_ir = 2;
constexpr uint32_t pin_us = A0;
constexpr uint32_t pin_mag = A1;

// communication variables
IPAddress app_ip(0, 0, 0, 0);
bool connected_once = false;
bool app_connected = false;
unsigned long last_packet_time = 0;

// wifi variables
WiFiUDP udp;

// sensor variables
constexpr uint32_t age_baud_rate = 600;
constexpr uint32_t age_msg_length = 3;
char age[age_msg_length + 1] = {0}; // +1 for null termination

volatile int ir_pulse_count = 0;
const char* ir = "Not detected";

constexpr int us_threshold = 1861; // 1.5V for 12-bit adc
constexpr int us_samples = 10;
const char* us = "Not detected";

const char* mag = "Not detected";

// ir interrupt function
void ir_pulse_detected() {
    ir_pulse_count++;
}

// check for received packet and process control data
void receive_packet() {
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

        if (!app_connected) {
            serial_log("App connected.");
        }

        connected_once = true;
        app_connected = true; // any received packet means app is connected
        app_ip = udp.remoteIP();

        if (info == 1) {
            serial_log("App disconnected."); //app tells shutdown
            app_connected = false;
        }

        last_packet_time = millis();
    } else if (app_connected && millis() - last_packet_time > 500) {
        serial_log("WARNING: Packet timeout. (0x10)");
        serial_log("App disconnected.");
        app_connected = false;
        analogWrite(pin_len, 0);
        analogWrite(pin_ren, 0); // stop motors if timeout
    }
}

// process age via uart
void process_age() {
    static char frame[age_msg_length];
    static uint8_t frame_pos = 0;
    static bool synced = false;

    // process all uart bytes
    while (Serial1.available()) {
        char c = Serial1.read();

        if (!synced) {
            if (c == '#') {
                synced = true;
                frame_pos = 0;
            }
        } else {
            frame[frame_pos++] = c;

            if (frame_pos == 3) {
                memcpy(age, frame, 3);
                age[3] = '\0';

                synced = false;
                frame_pos = 0;
            }
        }
    }
}

// process ir count data every 200ms
void process_ir() {
    static unsigned long last_time = 0;

    unsigned long elapsed_time = millis() - last_time;
    if (elapsed_time > 200) {
        noInterrupts();
        int ir_pulse_count_total = ir_pulse_count;
        ir_pulse_count = 0;
        interrupts();

        float ir_pulse_rate = (float) ir_pulse_count_total * 1000.0f / (float) elapsed_time;

        if (ir_pulse_rate > 430) {
          ir = "547";
        } else if (ir_pulse_rate > 10) {
          ir = "312";
        } else {
          ir = "Not detected";
        }

        last_time = millis();
    }
}

// process us by average of 3 results
void process_us() {
    static unsigned long last_time = 0;
    static long sum = 0;
    static int sample_count = 0;

    if (millis() - last_time < 10) return;
    last_time = millis();

    sum += analogRead(pin_us);
    sample_count++;

    if (sample_count >= us_samples) {
        int avg = sum / sample_count;
        us = (avg > us_threshold) ? "Detected" : "Not detected";
        serial_log("avg: %d", analogRead(pin_us));

        sum = 0;
        sample_count = 0;
    }
}

// process magnetism adc reading
void process_mag() {
    int mag_raw = analogRead(pin_mag);
    if (mag_raw > 1267) {
        mag = "Up";
    } else if (mag_raw < 1247) {
        mag = "Down";
    } else {
        mag = "Not detected";
    }
}

// check if udp stops for extended time and reset if so
void check_and_recover() {
    if (connected_once) {
        unsigned long silence = millis() - last_packet_time;
        static unsigned long last_reinit = 0;
        if (silence > 3000 && millis() - last_reinit > 3000) {   // soft attempt first
            serial_log("Link silent 3s -- restarting UDP socket.");
            udp.stop();
            udp.begin(rover_port);
            last_reinit = millis();
        }
        if (silence > 20000) {                                    // still dead -> full reset
            serial_log("Link silent 20s -- resetting board.");
            delay(50);
            NVIC_SystemReset();                                  // SAMD21 software reset = reset button
        }
    }
}

// setup
void setup() {
    delay(500);
    Serial.begin(115200);
    serial_log("Booted.");

    // age uart setup
    Serial1.begin(age_baud_rate);
    
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
    // receive and process control packet
    receive_packet();

    // get sensor data
    process_age();
    process_ir();
    process_us();
    process_mag();

    // send packet with info every 50ms
    static unsigned long last_send = 0;
    if (app_connected && millis() - last_send >= 50) {
        udp.beginPacket(app_ip, app_port);

        char buf[512];
        snprintf(buf, sizeof(buf), "%s,%s,%s,%s", age, ir, us, mag);

        udp.write((uint8_t*) buf, strlen(buf));
        udp.endPacket();

        last_send = millis();
    }

    check_and_recover();
}