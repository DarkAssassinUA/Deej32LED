#include "led_effects.h"
#include "globals.h"

// ============================================================
// СТАТУС-ИНДИКАЦИЯ СВЕТОДИОДАМИ
// ============================================================
void updateStatusLeds() {
    unsigned long now = millis();
    bool noWifi = apMode || (WiFi.status() != WL_CONNECTED);
    bool noApp  = (now - lastWsActivity > 3000);

    // Плавная пульсация (около 20 ударов в минуту, от 10_ до 255 яркости)
    uint8_t breath = beatsin8(20, 10, 255); 

    // Если нет связи (либо WiFi, либо вебсокет) — гасим все фейдеры, кроме индикационных
    if (noWifi || noApp) {
        fill_solid(leds, TOTAL_LEDS, CRGB::Black);
    }

    // 1) Нет WiFi -> 2-й фейдер мигает синим
    if (noWifi) {
        CRGB blueCol = CRGB(0, 0, breath);
        for (int i = 0; i < LEDS_PER_SEG; i++) {
            leds[1 * LEDS_PER_SEG + i] = blueCol;
        }
    }

    // 2) Нет Deej/Bridge -> 3-й фейдер мигает фиолетовым
    if (!noWifi && noApp) {
        CRGB purpleCol = CRGB(breath / 2, 0, breath);
        for (int i = 0; i < LEDS_PER_SEG; i++) {
            leds[2 * LEDS_PER_SEG + i] = purpleCol;
        }
    }
}
