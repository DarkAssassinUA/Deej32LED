#include "wifi_manager.h"
#include "globals.h"
#include <ArduinoJson.h>

// ============================================================
// WiFi CREDENTIALS
// ============================================================
void loadWiFiCredentials() {
    wifiPrefs.begin("deej", true);
    staSSID = wifiPrefs.getString("ssid", "");
    staPass = wifiPrefs.getString("pass", "");
    wifiPrefs.end();
    Serial.printf("[WiFi] Credentials: SSID='%s'\n", staSSID.c_str());
}

void saveWiFiCredentials(const String &ssid, const String &pass) {
    wifiPrefs.begin("deej", false);
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("pass", pass);
    wifiPrefs.end();
    Serial.printf("[WiFi] Saved: SSID='%s'\n", ssid.c_str());
}

// ============================================================
// WiFi SCAN
// Вызывается ТОЛЬКО из loop() — никогда из HTTP-callbacks!
// ============================================================
String buildScanJson() {
    Serial.println("[WiFi] Scanning...");
    int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
    if (n == WIFI_SCAN_FAILED || n < 0) {
        Serial.printf("[WiFi] Scan failed (code=%d)\n", n);
        WiFi.scanDelete();
        return "[]";
    }
    Serial.printf("[WiFi] Found %d networks\n", n);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"]   = WiFi.SSID(i);
        o["rssi"]   = WiFi.RSSI(i);
        o["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    String out;
    serializeJson(doc, out);
    return out;
}

// ============================================================
// AP CAPTIVE PORTAL
// ============================================================
void setupApServer() {
    const byte DNS_PORT = 53;
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // /scan — запускает скан через loop() и отдаёт кешированный результат
    httpServer.on("/scan", HTTP_GET, [](AsyncWebServerRequest *req) {
        g_scanRequest = true;
        req->send(200, "application/json", g_scanResult);
    });

    // /scan/result — отдаёт актуальный результат после опроса
    httpServer.on("/scan/result", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", g_scanResult);
    });

    // /connect — сохранить и перезагрузить
    httpServer.on("/connect", HTTP_POST, [](AsyncWebServerRequest *req) {
        String ssid = req->hasParam("ssid", true)
                        ? req->getParam("ssid", true)->value() : "";
        String pass = req->hasParam("pass", true)
                        ? req->getParam("pass", true)->value() : "";
        if (ssid.isEmpty()) {
            req->send(400, "application/json",
                      "{\"ok\":false,\"msg\":\"SSID не указан\"}");
            return;
        }
        saveWiFiCredentials(ssid, pass);
        req->send(200, "application/json",
                  "{\"ok\":true,\"msg\":\"Сохранено! Перезагружаю...\"}");
        delay(600);
        ESP.restart();
    });

    // / — страница настройки WiFi (Captive Portal)
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        static const char AP_PAGE[] PROGMEM = R"END(<!DOCTYPE html><html lang="ru">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deej32Led &mdash; Настройка Wi-Fi</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0f18;color:#e2e8f0;font-family:'Inter',sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#191c2a;border:1px solid #252840;border-radius:18px;padding:32px 36px;max-width:460px;width:100%;box-shadow:0 8px 40px #0008}
h1{font-size:1.25rem;color:#a78bfa;margin-bottom:4px}
.sub{font-size:.72rem;color:#475569;margin-bottom:18px}
.badge{display:inline-flex;align-items:center;gap:5px;padding:5px 12px;background:#f59e0b1a;border:1px solid #f59e0b44;border-radius:7px;color:#fbbf24;font-size:.64rem;font-weight:700;margin-bottom:22px;letter-spacing:.07em}
.lbl{font-size:.62rem;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:#64748b;margin-bottom:8px}
.nets{display:flex;flex-direction:column;gap:5px;max-height:230px;overflow-y:auto;margin-bottom:12px;padding-right:2px}
.net{display:flex;align-items:center;gap:10px;padding:9px 13px;background:#13151f;border:2px solid #252840;border-radius:9px;cursor:pointer;transition:border-color .15s,background .15s}
.net:hover{border-color:#7c3aed55;background:#17192a}
.net.sel{border-color:#a78bfa;background:#1a1730}
.ico{font-size:1rem;flex-shrink:0}
.nfo{flex:1;min-width:0}
.nssid{font-size:.83rem;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.nmeta{font-size:.62rem;color:#64748b;margin-top:2px}
.bs{display:flex;gap:2px;align-items:flex-end;flex-shrink:0}
.b{width:4px;border-radius:1px;background:#252840}.b.on{background:#a78bfa}
.b1{height:5px}.b2{height:9px}.b3{height:13px}.b4{height:17px}
.field{margin-bottom:13px}
.fl{font-size:.62rem;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:#64748b;margin-bottom:5px}
input[type=text],input[type=password]{width:100%;padding:9px 11px;background:#13151f;border:1.5px solid #252840;border-radius:8px;color:#e2e8f0;font-size:.84rem;outline:none;transition:border-color .15s;font-family:'Inter',sans-serif}
input:focus{border-color:#7c3aed}
.rb{width:100%;padding:8px;background:none;border:1.5px solid #252840;border-radius:8px;color:#64748b;font-size:.72rem;cursor:pointer;margin-bottom:12px;transition:border-color .15s,color .15s;font-family:'Inter',sans-serif}
.rb:hover:not(:disabled){border-color:#7c3aed66;color:#a78bfa}
.rb:disabled{opacity:.5;cursor:default}
.go{width:100%;padding:11px;background:linear-gradient(135deg,#7c3aed,#a78bfa);border:none;border-radius:9px;color:#fff;font-size:.88rem;font-weight:700;cursor:pointer;letter-spacing:.03em;font-family:'Inter',sans-serif;transition:opacity .2s}
.go:disabled{opacity:.4;cursor:default}
.msg{font-size:.73rem;text-align:center;margin-top:11px;min-height:16px;color:#64748b}
.ok{color:#34d399}.er{color:#f87171}
.sp{display:inline-block;width:11px;height:11px;border:2px solid #7c3aed33;border-top-color:#a78bfa;border-radius:50%;animation:sp .65s linear infinite;vertical-align:middle;margin-right:4px}
@keyframes sp{to{transform:rotate(360deg)}}
</style></head>
<body><div class="card">
<h1>&#127758; Настройка Wi-Fi</h1>
<div class="sub">Deej32Led &mdash; первоначальная настройка сети</div>
<div class="badge">&#9889; Точка доступа: Deej32Led-Setup &nbsp;&bull;&nbsp; 192.168.4.1</div>
<div class="lbl">Доступные сети</div>
<div class="nets" id="nets"><div class="msg"><span class="sp"></span>Сканирование...</div></div>
<button class="rb" id="rb" onclick="scan()">&#8635;&nbsp;Обновить список</button>
<div class="field"><div class="fl">Название сети (SSID)</div>
<input type="text" id="ssid" placeholder="Выберите из списка или введите вручную" autocomplete="off"></div>
<div class="field"><div class="fl">Пароль</div>
<input type="password" id="pass" placeholder="Пароль Wi-Fi" autocomplete="current-password"></div>
<button class="go" id="go" onclick="doConn()">Подключиться</button>
<div class="msg" id="msg"></div>
</div>
<script>
let ns=[];
function bars(r){const l=r>-55?4:r>-65?3:r>-75?2:1;return[0,1,2,3].map(i=>`<div class="b b${i+1}${i<l?' on':''}"></div>`).join('');}
function scan(){
  const el=document.getElementById('nets'),rb=document.getElementById('rb');
  el.innerHTML='<div class="msg"><span class="sp"></span>Сканирование...</div>';rb.disabled=true;
  fetch('/scan').then(r=>r.json()).then(a=>{if(a.length)renderNets(a);});
  setTimeout(()=>fetch('/scan/result').then(r=>r.json()).then(a=>renderNets(a))
    .catch(()=>el.innerHTML='<div class="msg er">Ошибка сканирования</div>').finally(()=>rb.disabled=false),3500);
}
function renderNets(a){
  const el=document.getElementById('nets'),rb=document.getElementById('rb');
  rb.disabled=false;
  ns=a.sort((a,b)=>b.rssi-a.rssi);
  if(!ns.length){el.innerHTML='<div class="msg">Сети не найдены</div>';return;}
  el.innerHTML=ns.map((n,i)=>`<div class="net" onclick="pick(${i})">
    <div class="ico">${n.secure?'&#128274;':'&#128275;'}</div>
    <div class="nfo"><div class="nssid">${n.ssid}</div><div class="nmeta">${n.rssi} dBm &bull; ${n.secure?'Защищённая':'Открытая'}</div></div>
    <div class="bs">${bars(n.rssi)}</div></div>`).join('');
}
function pick(i){
  document.querySelectorAll('.net').forEach((n,j)=>n.classList.toggle('sel',j===i));
  document.getElementById('ssid').value=ns[i].ssid;
  document.getElementById('pass').focus();
}
function doConn(){
  const ssid=document.getElementById('ssid').value.trim(),pass=document.getElementById('pass').value;
  const msg=document.getElementById('msg'),go=document.getElementById('go');
  if(!ssid){msg.textContent='Укажите сеть';msg.className='msg er';return;}
  go.disabled=true;msg.innerHTML='<span class="sp"></span>Подключение...';msg.className='msg';
  const fd=new FormData();fd.append('ssid',ssid);fd.append('pass',pass);
  fetch('/connect',{method:'POST',body:fd}).then(r=>r.json())
    .then(r=>{msg.textContent=r.msg;msg.className='msg '+(r.ok?'ok':'er');})
    .catch(()=>{msg.textContent='Ошибка';msg.className='msg er';go.disabled=false;});
}
scan();
</script></body></html>)END";
        req->send_P(200, "text/html", AP_PAGE);
    });

    // Редирект всего остального на портал
    httpServer.onNotFound([](AsyncWebServerRequest *req) {
        req->redirect("http://192.168.4.1/");
    });

    httpServer.begin();
    Serial.printf("[AP] Captive portal: http://192.168.4.1/ (SSID: %s)\n", AP_SSID);
}
