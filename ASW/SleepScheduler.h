#ifndef SLEEPSCHEDULER_H
#define SLEEPSCHEDULER_H

#include "Arduino.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "DeviceIdentity.h"

//LDO Enable Pins
#define LDO_3V3_EN    GPIO_NUM_17   // controls 3V3 rail 
#define LDO_1V8_EN    GPIO_NUM_18   // controls 1V8 rail
//In the instance that we do not have the PCb yet, these will controll the gates to mosfets to cut power to the sensors

//Sensor Identifiers
#define SENSOR_HR         0   
#define SENSOR_TEMP       1   
#define SENSOR_IMU        2    
#define SENSOR_GPS        3    

//GPS Pins
#define GPS_RST_PIN   GPIO_NUM_35
#define GPS_INT_PIN   GPIO_NUM_39
//sam m8q interrupt pins, ignore if using other

//Buffer sizes
#define MAX_READINGS  60 // the system will read a max of 60 data points until it will automaticlly transmit

// Data structure stored in RTC memory
struct SensorReading {
    // timestamp
    uint32_t timestamp;       // seconds since device start (from RTC)

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

    // SAM-M8Q
    float    latitude;
    float    longitude;
    float    altitude;
    bool     gpsValid;
};

struct SensorBuffer {
    DeviceIdentity device;
    SensorReading readings[MAX_READINGS];
    int           index;
    int           count;
    bool          transmitReady;
    uint32_t      deviceUptime;   // tracked across sleep cycles
};

//Public API, use these freely, everything else before this is behind the scenes functions
void scheduler_init();
void scheduler_run(int sensor, int sleepSeconds);
void scheduler_transmit();
bool scheduler_isTransmitReady();
SensorBuffer* scheduler_getBuffer();
void scheduler_sleep(int seconds);

#endif