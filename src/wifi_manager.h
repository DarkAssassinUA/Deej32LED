#pragma once
#include <Arduino.h>

void loadWiFiCredentials();
void saveWiFiCredentials(const String &ssid, const String &pass);
String buildScanJson();
void setupApServer();
