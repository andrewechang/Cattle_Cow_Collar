#include "Sensors.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
 
static MAX30105          hrSensor;
static Adafruit_MPU6050  imu;
static OneWire           oneWire(ONE_WIRE_PIN);
static DallasTemperature tempSensor(&oneWire);
 
// ── MAX30102 — Heart Rate & SpO2 ─────────────────────────
 
bool sensors_initHR() {
    if (!hrSensor.begin(Wire, I2C_SPEED_FAST)) {
        return false;
    }
    hrSensor.setup();
    hrSensor.setPulseAmplitudeRed(0x1F);
    hrSensor.setPulseAmplitudeIR(0x1F);
    return true;
}
 
HeartRateData sensors_readHR() {
    HeartRateData result = {0, 0.0, false};
 
    const int SAMPLES       = 100;
    const int FINGER_THRESH = 50000;
 
    byte rates[4] = {0};
    byte rateIndex = 0;
    long lastBeat  = 0;
    float beatsPerMinute = 0;
    int beatAvg = 0;
 
    unsigned long start = millis();
 
    for (int i = 0; i < SAMPLES; i++) {
        while (!hrSensor.available()) {
            hrSensor.check();
        }
 
        long irValue = hrSensor.getIR();
 
        if (irValue < FINGER_THRESH) {
            result.valid = false;
            hrSensor.nextSample();
            break;
        }
 
        if (checkForBeat(irValue)) {
            long delta = millis() - lastBeat;
            lastBeat = millis();
            beatsPerMinute = 60.0 / (delta / 1000.0);
 
            if (beatsPerMinute > 20 && beatsPerMinute < 255) {
                rates[rateIndex++ % 4] = (byte)beatsPerMinute;
                beatAvg = 0;
                for (byte x = 0; x < 4; x++) beatAvg += rates[x];
                beatAvg /= 4;
            }
        }
 
        hrSensor.nextSample();
 
        if (millis() - start > 10000) break; // 10s timeout
    }
 
    result.bpm   = beatAvg;
    result.valid = (beatAvg > 0);
    return result;
}
 
// ── MPU6050 — Accelerometer / Gyroscope (Adafruit driver) ─
 
bool sensors_initIMU() {
    return imu.begin();  // returns false if sensor not found on I2C bus
}
 
IMUData sensors_readIMU() {
    IMUData result;
 
    sensors_event_t accel, gyro, temp;
    imu.getEvent(&accel, &gyro, &temp);
 
    // accel in m/s², gyro in rad/s
    result.accelX = accel.acceleration.x;
    result.accelY = accel.acceleration.y;
    result.accelZ = accel.acceleration.z;
    result.gyroX  = gyro.gyro.x;
    result.gyroY  = gyro.gyro.y;
    result.gyroZ  = gyro.gyro.z;
    result.valid  = true;
 
    return result;
}
 
// ── DS18B20 — Temperature ─────────────────────────────────
 
bool sensors_initTemp() {
    tempSensor.begin();
    return (tempSensor.getDeviceCount() > 0);
}
 
float sensors_readTemp() {
    tempSensor.requestTemperatures();
    return tempSensor.getTempCByIndex(0);
}
 
// GPS functions removed
