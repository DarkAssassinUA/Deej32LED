#pragma once

// ============================================================
// КОНФИГУРАЦИЯ ПРОЕКТА
// ============================================================
#define TOTAL_LEDS   60
#define LEDS_PER_SEG 12
#define NUM_SLIDERS  5
#define LED_DATA_PIN 13
#define EEPROM_SIZE  512
#define NUM_THEMES   12
#define WS_PORT_NUM  8765

// EEPROM layout:
//  8  — globalSmoothing
//  9  — currentTheme
// 10  — globalBrightness
// 11  — vuMeterEnabled
// 12..16 — channelTheme[0..4]
