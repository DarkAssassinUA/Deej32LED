#pragma once

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>

// ============================================================
// СТРУКТУРА ТЕМЫ
// ============================================================
struct GradTheme { uint8_t hue0, hue1, sat; };

// ============================================================
// EXTERN-ОБЪЯВЛЕНИЯ ВСЕХ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ
// ============================================================

// Метаданные
extern const char    *version;
extern const char    *AP_SSID;
extern const int      sensorPins[NUM_SLIDERS];
extern const uint16_t WS_PORT;

// Настройки (EEPROM)
extern int  globalSmoothing;
extern int  globalBrightness;
extern int  currentTheme;
extern int  channelTheme[NUM_SLIDERS];
extern bool vuMeterEnabled;

// WiFi
extern bool        apMode;
extern String      staSSID;
extern String      staPass;
extern Preferences wifiPrefs;
extern DNSServer   dnsServer;

// Скан
extern volatile bool g_scanRequest;
extern volatile bool g_scanReady;
extern String        g_scanResult;

// WebSocket / HTTP серверы
extern AsyncWebServer wsServer;
extern AsyncWebSocket ws;
extern AsyncWebServer httpServer;

// Состояние каналов
extern char          channelName[NUM_SLIDERS][32];
extern bool          isMuted[NUM_SLIDERS];
extern int           vuLevel[NUM_SLIDERS];
extern volatile bool pendingStateApply;
extern int           pendingVol[NUM_SLIDERS];
extern bool          pendingMute[NUM_SLIDERS];
extern int           currentVol[NUM_SLIDERS];

// FastLED
extern CRGB leds[TOTAL_LEDS];
extern const GradTheme gradThemes[NUM_THEMES];

// Интервал отправки WS
extern const unsigned long WS_SEND_INTERVAL_MS;
