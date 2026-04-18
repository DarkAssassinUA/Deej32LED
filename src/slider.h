#pragma once

#include "globals.h"

// ============================================================
// КЛАСС СЛАЙДЕРА
// Отвечает за: чтение ADC, сглаживание, плавные переходы,
// peak hold, рендер LED-сегмента.
// ============================================================
class SliderControl {
public:
  int  id;
  int  readings[100];
  int  readIndex = 0;
  long total     = 0;
  int  avgPos    = 0;

  // Плавные переходы: displayLimit интерполируется к sliderLimit
  float displayLimit = 0.0f;

  // Peak hold
  float         peakLed    = 0.0f;  // текущая позиция пика (дробная)
  unsigned long peakHeldAt = 0;     // когда пик был обновлён
  unsigned long lastDecayAt= 0;     // последний такт убывания

  void init(int index) {
    id = index;
    readIndex = 0;
    int initial = analogRead(sensorPins[id]);
    total = 0;
    for (int i = 0; i < 100; i++)
      readings[i] = (i < globalSmoothing) ? initial : 0;
    total = (long)initial * globalSmoothing;
    displayLimit = 0.0f;
    peakLed      = 0.0f;
    peakHeldAt   = 0;
    lastDecayAt  = 0;
  }

  void update(unsigned long now) {
    total -= readings[readIndex];
    readings[readIndex] = analogRead(sensorPins[id]);
    total += readings[readIndex];
    readIndex = (readIndex + 1) % globalSmoothing;
    int rawAvg = total / globalSmoothing;
    avgPos = constrain(map(rawAvg, 8, 1015, 0, 1023), 0, 1023);
    render(now);
  }

  void render(unsigned long now) {
    int  sliderLimit = map(avgPos, 0, 1023, 0, LEDS_PER_SEG);
    int  startIdx    = id * LEDS_PER_SEG;
    bool wsActive    = (ws.count() > 0);
    const GradTheme &t = gradThemes[constrain(channelTheme[id], 0, NUM_THEMES - 1)];
    int vuBar = (vuMeterEnabled && wsActive && vuLevel[id] > 0)
                    ? map(vuLevel[id], 0, 100, 0, LEDS_PER_SEG) : -1;

    // ── Плавные переходы (lerp displayLimit → sliderLimit) ──────────────
    displayLimit += ((float)sliderLimit - displayLimit) * 0.18f;
    int dispLimit = (int)(displayLimit + 0.5f);

    // ── Peak hold ────────────────────────────────────────────────────────
    if (sliderLimit > (int)peakLed) {
      peakLed     = (float)sliderLimit;
      peakHeldAt  = now;
      lastDecayAt = now;
    } else if (now - peakHeldAt > 700) {
      // Убывание: 1 LED каждые 40 мс
      if (now - lastDecayAt >= 40) {
        lastDecayAt = now;
        peakLed    -= 0.5f;
        if (peakLed < displayLimit) peakLed = displayLimit;
      }
    }
    int peakIdx = constrain((int)peakLed, 0, LEDS_PER_SEG - 1);
    bool showPeak = (peakLed >= 1.0f && peakIdx >= dispLimit);

    // ── Первый сегмент (тусклое свечение при minPos) ─────────────────────
    const int kFirstSeg = 1023 / LEDS_PER_SEG; // ~85

    for (int i = 0; i < LEDS_PER_SEG; i++) {
      if (isMuted[id]) {
        leds[startIdx + i] = CRGB(beatsin8(8, 3, 18), 0, 0);
      } else if (i < dispLimit) {
        uint8_t frac = map(i, 0, LEDS_PER_SEG - 1, 0, 255);
        uint8_t hue  = lerp8by8(t.hue0, t.hue1, frac);
        uint8_t val  = (vuBar >= 0 && i >= vuBar) ? 70 : 255;
        leds[startIdx + i] = CHSV(hue, t.sat, val);
      } else if (showPeak && i == peakIdx) {
        // Пик: тот же оттенок, но чуть приглушён — выглядит как «хвостик»
        uint8_t frac = map(i, 0, LEDS_PER_SEG - 1, 0, 255);
        uint8_t hue  = lerp8by8(t.hue0, t.hue1, frac);
        leds[startIdx + i] = CHSV(hue, t.sat, 160);
      } else if (i == 0 && avgPos > 0 && dispLimit == 0) {
        // Первый диод тускло горит при minPos
        uint8_t hue = lerp8by8(t.hue0, t.hue1, 0);
        uint8_t val = (uint8_t)constrain(map(avgPos, 1, kFirstSeg, 15, 200), 15, 200);
        leds[startIdx] = CHSV(hue, t.sat, val);
      } else {
        leds[startIdx + i] = CRGB::Black;
      }
    }
  }
};

extern SliderControl sliders[NUM_SLIDERS];
