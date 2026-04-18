#include "globals.h"

// ============================================================
// МЕТАДАННЫЕ
// ============================================================
const char    *version = "0.75";
const char    *AP_SSID = "Deej32Led-Setup";
const uint16_t WS_PORT = WS_PORT_NUM;
const int      sensorPins[NUM_SLIDERS] = {36, 39, 34, 35, 32};

// ============================================================
// НАСТРОЙКИ
// ============================================================
int  globalSmoothing  = 20;
int  globalBrightness = 60;
int  currentTheme     = 0;
int  channelTheme[NUM_SLIDERS];
bool vuMeterEnabled   = true;

// ============================================================
// WiFi
// ============================================================
bool        apMode   = false;
String      staSSID  = "";
String      staPass  = "";
Preferences wifiPrefs;
DNSServer   dnsServer;

// ============================================================
// СКАН
// ============================================================
volatile bool g_scanRequest = false;
volatile bool g_scanReady   = false;
String        g_scanResult  = "[]";

// ============================================================
// СЕРВЕРЫ
// ============================================================
AsyncWebServer wsServer(WS_PORT_NUM);
AsyncWebSocket ws("/ws");
AsyncWebServer httpServer(80);

// ============================================================
// СОСТОЯНИЕ КАНАЛОВ
// ============================================================
char          channelName[NUM_SLIDERS][32];
bool          isMuted[NUM_SLIDERS];
int           vuLevel[NUM_SLIDERS];
volatile bool pendingStateApply = false;
int           pendingVol[NUM_SLIDERS];
bool          pendingMute[NUM_SLIDERS];
int           currentVol[NUM_SLIDERS];

const unsigned long WS_SEND_INTERVAL_MS = 100;

// ============================================================
// FastLED
// ============================================================
CRGB leds[TOTAL_LEDS];

//  0: VU Classic   1: Aurora      2: Ember     3: Synthwave
//  4: Ocean        5: Forest      6: Sunset    7: Cherry
//  8: Mint         9: Ice        10: Galaxy   11: Toxic
const GradTheme gradThemes[NUM_THEMES] = {
    {96,  0,   240}, //  0: VU Classic
    {130, 190, 255}, //  1: Aurora
    {0,   50,  255}, //  2: Ember
    {192, 248, 230}, //  3: Synthwave
    {160, 128, 220}, //  4: Ocean
    {90,  96,  235}, //  5: Forest
    {20,  210, 245}, //  6: Sunset
    {248, 240, 245}, //  7: Cherry
    {105, 130, 185}, //  8: Mint
    {148, 155, 90},  //  9: Ice
    {170, 224, 255}, // 10: Galaxy
    {85,  64,  255}, // 11: Toxic
};
