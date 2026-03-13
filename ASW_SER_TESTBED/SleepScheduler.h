#ifndef SLEEPSCHEDULER_H
#define SLEEPSCHEDULER_H

#include "Arduino.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "DeviceIdentity.h"

// ── Debug Output ──────────────────────────────────────────
// Comment this line out before flashing the final PCB build
#define DEBUG

#ifdef DEBUG
  #define DEBUG_BEGIN()     Serial.begin(115200)
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_BEGIN()
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ── Sleep Interval ────────────────────────────────────────
#define TEST_INTERVAL     10   // seconds — shortened for testbed

// ── LDO Enable Pins ──────────────────────────────────────
#define LDO_3V3_EN    GPIO_NUM_25   // was GPIO 17 on PCB, GPIO 15 on earlier testbed
// LDO_1V8_EN not populated on test board — stubbed out in .cpp

// ── Sensor Identifiers ───────────────────────────────────
#define SENSOR_HR         0
#define SENSOR_TEMP       1
#define SENSOR_IMU        2
// SENSOR_GPS not used on testbed

// ── Buffer ───────────────────────────────────────────────
#define MAX_READINGS  60

// ── Data Structures ──────────────────────────────────────
struct SensorReading {
    uint32_t timestamp;

    // MAX30102
    int      heartRate;
    float    spo2;
    bool     hrValid;

    // DS18B20
    float    temperature;
    bool     tempValid;

    // MPU6050
    float    accelX, accelY, accelZ;
    float    gyroX,  gyroY,  gyroZ;
};

struct SensorBuffer {
    DeviceIdentity device;
    SensorReading readings[MAX_READINGS];
    int           index;
    int           count;
    bool          transmitReady;
    uint32_t      deviceUptime;
};

// ── Public API ───────────────────────────────────────────
void scheduler_init();
void scheduler_run(int sensor, int sleepSeconds);
void scheduler_transmit();
bool scheduler_isTransmitReady();
SensorBuffer* scheduler_getBuffer();
void scheduler_sleep(int seconds);

#endif
