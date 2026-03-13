#ifndef SENSORS_H
#define SENSORS_H

#include "Arduino.h"
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// TinyGPSPlus not needed on testbed

// ── Pin Definitions ───────────────────────────────────────
#define ONE_WIRE_PIN    GPIO_NUM_4   // was GPIO 3 on PCB
// GPS_SERIAL / GPS_BAUD not used on testbed

// ── Data Structures ───────────────────────────────────────
struct HeartRateData {
    int    bpm;
    float  spo2;
    bool   valid;
};

struct IMUData {
    float accelX, accelY, accelZ;
    float gyroX,  gyroY,  gyroZ;
    bool  valid;
};

// GPSData removed — GPS not on testbed

// ── Public API ────────────────────────────────────────────
bool sensors_initHR();
bool sensors_initIMU();
bool sensors_initTemp();
// sensors_initGPS / sensors_readGPS removed

HeartRateData sensors_readHR();
IMUData       sensors_readIMU();
float         sensors_readTemp();

#endif
