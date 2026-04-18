#include "ws_handler.h"
#include "globals.h"
#include <ArduinoJson.h>

// ============================================================
// WEBSOCKET — отправка состояния хосту
// ============================================================
void sendWsState(AsyncWebSocketClient *client) {
    JsonDocument doc;
    doc["type"] = "update";
    JsonArray vol  = doc["vol"].to<JsonArray>();
    JsonArray mute = doc["mute"].to<JsonArray>();
    JsonArray bak  = doc["bak"].to<JsonArray>();
    JsonArray con  = doc["con"].to<JsonArray>();
    for (int i = 0; i < NUM_SLIDERS; i++) {
        vol.add(currentVol[i]);
        mute.add(isMuted[i]);
        bak.add(false);
        con.add(false);
    }
    String out;
    serializeJson(doc, out);
    if (client) client->text(out);
    else        ws.textAll(out);
}

// ============================================================
// WEBSOCKET — обработка входящих сообщений
// ============================================================
void handleWsMessage(uint8_t *data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) return;

    const char *type = doc["type"] | "";

    if (strcmp(type, "config") == 0) {
        JsonArray names = doc["names"].as<JsonArray>();
        for (int i = 0; i < NUM_SLIDERS && i < (int)names.size(); i++)
            strlcpy(channelName[i], names[i] | channelName[i], sizeof(channelName[i]));
        Serial.printf("[WS] config received\n");

    } else if (strcmp(type, "state") == 0) {
        for (int i = 0; i < NUM_SLIDERS; i++) {
            pendingVol[i]  = currentVol[i];
            pendingMute[i] = isMuted[i];
        }
        if (doc["vol"].is<JsonArray>()) {
            JsonArray vols = doc["vol"].as<JsonArray>();
            for (int i = 0; i < NUM_SLIDERS && i < (int)vols.size(); i++)
                pendingVol[i] = constrain((int)vols[i], 0, 100);
        }
        if (doc["mute"].is<JsonArray>()) {
            JsonArray mutes = doc["mute"].as<JsonArray>();
            for (int i = 0; i < NUM_SLIDERS && i < (int)mutes.size(); i++)
                pendingMute[i] = (bool)mutes[i];
        }
        pendingStateApply = true;

    } else if (strcmp(type, "vu") == 0) {
        if (doc["levels"].is<JsonArray>()) {
            JsonArray levels = doc["levels"].as<JsonArray>();
            for (int i = 0; i < NUM_SLIDERS && i < (int)levels.size(); i++)
                vuLevel[i] = constrain((int)(levels[i].as<float>() * 100.0f), 0, 100);
        }
    }
}

// ============================================================
// WEBSOCKET — событийный обработчик
// ============================================================
void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
               AwsEventType type, void *, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] client connected  id=%u  ip=%s\n",
                      client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] client disconnected  id=%u\n", client->id());
    } else if (type == WS_EVT_DATA) {
        handleWsMessage(data, len);
    } else if (type == WS_EVT_ERROR) {
        Serial.printf("[WS] error  id=%u\n", client->id());
    }
}
