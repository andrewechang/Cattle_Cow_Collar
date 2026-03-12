#ifndef SENSORS_H
#define SENSORS_H

#include "Arduino.h"

//alot of these require libraries to be installed from arduino IDE
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
//#include <MPU6050.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TinyGPSPlus.h>

//Pin def, ignore or rewrite if you use other pins
#define ONE_WIRE_PIN    GPIO_NUM_3
#define GPS_SERIAL      Serial0
#define GPS_BAUD        9600

//Data structures
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

struct GPSData {
    float latitude;
    float longitude;
    float altitude;
    bool  valid;
};

//Public API, for general use, everything else before this are like behind the scenes functions
bool sensors_initHR();
bool sensors_initIMU();
bool sensors_initTemp();
bool sensors_initGPS();

HeartRateData sensors_readHR();
IMUData       sensors_readIMU();
float         sensors_readTemp();
GPSData       sensors_readGPS(int timeoutSeconds);

#endif