#include "settings.h"
#include "globals.h"
#include <EEPROM.h>

// ============================================================
// EEPROM — загрузка настроек
// ============================================================
void loadSettings() {
    EEPROM.begin(EEPROM_SIZE);
    globalSmoothing  = EEPROM.read(8);
    currentTheme     = EEPROM.read(9);
    globalBrightness = EEPROM.read(10);
    uint8_t vuByte   = EEPROM.read(11);
    if (globalSmoothing < 1 || globalSmoothing > 100) globalSmoothing  = 20;
    if (globalBrightness < 5 || globalBrightness > 255) globalBrightness = 60;
    if (currentTheme >= NUM_THEMES) currentTheme = 0;
    // 0xFF = ячейка не записана → дефолт true
    vuMeterEnabled = (vuByte == 0xFF) ? true : (vuByte != 0);
    // Индивидуальные темы каналов (slots 12-16)
    for (int i = 0; i < NUM_SLIDERS; i++) {
        uint8_t b = EEPROM.read(12 + i);
        channelTheme[i] = (b < NUM_THEMES) ? (int)b : currentTheme;
    }
    EEPROM.end();
}

// ============================================================
// ЗАГРУЗОЧНАЯ АНИМАЦИЯ
// ============================================================
void bootAnimation() {
    // Пробег радуги от базы к концу ленты
    for (int pos = 0; pos < TOTAL_LEDS; pos++) {
        leds[pos] = CHSV(map(pos, 0, TOTAL_LEDS - 1, 0, 224), 230, 255);
        if (pos > 0) leds[pos - 1].fadeToBlackBy(60);
        FastLED.show();
        delay(7);
    }
    // Плавное затухание
    for (int v = 0; v < 20; v++) {
        fadeToBlackBy(leds, TOTAL_LEDS, 30);
        FastLED.show();
        delay(12);
    }
    FastLED.clear();
    FastLED.show();
}
