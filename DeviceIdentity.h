#ifndef DEVICEIDENTITY_H
#define DEVICEIDENTITY_H
 
#include "Arduino.h"
#include "Preferences.h"
#include "esp_system.h"
#include "esp_mac.h"
 
#define LABEL_MAX_LENGTH 32
 
struct DeviceIdentity {
    char macAddress[18];
    char animalLabel[LABEL_MAX_LENGTH];
};
 
void           identity_init();
DeviceIdentity identity_get();
bool           identity_isAssigned();
void           identity_assignLabel(String label);
String         identity_getMac();
 
#endif