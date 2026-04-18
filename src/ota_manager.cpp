#include "ota_manager.h"
#include "globals.h"
#include <ArduinoOTA.h>

void setupOTA() {
    ArduinoOTA.setHostname("deej32led");

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.println("[OTA] Start: " + type);
        FastLED.clear();
        FastLED.show();
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] End — restarting");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static int lastPct = -1;
        int pct = progress * 100 / total;
        if (pct != lastPct) {
            lastPct = pct;
            Serial.printf("[OTA] %u%%\r", pct);
            int lit = map(pct, 0, 100, 0, LEDS_PER_SEG);
            for (int i = 0; i < LEDS_PER_SEG; i++)
                leds[i] = (i < lit) ? CRGB(0, 80, 255) : CRGB::Black;
            FastLED.show();
        }
    });

    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("[OTA] Error[%u]: ", err);
        if      (err == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
        else if (err == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
        else if (err == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (err == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (err == OTA_END_ERROR)     Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] ArduinoOTA ready — hostname: deej32led.local\n");
}
