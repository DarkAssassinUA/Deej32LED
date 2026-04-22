// ============================================================
// Deej32Led — main.cpp
// Содержит только setup() и loop().
// Вся логика вынесена в отдельные модули.
//
// Индикация статусов (LED Status):
// - CH1: заполняется белым/зеленым во время OTA-обновлений
// - CH2: плавно мигает синим при потере сети Wi-Fi
// - CH3: плавно мигает фиолетовым, если не запущен DeejNG (нет связи с ПК)
// ============================================================
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

#include "config.h"
#include "globals.h"
#include "slider.h"
#include "settings.h"
#include "led_effects.h"
#include "ws_handler.h"
#include "ota_manager.h"
#include "wifi_manager.h"
#include "web_server.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>

// Экземпляры слайдеров
SliderControl sliders[NUM_SLIDERS];

// ============================================================
// SETUP
// ============================================================
void setup() {
    // FastLED первым — гасим LED до загрузки настроек
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, TOTAL_LEDS);
    FastLED.setBrightness(20);
    FastLED.clear();
    FastLED.show();

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // отключить brownout

    Serial.begin(115200);
    delay(200);

    // CPU 80 MHz: экономит ~50 мА без потери производительности
    setCpuFrequencyMhz(80);
    Serial.printf("\n\n=== Deej32Led v%s boot === CPU: %u MHz ===\n",
                  version, getCpuFrequencyMhz());

    analogReadResolution(10);
    analogSetAttenuation(ADC_11db);
    loadSettings();

    FastLED.setBrightness(globalBrightness);
    FastLED.show();

    // Инициализация состояния каналов
    for (int i = 0; i < NUM_SLIDERS; i++) {
        snprintf(channelName[i], sizeof(channelName[i]), "CH %d", i + 1);
        isMuted[i]      = false;
        vuLevel[i]      = 0;
        currentVol[i]   = 0;
        pendingVol[i]   = 0;
        pendingMute[i]  = false;
        channelTheme[i] = currentTheme; // перезапишется из loadSettings EEPROM
    }
    for (int i = 0; i < NUM_SLIDERS; i++) sliders[i].init(i);

    // Загрузочная анимация (до WiFi)
    bootAnimation();

    // ── WiFi ──────────────────────────────────────────────────
    loadWiFiCredentials();

    if (staSSID.isEmpty()) {
        Serial.println("[WiFi] No credentials → AP mode");
    } else {
        Serial.printf("[WiFi] Connecting to '%s'...\n", staSSID.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(staSSID.c_str(), staPass.c_str());
        unsigned long t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
            delay(500); Serial.print(".");
        }
        Serial.println();
    }

    if (WiFi.status() == WL_CONNECTED) {
        // ── STA режим ─────────────────────────────────────────
        apMode = false;
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

        // Modem Sleep: ~50–80 мА экономии в паузах между TX
        WiFi.setSleep(WIFI_PS_MIN_MODEM);
        Serial.println("[WiFi] Modem sleep enabled");

        // mDNS: http://deej32led.local
        if (MDNS.begin("deej32led")) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addService("ws",   "tcp", WS_PORT);
            Serial.println("[mDNS] http://deej32led.local");
        }

        ws.onEvent(onWsEvent);
        wsServer.addHandler(&ws);
        wsServer.begin();
        Serial.printf("[WS]   Server started on port %d\n", WS_PORT);

        setupOTA();
        setupHttpServer();
    } else {
        // ── AP режим (настройка WiFi) ──────────────────────────
        apMode = true;
        Serial.println("[WiFi] Starting AP mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID);
        delay(200);
        Serial.printf("[AP]   IP: %s\n", WiFi.softAPIP().toString().c_str());

        for (int i = 0; i < LEDS_PER_SEG; i++)
            leds[i] = (i % 3 == 0) ? CRGB(80, 60, 0) : CRGB::Black;
        FastLED.show();

        setupApServer();
    }

    Serial.println("=== setup complete ===");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
    // WiFi скан выполняется здесь, а не в HTTP-callback
    if (g_scanRequest) {
        g_scanRequest = false;
        g_scanResult  = buildScanJson();
        g_scanReady   = true;
    }

    if (apMode) {
        dnsServer.processNextRequest();
        updateStatusLeds();
        FastLED.show();
        return;
    }

    ArduinoOTA.handle();
    unsigned long now = millis();

    // Применяем pending state от хоста
    if (pendingStateApply) {
        pendingStateApply = false;
        for (int i = 0; i < NUM_SLIDERS; i++) {
            currentVol[i] = pendingVol[i];
            isMuted[i]    = pendingMute[i];
        }
    }

    // Обновляем слайдеры и LED
    static unsigned long lastActivity = millis();
    bool faderMoved = false;

    for (int i = 0; i < NUM_SLIDERS; i++) {
        int oldPos = sliders[i].avgPos;
        sliders[i].update(now);
        if (abs(sliders[i].avgPos - oldPos) > 10) faderMoved = true;
    }

    if ((now - lastWsActivity < 3000) || faderMoved) {
        lastActivity = now;
        if (deviceAsleep) {
            deviceAsleep = false; 
            Serial.println("[SYSTEM] Waking up from sleep.");
        }
    }

    unsigned long inactiveTime = now - lastActivity;
    uint8_t targetBrt = globalBrightness;

    if (inactiveTime >= 120000) {
        targetBrt = 0;
        if (!deviceAsleep) {
            deviceAsleep = true;
            Serial.println("[SYSTEM] No connection / activity for 2 mins. Sleeping...");
        }
    } else {
        // Плавно гасим в течение всех 2 минут (120000 мс)
        targetBrt = map(inactiveTime, 0, 120000, globalBrightness, 0);
        deviceAsleep = false;
    }

    FastLED.setBrightness(targetBrt);


    if (deviceAsleep) {
        FastLED.clear();
    } else {
        updateStatusLeds();
    }
    FastLED.show();

    // Отправка данных в WebSocket и Serial
    static unsigned long lastSend = 0;
    if (now - lastSend >= WS_SEND_INTERVAL_MS) {
        lastSend = now;

        String deejData = "";
        bool stateChanged = false;
        for (int i = 0; i < NUM_SLIDERS; i++) {
            int vol = map(sliders[i].avgPos, 0, 1023, 0, 100);
            if (abs(vol - currentVol[i]) > 1) {
                currentVol[i] = vol;
                stateChanged  = true;
            }
            int deejVal = isMuted[i] ? 0 : sliders[i].avgPos;
            deejData += String(deejVal) + "|";
        }
        Serial.println(deejData);

        bool anyGesture = false;
        static bool lastBak[NUM_SLIDERS] = {};
        static bool lastCon[NUM_SLIDERS] = {};
        for (int i = 0; i < NUM_SLIDERS; i++) {
            if (virtualBtnToggle[i] != lastBak[i]) { lastBak[i] = virtualBtnToggle[i]; anyGesture = true; }
            if (virtualConToggle[i] != lastCon[i]) { lastCon[i] = virtualConToggle[i]; anyGesture = true; }
        }
        if (ws.count() > 0 && (stateChanged || anyGesture)) sendWsState();

        static unsigned long lastCleanup = 0;
        if (now - lastCleanup >= 1000) {
            lastCleanup = now;
            ws.cleanupClients();
        }
    }
}