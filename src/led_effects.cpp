#include "led_effects.h"
#include "globals.h"

// ============================================================
// СТАТУС-ИНДИКАЦИЯ СВЕТОДИОДАМИ
//  • AP mode (нет WiFi) → сег 0 и 4 мигают красным ~30%
//  • WiFi есть, нет WS  → сег 1 и 3 мигают синим
//  • WS подключён        → ничего не трогаем (слайдеры рисуют сами)
// ============================================================
void updateStatusLeds() {
    static unsigned long lastToggle = 0;
    static bool          blinkOn    = false;

    unsigned long now = millis();
    if (now - lastToggle >= 500) {
        lastToggle = now;
        blinkOn    = !blinkOn;
    }

    if (apMode) {
        CRGB redCol = blinkOn ? CRGB(77, 0, 0) : CRGB::Black;
        for (int i = 0; i < LEDS_PER_SEG; i++) {
            leds[i]                                        = redCol; // сег 0
            leds[(NUM_SLIDERS - 1) * LEDS_PER_SEG + i]   = redCol; // сег 4
        }
        for (int i = LEDS_PER_SEG; i < (NUM_SLIDERS - 1) * LEDS_PER_SEG; i++)
            leds[i] = CRGB::Black;
    } else if (ws.count() == 0) {
        CRGB blueCol = blinkOn ? CRGB(0, 0, 200) : CRGB::Black;
        for (int i = 0; i < LEDS_PER_SEG; i++) {
            leds[1 * LEDS_PER_SEG + i] = blueCol; // сег 1
            leds[3 * LEDS_PER_SEG + i] = blueCol; // сег 3
        }
    }
}
