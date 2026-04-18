#include "ota_manager.h"
#include "globals.h"
#include <ArduinoOTA.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

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

// ============================================================
// CLOUD OTA (Загрузка обновления из WebDash / GitHub)
// ============================================================
void startCloudOTA(String url) {
    Serial.println("[OTA] Starting cloud update from: " + url);
    WiFiClientSecure client;
    client.setInsecure(); // Игнорируем проверку SSL-сертификата (нужно для GitHub)
    
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // GitHub переадресует на AWS
    
    httpUpdate.onProgress([](int curr, int total) {
        int pct = curr * 100 / (total > 0 ? total : 1);
        int lit = map(pct, 0, 100, 0, LEDS_PER_SEG);
        for (int i = 0; i < LEDS_PER_SEG; i++)
            leds[i] = (i < lit) ? CRGB(200, 150, 0) : CRGB::Black; // Оранжевая полоса при загрузке с облака
        FastLED.show();
    });

    FastLED.clear(); 
    FastLED.show();
    
    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Failed! Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No updates");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update OK! Rebooting...");
            ESP.restart();
            break;
    }
}
