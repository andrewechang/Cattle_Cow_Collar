#include "Sensors.h"

static MAX30105       hrSensor;
static MPU6050        imu;
static OneWire        oneWire(ONE_WIRE_PIN);
static DallasTemperature tempSensor(&oneWire);
static TinyGPSPlus    gps;

// MAX30102 — Heart Rate & SpO2

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

    const int SAMPLES     = 100;
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
            // no finger detected
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

        if (millis() - start > 10000) break; // 10 second timeout
    }

    result.bpm   = beatAvg;
    result.valid = (beatAvg > 0);
    return result;
}

// MPU6050 — Accelerometer/Gyroscope

bool sensors_initIMU() {
    imu.initialize();
    return imu.testConnection();
}

IMUData sensors_readIMU() {
    IMUData result;
    int16_t ax, ay, az, gx, gy, gz;

    imu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    // convert raw to g's and degrees/sec
    result.accelX = ax / 16384.0;
    result.accelY = ay / 16384.0;
    result.accelZ = az / 16384.0;
    result.gyroX  = gx / 131.0;
    result.gyroY  = gy / 131.0;
    result.gyroZ  = gz / 131.0;
    result.valid  = true;

    return result;
}

// DS18B20 — Temperature

bool sensors_initTemp() {
    tempSensor.begin();
    return (tempSensor.getDeviceCount() > 0);
}

float sensors_readTemp() {
    tempSensor.requestTemperatures();
    float temp = tempSensor.getTempCByIndex(0);
    return temp;
}

// SAM-M8Q — GPS

bool sensors_initGPS() {
    GPS_SERIAL.begin(GPS_BAUD);
    return true; // UART init always succeeds, valid flag on read
}

GPSData sensors_readGPS(int timeoutSeconds) {
    GPSData result = {0.0, 0.0, 0.0, false};
    unsigned long start = millis();
    unsigned long timeout = (unsigned long)timeoutSeconds * 1000;

    while (millis() - start < timeout) {
        while (GPS_SERIAL.available()) {
            gps.encode(GPS_SERIAL.read());
        }

        if (gps.location.isValid() && gps.location.isUpdated()) {
            result.latitude  = gps.location.lat();
            result.longitude = gps.location.lng();
            result.altitude  = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
            result.valid     = true;
            break;
        }
    }

    return result;
}