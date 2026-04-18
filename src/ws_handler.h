#pragma once
#include <ESPAsyncWebServer.h>

void sendWsState(AsyncWebSocketClient *client = nullptr);
void handleWsMessage(uint8_t *data, size_t len);
void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
               AwsEventType type, void *, uint8_t *data, size_t len);
