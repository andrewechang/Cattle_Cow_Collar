#include "SleepScheduler.h"
#include "Sensors.h"

static const int schedule[] = {
    SENSOR_TEMP,
    SENSOR_GPS,
    SENSOR_TEMP,   //this is the schedule layout, modify the order however, it will loop around
    SENSOR_HR,
    SENSOR_TEMP,
    SENSOR_GPS,
    SENSOR_TEMP,
    SENSOR_HR
};
static const int SCHEDULE_LEN = sizeof(schedule) / sizeof(schedule[0]); // auto calculates the length of the schedule list

RTC_DATA_ATTR int scheduleIndex = 0; // tracks where we are in the schedule

void setup() {
    scheduler_init();

    // transmit only wake — skip sensor read entirely
    if (scheduler_isTransmitReady()) {
    scheduler_transmit();
    scheduler_sleep(HR_INTERVAL);
    }

    // normal wake — read next sensor in schedule
    int sensor = schedule[scheduleIndex];
    scheduleIndex = (scheduleIndex + 1) % SCHEDULE_LEN;
    scheduler_run(sensor, HR_INTERVAL);
}

void loop() {} //since we're running deep sleep cycles wwe just cant use this