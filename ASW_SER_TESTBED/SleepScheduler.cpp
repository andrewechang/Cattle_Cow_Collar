#include "SleepScheduler.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <driver/rtc_io.h>
#include <WiFi.h>
#include "Sensors.h"
 
#define WIFI_SSID       "your hotspot name"
#define WIFI_PASSWORD   "your hotspot password"
#define TS_API_KEY      "your write api key"
#define TS_CHANNEL_ID   "your channel id"
 
// ── RTC Memory Buffer ─────────────────────────────────────
RTC_DATA_ATTR SensorBuffer rtcBuffer;
RTC_DATA_ATTR bool firstBoot = true;
 
// ── Pin Setup ─────────────────────────────────────────────
// N-channel low-side MOSFET (VN0106):
//   HIGH = MOSFET on  = rail on  (non-inverted logic)
//   LOW  = MOSFET off = rail off
// Pull-down resistor (10kΩ gate to GND) holds gate LOW when ESP is not driving,
// keeping MOSFET off during boot, reset, flash, and deep sleep.
void _initLDOPin() {
    rtc_gpio_init(LDO_3V3_EN);
    rtc_gpio_set_direction(LDO_3V3_EN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_en(LDO_3V3_EN);   // pull LOW = MOSFET off when not driven
    rtc_gpio_pullup_dis(LDO_3V3_EN);
    rtc_gpio_set_level(LDO_3V3_EN, 0);  // start LOW = rail OFF
}
 
// ── Pin Hold Helpers ──────────────────────────────────────
void _releasePinHolds() {
    rtc_gpio_hold_dis(LDO_3V3_EN);
}
 
void _applyPinHolds() {
    rtc_gpio_set_level(LDO_3V3_EN, 0);  // LOW = MOSFET off = rail off
    rtc_gpio_hold_en(LDO_3V3_EN);       // latch LOW through deep sleep
}
 
// ── Power Control ─────────────────────────────────────────
void _powerOn(int sensor) {
    rtc_gpio_hold_dis(LDO_3V3_EN);      // release hold first
    rtc_gpio_set_level(LDO_3V3_EN, 1);  // then drive HIGH = MOSFET on = rail on
    delay(20);
    Wire.begin();
}
 
void _powerOff() {
    Wire.end();
    _applyPinHolds();  // drives LOW (rail off) then latches through sleep
}
 
// ── Sleep ─────────────────────────────────────────────────
// _powerOff is always called here so the transmit path (scheduler_sleep)
// also cuts the rail — previously it bypassed _powerOff entirely.
void _goToSleep(int seconds) {
    _powerOff();
    DEBUG_PRINTF("[SLEEP] Going to sleep for %d seconds\n\n", seconds);
    delay(100); // flush serial before sleep
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
}
 
// ── Sensor Reading & Storage ──────────────────────────────
void _storeReading(int sensor, int sleepSeconds) {
    int i = rtcBuffer.index;
    rtcBuffer.readings[i].timestamp = rtcBuffer.deviceUptime;
 
    DEBUG_PRINTLN("────────────────────────────────");
    DEBUG_PRINTF("[DEBUG] Uptime: %lus  |  Buffer: %d/%d  |  Transmit ready: %s\n",
        rtcBuffer.deviceUptime,
        rtcBuffer.count + 1,
        MAX_READINGS,
        rtcBuffer.transmitReady ? "YES" : "no"
    );
 
    switch (sensor) {
        case SENSOR_TEMP: {
            DEBUG_PRINTLN("[TEMP] Reading DS18B20...");
            if (sensors_initTemp()) {
                float t = sensors_readTemp();
                rtcBuffer.readings[i].temperature = t;
                rtcBuffer.readings[i].tempValid   = true;
                DEBUG_PRINTF("[TEMP] %.2f °C\n", t);
            } else {
                rtcBuffer.readings[i].temperature = 0.0;
                rtcBuffer.readings[i].tempValid   = false;
                DEBUG_PRINTLN("[TEMP] ERROR — sensor not found");
            }
            break;
        }
        case SENSOR_HR: {
            DEBUG_PRINTLN("[HR] Reading MAX30102...");
            if (sensors_initHR()) {
                HeartRateData data = sensors_readHR();
                rtcBuffer.readings[i].heartRate = data.bpm;
                rtcBuffer.readings[i].spo2      = data.spo2;
                rtcBuffer.readings[i].hrValid   = data.valid;
                if (data.valid) {
                    DEBUG_PRINTF("[HR] BPM: %d  |  SpO2: %.1f%%\n", data.bpm, data.spo2);
                } else {
                    DEBUG_PRINTLN("[HR] No finger detected or invalid reading");
                }
            } else {
                rtcBuffer.readings[i].hrValid = false;
                DEBUG_PRINTLN("[HR] ERROR — sensor not found");
            }
            break;
        }
        case SENSOR_IMU: {
            DEBUG_PRINTLN("[IMU] Reading MPU6050...");
            if (sensors_initIMU()) {
                IMUData data = sensors_readIMU();
                rtcBuffer.readings[i].accelX = data.accelX;
                rtcBuffer.readings[i].accelY = data.accelY;
                rtcBuffer.readings[i].accelZ = data.accelZ;
                rtcBuffer.readings[i].gyroX  = data.gyroX;
                rtcBuffer.readings[i].gyroY  = data.gyroY;
                rtcBuffer.readings[i].gyroZ  = data.gyroZ;
                DEBUG_PRINTF("[IMU] Accel  X: %.3f  Y: %.3f  Z: %.3f m/s²\n",
                    data.accelX, data.accelY, data.accelZ);
                DEBUG_PRINTF("[IMU] Gyro   X: %.3f  Y: %.3f  Z: %.3f rad/s\n",
                    data.gyroX, data.gyroY, data.gyroZ);
            } else {
                DEBUG_PRINTLN("[IMU] ERROR — sensor not found");
            }
            break;
        }
    }
 
    rtcBuffer.index++;
    rtcBuffer.count++;
    rtcBuffer.deviceUptime += sleepSeconds;
 
    if (rtcBuffer.index >= MAX_READINGS) {
        rtcBuffer.transmitReady = true;
        rtcBuffer.index = 0;
        DEBUG_PRINTLN("[BUFFER] Full — transmit flagged");
    }
}
 
// ── Public API ────────────────────────────────────────────
void scheduler_init() {
    DEBUG_BEGIN();
    _initLDOPin();
    identity_init();
 
    if (firstBoot) {
        rtcBuffer.index         = 0;
        rtcBuffer.transmitReady = false;
        rtcBuffer.deviceUptime  = 0;
        memset(rtcBuffer.readings, 0, sizeof(rtcBuffer.readings));
        rtcBuffer.device = identity_get();
        firstBoot = false;
        DEBUG_PRINTLN("[INIT] First boot — buffer cleared");
    }
}
 
void scheduler_run(int sensor, int sleepSeconds) {
    _powerOn(sensor);
    _storeReading(sensor, sleepSeconds);
    _goToSleep(sleepSeconds);  // _powerOff is now inside _goToSleep
}
 
bool scheduler_isTransmitReady() {
    return rtcBuffer.transmitReady;
}
 
SensorBuffer* scheduler_getBuffer() {
    return &rtcBuffer;
}
 
void scheduler_transmit() {
    DEBUG_PRINTLN("[TX] Buffer full — attempting transmit...");
 
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
        DEBUG_PRINTLN("[TX] WiFi failed — will retry next cycle");
        return;
    }
 
    DEBUG_PRINTLN("[TX] WiFi connected — sending payload...");
 
    // ── Build JSON payload ────────────────────────────────
    String json = "{\"write_api_key\":\"YOUR_API_KEY\",\"updates\":[";
 
    for (int i = 0; i < rtcBuffer.count; i++) {
        SensorReading r = rtcBuffer.readings[i];
        uint32_t delta = (i == 0) ? 0 : r.timestamp - rtcBuffer.readings[i-1].timestamp;
 
        json += "{";
        json += "\"delta_t\":"  + String(delta) + ",";
        json += "\"field1\":"   + String(r.heartRate) + ",";
        json += "\"field2\":"   + String(r.spo2, 1) + ",";
        json += "\"field3\":"   + String(r.temperature, 1) + ",";
        json += "\"field4\":"   + String(r.accelX, 4) + ",";
        json += "\"field5\":"   + String(r.accelY, 4) + ",";
        json += "\"field6\":"   + String(r.accelZ, 4) + ",";
        json += "\"field7\":"   + String(r.timestamp) + ",";
        json += "\"field8\":"   + String(r.hrValid ? 1 : 0);
        json += "}";
 
        if (i < rtcBuffer.count - 1) json += ",";
    }
 
    json += "]}";
 
    // ── Send HTTP POST ────────────────────────────────────
    WiFiClient client;
    if (!client.connect("api.thingspeak.com", 80)) {
        DEBUG_PRINTLN("[TX] Failed to connect to ThingSpeak");
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
 
    uint32_t timeout = millis();
    bool success = false;
    while (client.connected() && millis() - timeout < 5000) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.indexOf("202") >= 0) {
                success = true;
                break;
            }
        }
    }
 
    DEBUG_PRINTF("[TX] %s\n", success ? "Success — 202 Accepted" : "No 202 response received");
 
    client.stop();
    WiFi.disconnect(true);
 
    // ── Reset buffer ──────────────────────────────────────
    rtcBuffer.transmitReady = false;
    rtcBuffer.index = 0;
    rtcBuffer.count = 0;
    DEBUG_PRINTLN("[TX] Buffer reset");
}
 
void scheduler_sleep(int seconds) {
    _goToSleep(seconds);  // _powerOff now handled inside _goToSleep
}
