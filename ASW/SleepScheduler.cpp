#include "SleepScheduler.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <driver/rtc_io.h>
#include "Sensors.h"

#define WIFI_SSID       "4G-UFI-157039"
#define WIFI_PASSWORD   "admin"
#define TS_API_KEY      "1YAEBATA7OQSTKPE"
#define TS_CHANNEL_ID   "3297831"

// ── RTC Memory Buffer ────────────────────────────────────
// This persists across deep sleep cycles
RTC_DATA_ATTR SensorBuffer rtcBuffer;
RTC_DATA_ATTR bool firstBoot = true;

//releases the hold on a pins level
void _releasePinHolds() {
    gpio_hold_dis(LDO_3V3_EN);
    gpio_hold_dis(LDO_1V8_EN);
}
//apply a hold to the pins level, keep high or keep low during sleep
void _applyPinHolds() {
    gpio_hold_en(LDO_3V3_EN);
    gpio_hold_en(LDO_1V8_EN);
    gpio_deep_sleep_hold_en();
}
//Voltage regulator control for powering on and off specific sensors, again in the case this is not for the PCB, these control mosfets
void _powerOn(int sensor) {
    _releasePinHolds();

    if (sensor == SENSOR_GPS) {
        // GPS is on 3V3 rail but managed separately
        digitalWrite(LDO_3V3_EN, HIGH);
    } else {
        // HR, temp, IMU all on 3V3 rail
        digitalWrite(LDO_3V3_EN, HIGH);
        digitalWrite(LDO_1V8_EN, HIGH);
    }

    // give LDO and sensor time to stabilize
    delay(20);
    Wire.begin();
}

void _powerOff() {
    Wire.end();
    digitalWrite(LDO_3V3_EN, LOW);
    digitalWrite(LDO_1V8_EN, LOW);
    _applyPinHolds();
}
//set the esp to sleep
void _goToSleep(int seconds) {
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
}


//storage code, claude helped with most of this part
void _storeReading(int sensor, int sleepSeconds) {
    int i = rtcBuffer.index;

    // timestamp this reading
    rtcBuffer.readings[i].timestamp = rtcBuffer.deviceUptime;

    switch (sensor) {
        case SENSOR_TEMP: {
            if (sensors_initTemp()) {
                rtcBuffer.readings[i].temperature = sensors_readTemp();
                rtcBuffer.readings[i].tempValid = true;
            } else {
                rtcBuffer.readings[i].temperature = 0.0;
                rtcBuffer.readings[i].tempValid = false;
            }
            break;
        }
        case SENSOR_HR: {
            if (sensors_initHR()) {
                HeartRateData data = sensors_readHR();
                rtcBuffer.readings[i].heartRate = data.bpm;
                rtcBuffer.readings[i].spo2      = data.spo2;
                rtcBuffer.readings[i].hrValid   = data.valid;
            } else {
                rtcBuffer.readings[i].hrValid = false;
            }
            break;
        }
        case SENSOR_IMU: {
            if (sensors_initIMU()) {
                IMUData data = sensors_readIMU();
                rtcBuffer.readings[i].accelX = data.accelX;
                rtcBuffer.readings[i].accelY = data.accelY;
                rtcBuffer.readings[i].accelZ = data.accelZ;
                rtcBuffer.readings[i].gyroX  = data.gyroX;
                rtcBuffer.readings[i].gyroY  = data.gyroY;
                rtcBuffer.readings[i].gyroZ  = data.gyroZ;
            }
            break;
        }
        case SENSOR_GPS: {
            sensors_initGPS();
            GPSData data = sensors_readGPS(30);
            rtcBuffer.readings[i].latitude  = data.latitude;
            rtcBuffer.readings[i].longitude = data.longitude;
            rtcBuffer.readings[i].altitude  = data.altitude;
            rtcBuffer.readings[i].gpsValid  = data.valid;
            break;
        }
    }

    rtcBuffer.index++;
    rtcBuffer.count++;
    rtcBuffer.deviceUptime += sleepSeconds; // increment by sleep interval

    if (rtcBuffer.index >= MAX_READINGS) {
        rtcBuffer.transmitReady = true;
        rtcBuffer.index = 0;
    }
}

//first initializer
void scheduler_init() {
    pinMode(LDO_3V3_EN, OUTPUT);
    pinMode(LDO_1V8_EN, OUTPUT);
    digitalWrite(LDO_3V3_EN, LOW);
    digitalWrite(LDO_1V8_EN, LOW);

    // always run identity init and dont run when already assigned
    identity_init();

    if (firstBoot) {
        rtcBuffer.index         = 0;
        rtcBuffer.transmitReady = false;
        rtcBuffer.deviceUptime  = 0;
        memset(rtcBuffer.readings, 0, sizeof(rtcBuffer.readings));
        rtcBuffer.device = identity_get();

        firstBoot = false;
    }
}

void scheduler_run(int sensor, int sleepSeconds) {
    _powerOn(sensor);
    _storeReading(sensor, sleepSeconds);
    _powerOff();
    _goToSleep(sleepSeconds);
    // execution never reaches here
}

bool scheduler_isTransmitReady() {
    return rtcBuffer.transmitReady;
}

SensorBuffer* scheduler_getBuffer() {
    return &rtcBuffer;
}

void scheduler_transmit() {
    // ── Connect to WiFi ──────────────────────────────────
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(true, true);
    delay(300);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(300);
    }

    if (WiFi.status() != WL_CONNECTED) {
        // WiFi failed — don't reset buffer, try again next cycle
        return;
    }

    // ── Build JSON payload ───────────────────────────────
    String json = "{\"write_api_key\":\"YOUR_API_KEY\",\"updates\":[";

    for (int i = 0; i < rtcBuffer.count; i++) {
        SensorReading r = rtcBuffer.readings[i];

        uint32_t delta = (i == 0) ? 0 : r.timestamp - rtcBuffer.readings[i-1].timestamp;

        json += "{";
        json += "\"delta_t\":" + String(delta) + ",";
        json += "\"field1\":" + String(r.heartRate) + ",";
        json += "\"field2\":" + String(r.spo2, 1) + ",";
        json += "\"field3\":" + String(r.temperature, 1) + ",";
        json += "\"field4\":" + String(r.latitude, 6) + ",";
        json += "\"field5\":" + String(r.longitude, 6) + ",";
        json += "\"field6\":" + String(r.altitude, 1) + ",";
        json += "\"field7\":" + String(r.timestamp) + ",";
        json += "\"field8\":" + String(r.hrValid ? 1 : 0);
        json += "}";

        if (i < rtcBuffer.count - 1) json += ",";
    }

    json += "]}";

    // ── Send HTTP POST ───────────────────────────────────
    WiFiClient client;
    if (!client.connect("api.thingspeak.com", 80)) {
        WiFi.disconnect(true);
        return;
    }

    String path = "/channels/YOUR_CHANNEL_ID/bulk_update.json";

    client.print("POST " + path + " HTTP/1.1\r\n");
    client.print("Host: api.thingspeak.com\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: " + String(json.length()) + "\r\n\r\n");
    client.print(json);

    // ── Read response ────────────────────────────────────
    uint32_t timeout = millis();
    while (client.connected() && millis() - timeout < 5000) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.indexOf("202") >= 0) {
                // 202 Accepted = success
                break;
            }
        }
    }
    client.stop();
    WiFi.disconnect(true);

    // ── Reset buffer ─────────────────────────────────────
    rtcBuffer.transmitReady = false;
    rtcBuffer.index = 0;
    rtcBuffer.count = 0;
}

void scheduler_sleep(int seconds) {
    _goToSleep(seconds);
}