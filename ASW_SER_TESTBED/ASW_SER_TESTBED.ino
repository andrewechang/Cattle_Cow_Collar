#include "SleepScheduler.h"
#include "Sensors.h"

static const int schedule[] = {
    SENSOR_TEMP,
    SENSOR_HR,
    SENSOR_TEMP,   // modify order however; it loops around
};
static const int SCHEDULE_LEN = sizeof(schedule) / sizeof(schedule[0]);

RTC_DATA_ATTR int scheduleIndex = 0;

void setup() {
    scheduler_init();

    // transmit only wake — skip sensor read entirely
    if (scheduler_isTransmitReady()) {
        scheduler_transmit();
        scheduler_sleep(TEST_INTERVAL);
    }

    // normal wake — read next sensor in schedule
    int sensor = schedule[scheduleIndex];
    scheduleIndex = (scheduleIndex + 1) % SCHEDULE_LEN;
    scheduler_run(sensor, TEST_INTERVAL);
}

void loop() {} // deep sleep architecture — loop never runs
