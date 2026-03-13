#include "DeviceIdentity.h"

static Preferences prefs;

//get the esp mac address
String identity_getMac() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

//
bool identity_isAssigned() {
    prefs.begin("identity", true);
    String label = prefs.getString("label", "UNASSIGNED");
    prefs.end();
    return label != "UNASSIGNED";
}

//
void identity_assignLabel(String label) {
    prefs.begin("identity", false);
    prefs.putString("label", label);
    prefs.end();
    Serial.println("Device labeled: " + label);
    Serial.println("MAC: " + identity_getMac());
}

DeviceIdentity identity_get() {
    DeviceIdentity id;

    // get MAC
    String mac = identity_getMac();
    mac.toCharArray(id.macAddress, sizeof(id.macAddress));

    // get label
    prefs.begin("identity", true);
    String label = prefs.getString("label", "UNASSIGNED");
    prefs.end();
    label.toCharArray(id.animalLabel, sizeof(id.animalLabel));

    return id;
}

void identity_init() {
    if (!identity_isAssigned()) {
        Serial.begin(115200);
        Serial.println("════════════════════════════════");
        Serial.println("  No device label assigned.");
        Serial.println("  MAC: " + identity_getMac());
        Serial.println("  Send animal label within 30s:");
        Serial.println("════════════════════════════════");

        unsigned long start = millis();
        while (millis() - start < 30000) {
            if (Serial.available()) {
                String label = Serial.readStringUntil('\n');
                label.trim();
                if (label.length() > 0 && label.length() < LABEL_MAX_LENGTH) {
                    identity_assignLabel(label);
                    return;
                }
            }
        }

        // timed out without assignmen, retry next boot
        Serial.println("Timeout. Label not assigned, retrying next boot.");
    }
}
