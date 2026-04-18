#include "web_server.h"
#include "globals.h"
#include "slider.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Update.h>
#include "ota_manager.h"

// ============================================================
// HTTP СЕРВЕР УПРАВЛЕНИЯ (STA режим)
// GET /            — HTML-дашборд
// GET /data        — JSON: данные каналов
// GET /set         — смена темы/яркости/VU/канала
// GET /wifi        — страница настроек WiFi
// GET /wifi/status — JSON: текущее подключение
// GET /scan        — запуск скана (loop()) + кешированный результат
// GET /scan/result — свежий результат скана
// POST /connect    — сохранить и перезагрузить
// GET/POST /update — OTA страница и загрузка прошивки
// ============================================================

void setupHttpServer() {

    // ---- /data ----
    httpServer.on("/data", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray ch = doc["ch"].to<JsonArray>();
        for (int i = 0; i < NUM_SLIDERS; i++) {
            JsonObject o = ch.add<JsonObject>();
            int rawAvg = sliders[i].total / globalSmoothing;
            o["name"]   = channelName[i];
            o["rawAvg"] = rawAvg;
            o["avgPos"] = sliders[i].avgPos;
            o["vol"]    = currentVol[i];
            o["leds"]   = map(sliders[i].avgPos, 0, 1023, 0, LEDS_PER_SEG);
            o["vu"]     = vuLevel[i];
            o["muted"]  = isMuted[i];
            o["theme"]  = channelTheme[i];
        }
        doc["theme"]      = currentTheme;
        doc["brightness"] = globalBrightness;
        doc["vuEnabled"]  = vuMeterEnabled;
        doc["version"]    = version;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ---- /set?theme=N &/or ?brightness=N &/or ?vu=0|1 &/or ?ch=N&theme=M ----
    // ?theme=N (без ?ch) — применить ко ВСЕМ каналам
    // ?ch=N&theme=M      — только канал N
    httpServer.on("/set", HTTP_GET, [](AsyncWebServerRequest *req) {
        bool save = false;
        if (req->hasParam("theme")) {
            int t = req->getParam("theme")->value().toInt();
            if (t >= 0 && t < NUM_THEMES) {
                if (req->hasParam("ch")) {
                    int ch = constrain(req->getParam("ch")->value().toInt(), 0, NUM_SLIDERS - 1);
                    channelTheme[ch] = t;
                } else {
                    currentTheme = t;
                    for (int i = 0; i < NUM_SLIDERS; i++) channelTheme[i] = t;
                }
                save = true;
            }
        }
        if (req->hasParam("brightness")) {
            int b = constrain(req->getParam("brightness")->value().toInt(), 5, 255);
            globalBrightness = b;
            FastLED.setBrightness(globalBrightness);
            save = true;
        }
        if (req->hasParam("vu")) {
            vuMeterEnabled = (req->getParam("vu")->value().toInt() != 0);
            save = true;
        }
        if (save) {
            EEPROM.begin(EEPROM_SIZE);
            EEPROM.write(9,  currentTheme);
            EEPROM.write(10, globalBrightness);
            EEPROM.write(11, vuMeterEnabled ? 1 : 0);
            for (int i = 0; i < NUM_SLIDERS; i++)
                EEPROM.write(12 + i, (uint8_t)channelTheme[i]);
            EEPROM.commit();
            EEPROM.end();
        }
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // ---- /wifi/status ----
    httpServer.on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["ssid"] = staSSID;
        doc["ip"]   = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ---- /scan — безопасный: скан выполняется в loop() ----
    httpServer.on("/scan", HTTP_GET, [](AsyncWebServerRequest *req) {
        g_scanRequest = true;
        req->send(200, "application/json", g_scanResult);
    });

    // ---- /scan/result ----
    httpServer.on("/scan/result", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", g_scanResult);
    });

    // ---- /connect ----
    httpServer.on("/connect", HTTP_POST, [](AsyncWebServerRequest *req) {
        String ssid = req->hasParam("ssid", true)
                        ? req->getParam("ssid", true)->value() : "";
        String pass = req->hasParam("pass", true)
                        ? req->getParam("pass", true)->value() : "";
        if (ssid.isEmpty()) {
            req->send(400, "application/json", "{\"ok\":false,\"msg\":\"SSID не указан\"}");
            return;
        }
        saveWiFiCredentials(ssid, pass);
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Сохранено! Перезагружаю...\"}");
        delay(600);
        ESP.restart();
    });

    // ---- /wifi — страница настроек WiFi (STA режим) ----
    httpServer.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *req) {
        static const char WIFI_PAGE[] PROGMEM = R"END(<!DOCTYPE html><html lang="ru">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deej32Led &mdash; Wi-Fi</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@400;600&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0f18;color:#e2e8f0;font-family:'Inter',sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#191c2a;border:1px solid #252840;border-radius:18px;padding:32px 36px;max-width:460px;width:100%;box-shadow:0 8px 40px #0008}
h1{font-size:1.25rem;color:#38bdf8;margin-bottom:4px}
.sub{font-size:.72rem;color:#475569;margin-bottom:20px}
.info{background:#13151f;border-radius:10px;padding:12px 16px;margin-bottom:22px}
.irow{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #1e2235}
.irow:last-child{border-bottom:none}
.il{font-size:.63rem;color:#64748b;text-transform:uppercase;letter-spacing:.08em}
.iv{font-family:'JetBrains Mono',monospace;font-size:.8rem;font-weight:600;color:#e2e8f0}
.iv.ok{color:#34d399}
.sep{border:none;border-top:1px solid #252840;margin:20px 0}
.lbl{font-size:.62rem;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:#64748b;margin-bottom:8px}
.nets{display:flex;flex-direction:column;gap:5px;max-height:220px;overflow-y:auto;margin-bottom:12px;padding-right:2px}
.net{display:flex;align-items:center;gap:10px;padding:9px 13px;background:#13151f;border:2px solid #252840;border-radius:9px;cursor:pointer;transition:border-color .15s,background .15s}
.net:hover{border-color:#0ea5e955;background:#17192a}
.net.sel{border-color:#38bdf8;background:#0e1e2a}
.ico{font-size:1rem;flex-shrink:0}
.nfo{flex:1;min-width:0}
.nssid{font-size:.83rem;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.nmeta{font-size:.62rem;color:#64748b;margin-top:2px}
.bs{display:flex;gap:2px;align-items:flex-end;flex-shrink:0}
.b{width:4px;border-radius:1px;background:#252840}.b.on{background:#38bdf8}
.b1{height:5px}.b2{height:9px}.b3{height:13px}.b4{height:17px}
.field{margin-bottom:13px}
.fl{font-size:.62rem;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:#64748b;margin-bottom:5px}
input[type=text],input[type=password]{width:100%;padding:9px 11px;background:#13151f;border:1.5px solid #252840;border-radius:8px;color:#e2e8f0;font-size:.84rem;outline:none;transition:border-color .15s;font-family:'Inter',sans-serif}
input:focus{border-color:#0ea5e9}
.rb{width:100%;padding:8px;background:none;border:1.5px solid #252840;border-radius:8px;color:#64748b;font-size:.72rem;cursor:pointer;margin-bottom:12px;transition:border-color .15s,color .15s;font-family:'Inter',sans-serif}
.rb:hover:not(:disabled){border-color:#0ea5e966;color:#38bdf8}
.rb:disabled{opacity:.5;cursor:default}
.go{width:100%;padding:11px;background:linear-gradient(135deg,#0ea5e9,#38bdf8);border:none;border-radius:9px;color:#fff;font-size:.88rem;font-weight:700;cursor:pointer;letter-spacing:.03em;font-family:'Inter',sans-serif;transition:opacity .2s}
.go:disabled{opacity:.4;cursor:default}
.msg{font-size:.73rem;text-align:center;margin-top:11px;min-height:16px;color:#64748b}
.ok-c{color:#34d399}.er{color:#f87171}
.sp{display:inline-block;width:11px;height:11px;border:2px solid #0ea5e933;border-top-color:#38bdf8;border-radius:50%;animation:sp .65s linear infinite;vertical-align:middle;margin-right:4px}
@keyframes sp{to{transform:rotate(360deg)}}
.back{display:block;text-align:center;margin-top:18px;font-size:.72rem;color:#475569;text-decoration:none}
.back:hover{color:#38bdf8}
</style></head>
<body><div class="card">
<h1>&#128246; Настройки Wi-Fi</h1>
<div class="sub">Deej32Led &mdash; управление сетью</div>
<div class="info" id="info"><div class="msg"><span class="sp"></span>Загрузка...</div></div>
<div class="sep"></div>
<div class="lbl">Сменить сеть</div>
<div class="nets" id="nets"><div class="msg" style="padding:12px 0">Нажмите «Сканировать» для поиска сетей</div></div>
<button class="rb" id="rb" onclick="scan()">&#8635;&nbsp;Сканировать сети</button>
<div class="field"><div class="fl">Название сети (SSID)</div>
<input type="text" id="ssid" placeholder="Выберите из списка или введите вручную" autocomplete="off"></div>
<div class="field"><div class="fl">Пароль</div>
<input type="password" id="pass" placeholder="Пароль Wi-Fi" autocomplete="current-password"></div>
<button class="go" id="go" onclick="doConn()">Подключиться</button>
<div class="msg" id="msg"></div>
<a class="back" href="/">&larr; Назад в панель управления</a>
</div>
<script>
let ns=[];
function bars(r){const l=r>-55?4:r>-65?3:r>-75?2:1;return[0,1,2,3].map(i=>`<div class="b b${i+1}${i<l?' on':''}"></div>`).join('');}
fetch('/wifi/status').then(r=>r.json()).then(s=>{
  document.getElementById('info').innerHTML=`
    <div class="irow"><span class="il">SSID</span><span class="iv ok">${s.ssid}</span></div>
    <div class="irow"><span class="il">IP-адрес</span><span class="iv">${s.ip}</span></div>
    <div class="irow"><span class="il">Сигнал RSSI</span><span class="iv">${s.rssi} dBm</span></div>`;
  document.getElementById('ssid').placeholder=s.ssid||'Выберите или введите сеть';
}).catch(()=>document.getElementById('info').innerHTML='<div class="msg er">Ошибка загрузки</div>');
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
    .then(r=>{msg.textContent=r.msg;msg.className='msg '+(r.ok?'ok-c':'er');})
    .catch(()=>{msg.textContent='Ошибка';msg.className='msg er';go.disabled=false;});
}
</script></body></html>)END";
        req->send(200, "text/html", WIFI_PAGE);
    });

    // ---- /update POST — HTTP OTA ----
    httpServer.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *req) {
            bool ok = !Update.hasError();
            AsyncWebServerResponse *resp = req->beginResponse(
                200, "application/json",
                ok ? "{\"ok\":true,\"msg\":\"Прошивка загружена! Перезагружаю...\"}" :
                     "{\"ok\":false,\"msg\":\"Ошибка OTA\"}");
            resp->addHeader("Connection", "close");
            req->send(resp);
            if (ok) { delay(500); ESP.restart(); }
        },
        [](AsyncWebServerRequest *req, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {
            if (!index) {
                Serial.printf("[HTTP-OTA] Start: %s  size hint: %u\n",
                              filename.c_str(), req->contentLength());
                FastLED.clear(); FastLED.show();
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
                    Update.printError(Serial);
            }
            if (Update.isRunning()) {
                Update.write(data, len);
                int pct = (int)(Update.progress() * 100 / Update.size());
                int lit = map(pct, 0, 100, 0, LEDS_PER_SEG);
                for (int i = 0; i < LEDS_PER_SEG; i++)
                    leds[i] = (i < lit) ? CRGB(0, 200, 80) : CRGB::Black;
                FastLED.show();
            }
            if (final) {
                if (Update.end(true))
                    Serial.printf("[HTTP-OTA] Success: %u bytes\n", index + len);
                else
                    Update.printError(Serial);
            }
        });

    // ---- /update/url POST — Обновление по ссылке (GitHub OTA) ----
    httpServer.on("/update/url", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (req->hasParam("url", true)) {
            String url = req->getParam("url", true)->value();
            req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Запущен процесс обновления с облака...\"}");
            delay(500);
            startCloudOTA(url);
        } else {
            req->send(400, "application/json", "{\"ok\":false,\"msg\":\"Не указан URL\"}");
        }
    });

    // ---- /update GET — OTA страница ----
    httpServer.on("/update", HTTP_GET, [](AsyncWebServerRequest *req) {
        static const char OTA_PAGE[] PROGMEM = R"END(<!DOCTYPE html><html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deej32Led OTA</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0f18;color:#e2e8f0;font-family:'Inter',sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}
.card{background:#191c2a;border:1px solid #252840;border-radius:18px;padding:36px 40px;max-width:420px;width:100%;box-shadow:0 8px 40px #0006}
h1{font-size:1.3rem;color:#a78bfa;margin-bottom:6px}
.sub{font-size:.72rem;color:#475569;margin-bottom:28px}
.drop{border:2px dashed #7c3aed55;border-radius:12px;padding:36px 20px;text-align:center;cursor:pointer;transition:all .2s;background:#13151f}
.drop:hover,.drop.over{border-color:#a78bfa;background:#1e1b2e}
.drop svg{margin-bottom:10px;opacity:.5}
.drop p{font-size:.8rem;color:#64748b}
.drop strong{display:block;font-size:.95rem;color:#94a3b8;margin-bottom:6px}
input[type=file]{display:none}
.btn{width:100%;margin-top:18px;padding:12px;background:linear-gradient(135deg,#7c3aed,#a78bfa);border:none;border-radius:10px;color:#fff;font-size:.9rem;font-weight:600;cursor:pointer;transition:opacity .2s;letter-spacing:.03em}
.btn:disabled{opacity:.4;cursor:default}
.btn-gh{background:linear-gradient(135deg,#1f2937,#111827);box-shadow:0 0 12px #0004;margin-top:10px}
.prog{margin-top:18px;display:none}
.bar-wrap{background:#0d0f18;border-radius:6px;height:8px;overflow:hidden;margin-top:6px}
.bar{height:100%;width:0;background:linear-gradient(90deg,#7c3aed,#a78bfa);border-radius:6px;transition:width .2s}
.msg{font-size:.75rem;text-align:center;margin-top:10px;min-height:18px}
.ok{color:#34d399}.er{color:#f87171}
.back{display:block;text-align:center;margin-top:22px;font-size:.72rem;color:#475569;text-decoration:none}
.back:hover{color:#a78bfa}
</style>
</head>
<body>
<div class="card">
  <h1>&#128640; OTA Update</h1>
  <div class="sub">Deej32Led &mdash; загрузка прошивки по воздуху</div>
  
  <div class="drop" id="drop" onclick="document.getElementById('fw').click()">
    <svg width="40" height="40" fill="none" viewBox="0 0 24 24" stroke="#a78bfa" stroke-width="1.5">
      <path stroke-linecap="round" stroke-linejoin="round" d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5m-13.5-9L12 3m0 0l4.5 4.5M12 3v13.5"/>
    </svg>
    <strong id="fname">Выберите .bin файл</strong>
    <p>или перетащите сюда (Ручное обновление)</p>
    <input type="file" id="fw" accept=".bin">
  </div>
  <button class="btn" id="btn" disabled onclick="doFlash()">Загрузить прошивку</button>
  
  <hr style="border:none;border-top:1px solid #252840;margin:20px 0">
  
  <button class="btn btn-gh" onclick="checkGithub()">&#128269; Проверить обновления на GitHub</button>
  
  <div class="prog" id="prog">
    <div class="bar-wrap"><div class="bar" id="bar"></div></div>
  </div>
  <div class="msg" id="msg"></div>

  <a class="back" href="/">&larr; Назад в панель управления</a>
</div>
<script>
const fw=document.getElementById('fw'),drop=document.getElementById('drop');
fw.onchange=()=>{if(fw.files[0]){document.getElementById('fname').textContent=fw.files[0].name;document.getElementById('btn').disabled=false;}};
drop.ondragover=e=>{e.preventDefault();drop.classList.add('over');};
drop.ondragleave=()=>drop.classList.remove('over');
drop.ondrop=e=>{e.preventDefault();drop.classList.remove('over');fw.files=e.dataTransfer.files;fw.onchange();};
function doFlash(){
  const file=fw.files[0]; if(!file) return;
  const fd=new FormData(); fd.append('firmware',file,file.name);
  const btn=document.getElementById('btn'),prog=document.getElementById('prog'),
        bar=document.getElementById('bar'),msg=document.getElementById('msg');
  btn.disabled=true; prog.style.display='block'; msg.textContent='Загрузка...';
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/update');
  xhr.upload.onprogress=e=>{if(e.lengthComputable){const p=e.loaded*100/e.total;bar.style.width=p+'%';msg.textContent=Math.round(p)+'%';}};
  xhr.onload=()=>{try{const r=JSON.parse(xhr.responseText);msg.textContent=r.msg;msg.className='msg '+(r.ok?'ok':'er');}catch{msg.textContent='Готово';}};
  xhr.onerror=()=>{msg.textContent='Ошибка соединения';msg.className='msg er';};
  xhr.send(fd);
}

function checkGithub(){
  const msg=document.getElementById('msg');
  msg.innerHTML='<span style="display:inline-block;animation:sp .65s linear infinite;">&#8987;</span> Поиск обновлений...'; 
  msg.className='msg';
  // Если у вас свой форк — замените DarkAssassinUA/Deej32Led на свои данные!
  Promise.all([
    fetch('/data').then(r=>r.json()),
    fetch('https://api.github.com/repos/DarkAssassinUA/Deej32LED/releases/latest').then(r=>r.json())
  ]).then(([d, gh]) => {
    let cur = (d.version||'0.0').replace('v','');
    let fresh = (gh.tag_name||'0.0').replace('v','');
    if (fresh !== cur && gh.assets) {
        let binAsset = gh.assets.find(a => a.name.endsWith('.bin'));
        if(binAsset) {
            msg.innerHTML = `Доступна версия <b>v${fresh}</b>! <br><button onclick="startCloud('${binAsset.browser_download_url}')" style="margin-top:10px;padding:8px 16px;border-radius:6px;border:none;background:linear-gradient(135deg,#10b981,#34d399);color:#000;font-weight:bold;cursor:pointer">Скачать и Установить \u2193</button>`;
        } else {
            msg.textContent = `Доступна версия v${fresh}, но скомпилированный .bin файл не прикреплен к релизу.`;
        }
    } else {
        msg.textContent = 'У вас установлена самая актуальная версия \u2705';
        msg.className = 'msg ok';
    }
  }).catch(e=>{
    console.error(e);
    msg.textContent='Ошибка проверки GitHub API =(';
    msg.className='msg er';
  });
}

function startCloud(url){
  const msg=document.getElementById('msg');
  msg.innerHTML='Отправка команды контроллеру...'; 
  const fd=new FormData(); fd.append('url', url);
  fetch('/update/url', {method:'POST', body:fd}).then(r=>r.json()).then(r=>{
    if(r.ok) {
       msg.innerHTML = '<b style="color:#f59e0b">Установка запущена!</b><br>Подождите примерно 30-40 секунд.<br>Следите за <b>оранжевой полосой прогресса</b> на LED-ленте!';
    } else {
       msg.textContent = 'Ошибка: ' + r.msg; msg.className='msg er';
    }
  }).catch(()=>{msg.textContent='Потеряна связь с устройством...';});
}

</script>
</body></html>)END";
        req->send(200, "text/html", OTA_PAGE);
    });

    // ---- / — Главный дашборд ----
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        static const char PAGE[] PROGMEM = R"END(<!DOCTYPE html><html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deej32Led</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@400;600&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0f18;color:#e2e8f0;font-family:'Inter',sans-serif;min-height:100vh}
.topbar{background:#13151f;border-bottom:1px solid #252840;padding:14px 20px;position:sticky;top:0;z-index:10}
.ti{max-width:1100px;margin:0 auto}
h1{font-size:1.2rem;font-weight:700;color:#a78bfa;margin-bottom:10px;display:flex;align-items:center;gap:8px}
h1 .sub{font-size:.65rem;font-weight:400;color:#475569;letter-spacing:.1em;text-transform:uppercase}
.ctrls{display:flex;flex-wrap:wrap;gap:14px;align-items:flex-end}
.cg{display:flex;flex-direction:column;gap:5px}
.cl{font-size:.63rem;color:#64748b;text-transform:uppercase;letter-spacing:.1em;font-weight:600}
.ths{display:flex;gap:5px;flex-wrap:wrap}
.tb{border:2px solid #252840;border-radius:9px;padding:5px 9px;cursor:pointer;background:none;font-size:.68rem;font-weight:600;color:#94a3b8;transition:all .18s;display:flex;align-items:center;gap:6px}
.tb:hover{border-color:#7c3aed99;color:#e2e8f0;transform:translateY(-1px)}
.tb.on{border-color:#a78bfa;color:#fff;background:#7c3aed28;box-shadow:0 0 14px #7c3aed33}
.sw{width:34px;height:9px;border-radius:4px;flex-shrink:0}
.bwc{display:flex;align-items:center;gap:8px}
input[type=range]{-webkit-appearance:none;width:130px;height:5px;border-radius:3px;background:#252840;outline:none;cursor:pointer}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;border-radius:50%;background:#a78bfa;cursor:pointer;transition:transform .15s}
input[type=range]::-webkit-slider-thumb:hover{transform:scale(1.25)}
.bvl{font-family:'JetBrains Mono',monospace;font-size:.78rem;color:#a78bfa;min-width:26px;text-align:right}
.top-actions{display:flex;gap:8px;align-items:center;margin-left:auto}
.action-btn{display:inline-flex;align-items:center;gap:6px;padding:7px 14px;border-radius:9px;color:#fff;font-size:.72rem;font-weight:600;text-decoration:none;letter-spacing:.03em;transition:opacity .2s,transform .15s;box-shadow:0 0 12px #0004}
.action-btn:hover{opacity:.85;transform:translateY(-1px)}
.btn-wifi{background:linear-gradient(135deg,#0ea5e9,#38bdf8);box-shadow:0 0 14px #0ea5e944}
.btn-ota{background:linear-gradient(135deg,#7c3aed,#a78bfa);box-shadow:0 0 14px #7c3aed44}
.btn-vu{padding:7px 14px;border-radius:9px;font-size:.72rem;font-weight:600;border:2px solid #252840;background:none;color:#64748b;cursor:pointer;transition:all .2s;letter-spacing:.03em}
.btn-vu.on{border-color:#06b6d4;color:#22d3ee;background:#06b6d412;box-shadow:0 0 12px #06b6d430}
.btn-vu:hover{border-color:#06b6d488;color:#22d3ee}
.wrap{padding:18px 20px;max-width:1100px;margin:0 auto}
.sub2{font-size:.7rem;color:#475569;margin-bottom:14px;display:flex;align-items:center;gap:5px}
.dot{display:inline-block;width:6px;height:6px;border-radius:50%;background:#10b981;animation:bl 1.1s infinite;flex-shrink:0}
@keyframes bl{0%,100%{opacity:1}50%{opacity:.15}}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(255px,1fr));gap:12px}
.card{background:#191c2a;border:1px solid #252840;border-radius:13px;padding:14px;transition:border-color .2s}
.card:hover{border-color:#7c3aed44}
.ct{font-size:.64rem;font-weight:600;text-transform:uppercase;letter-spacing:.12em;color:#64748b;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center}
.badge{background:#7c3aed22;color:#a78bfa;border:1px solid #7c3aed44;border-radius:5px;padding:1px 6px;font-size:.59rem}
.muted .badge{background:#dc262622;color:#f87171;border-color:#dc262644}
.row{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;gap:8px}
.lbl{font-size:.67rem;color:#64748b;white-space:nowrap}
.val{font-family:'JetBrains Mono',monospace;font-size:.82rem;font-weight:600;color:#e2e8f0;text-align:right}
.bw{width:100%;background:#0d0f18;border-radius:5px;overflow:hidden;height:6px;margin-top:2px}
.b{height:100%;border-radius:5px;transition:width .12s ease}
.br{background:linear-gradient(90deg,#3b82f6,#60a5fa)}
.ba{background:linear-gradient(90deg,#8b5cf6,#a78bfa)}
.bv{background:linear-gradient(90deg,#10b981,#34d399)}
.bl{background:linear-gradient(90deg,#f59e0b,#fbbf24)}
.bu{background:linear-gradient(90deg,#06b6d4,#22d3ee)}
.muted .bv{background:#dc2626}
.sep{border:none;border-top:1px solid #252840;margin:6px 0}
.lv{display:flex;gap:3px;margin-top:6px;flex-wrap:wrap}
.led{width:12px;height:12px;border-radius:3px;background:#111420;transition:background .1s}
.led.on{background:var(--lc)}
.cths{display:flex;gap:3px;flex-wrap:wrap;margin-top:7px}
.cth{width:22px;height:6px;border-radius:2px;cursor:pointer;border:1.5px solid transparent;transition:all .15s;flex-shrink:0}
.cth.on{border-color:#e2e8f0;transform:scaleY(1.6)}
.cth:hover{transform:scaleY(1.4);opacity:.85}
footer{font-size:.65rem;text-align:right;color:#1e293b;padding:10px 20px}
</style>
</head>
<body>
<div class="topbar"><div class="ti">
  <h1>Deej32Led <span class="sub">control panel</span> <span id="ver" style="font-size:.55rem;color:#334155;font-weight:400;letter-spacing:.06em"></span></h1>
  <div class="ctrls">
    <div class="cg">
      <div class="cl">Тема подсветки LED</div>
      <div class="ths" id="th">
        <button class="tb" data-t="0"><span class="sw" style="background:linear-gradient(90deg,#22c55e,#eab308,#ef4444)"></span>VU Classic</button>
        <button class="tb" data-t="1"><span class="sw" style="background:linear-gradient(90deg,#22d3ee,#3b82f6,#818cf8)"></span>Aurora</button>
        <button class="tb" data-t="2"><span class="sw" style="background:linear-gradient(90deg,#dc2626,#f97316,#eab308)"></span>Ember</button>
        <button class="tb" data-t="3"><span class="sw" style="background:linear-gradient(90deg,#9333ea,#ec4899,#fbcfe8)"></span>Synthwave</button>
        <button class="tb" data-t="4"><span class="sw" style="background:linear-gradient(90deg,#1e40af,#06b6d4)"></span>Ocean</button>
        <button class="tb" data-t="5"><span class="sw" style="background:linear-gradient(90deg,#14532d,#4ade80)"></span>Forest</button>
        <button class="tb" data-t="6"><span class="sw" style="background:linear-gradient(90deg,#fb923c,#e879f9)"></span>Sunset</button>
        <button class="tb" data-t="7"><span class="sw" style="background:linear-gradient(90deg,#be123c,#f9a8d4)"></span>Cherry</button>
        <button class="tb" data-t="8"><span class="sw" style="background:linear-gradient(90deg,#6ee7b7,#67e8f9)"></span>Mint</button>
        <button class="tb" data-t="9"><span class="sw" style="background:linear-gradient(90deg,#bae6fd,#f0f9ff)"></span>Ice</button>
        <button class="tb" data-t="10"><span class="sw" style="background:linear-gradient(90deg,#1e3a8a,#7c3aed,#db2777)"></span>Galaxy</button>
        <button class="tb" data-t="11"><span class="sw" style="background:linear-gradient(90deg,#84cc16,#eab308)"></span>Toxic</button>
      </div>
    </div>
    <div class="cg">
      <div class="cl">Яркость</div>
      <div class="bwc">
        <input type="range" id="br" min="5" max="255" value="60">
        <span class="bvl" id="bvlbl">60</span>
      </div>
    </div>
    <div class="top-actions">
      <button class="btn-vu" id="vuBtn" onclick="toggleVU()">&#x1F50A; VU meter</button>
      <a href="/wifi" class="action-btn btn-wifi">&#128246; Wi-Fi</a>
      <a href="/update" class="action-btn btn-ota">&#128640; OTA Update</a>
    </div>
  </div>
</div></div>
<div class="wrap">
  <div class="sub2"><span class="dot"></span><span id="ts">подключение...</span>
    &nbsp;&bull;&nbsp; 100 мс &nbsp;&bull;&nbsp; ADC 10-бит &nbsp;&bull;&nbsp; map(rawAvg,8,1015)</div>
  <div class="grid" id="grid"></div>
</div>
<footer>Deej32Led &mdash; calibration &amp; control</footer>
<script>
const CC=['#3b82f6','#8b5cf6','#10b981','#f59e0b','#06b6d4'];
const TG=['#22c55e,#eab308,#ef4444','#22d3ee,#3b82f6,#818cf8','#dc2626,#f97316,#eab308',
          '#9333ea,#ec4899,#fbcfe8','#1e40af,#06b6d4','#14532d,#4ade80','#fb923c,#e879f9',
          '#be123c,#f9a8d4','#6ee7b7,#67e8f9','#bae6fd,#f0f9ff','#1e3a8a,#7c3aed,#db2777','#84cc16,#eab308'];
let cT=-1,cB=-1,cVU=null,bTmr=null;
document.querySelectorAll('.tb').forEach(b=>{
  b.onclick=async()=>{const t=+b.dataset.t;await fetch('/set?theme='+t);markT(t);};
});
function markT(t){
  document.querySelectorAll('.tb').forEach(b=>b.classList.toggle('on',+b.dataset.t===t));
  cT=t;
}
function markVU(v){
  cVU=v;
  const btn=document.getElementById('vuBtn');
  btn.classList.toggle('on',v);
  btn.textContent=(v?'\uD83D\uDD0A':'\uD83D\uDD07')+' VU meter';
}
async function toggleVU(){
  const next=!cVU;
  await fetch('/set?vu='+(next?1:0));
  markVU(next);
}
async function setChTheme(ch,t){
  await fetch(`/set?ch=${ch}&theme=${t}`);
}
const bR=document.getElementById('br'),bL=document.getElementById('bvlbl');
bR.oninput=()=>{bL.textContent=bR.value;clearTimeout(bTmr);bTmr=setTimeout(()=>fetch('/set?brightness='+bR.value),200);};
async function refresh(){
  try{
    const d=await(await fetch('/data')).json();
    if(d.theme!==cT)markT(d.theme);
    if(d.brightness!==cB&&document.activeElement!==bR){cB=d.brightness;bR.value=d.brightness;bL.textContent=d.brightness;}
    if(d.vuEnabled!==undefined&&d.vuEnabled!==cVU)markVU(d.vuEnabled);
    if(d.version){const v=document.getElementById('ver');if(v&&v.textContent!=='v'+d.version)v.textContent='v'+d.version;}
    const g=document.getElementById('grid');
    d.ch.forEach((c,i)=>{
      let el=document.getElementById('c'+i);
      if(!el){el=document.createElement('div');el.id='c'+i;g.appendChild(el);}
      el.className='card'+(c.muted?' muted':'');
      el.style.setProperty('--lc',CC[i%CC.length]);
      const lh=Array.from({length:12},(_,j)=>`<div class="led${j<c.leds?' on':''}" title="LED ${j+1}"></div>`).join('');
      const vuRow=cVU?`<hr class="sep"><div class="row"><span class="lbl">VU</span><span class="val">${c.vu}</span></div><div class="bw"><div class="b bu" style="width:${c.vu}%"></div></div>`:'';
      const chT=c.theme!==undefined?c.theme:-1;
      const cthRow='<div class="cths">'+TG.map((g,t)=>`<div class="cth${t===chT?' on':''}" style="background:linear-gradient(90deg,${g})" onclick="setChTheme(${i},${t})" title="\u0422\u0435\u043c\u0430 ${t}"></div>`).join('')+'</div>';
      el.innerHTML=`
        <div class="ct"><span>CH${i+1} &mdash; ${c.name}</span><span class="badge">${c.muted?'MUTE':c.vol+'%'}</span></div>
        <div class="row"><span class="lbl">Сырой ADC</span><span class="val">${c.rawAvg}</span></div>
        <div class="bw"><div class="b br" style="width:${(c.rawAvg/1023*100).toFixed(1)}%"></div></div>
        <hr class="sep">
        <div class="row"><span class="lbl">avgPos</span><span class="val">${c.avgPos}</span></div>
        <div class="bw"><div class="b ba" style="width:${(c.avgPos/1023*100).toFixed(1)}%"></div></div>
        <hr class="sep">
        <div class="row"><span class="lbl">Громкость vol%</span><span class="val">${c.vol}</span></div>
        <div class="bw"><div class="b bv" style="width:${c.vol}%"></div></div>
        <hr class="sep">
        <div class="row"><span class="lbl">LED (${c.leds}/12)</span><span class="val">${c.leds}</span></div>
        <div class="bw"><div class="b bl" style="width:${(c.leds/12*100).toFixed(1)}%"></div></div>
        <div class="lv">${lh}</div>${vuRow}
        <hr class="sep">
        <div class="row"><span class="lbl" style="font-size:.6rem">Тема канала</span></div>
        ${cthRow}`;
    });
    document.getElementById('ts').textContent=new Date().toLocaleTimeString();
  }catch(e){document.getElementById('ts').textContent='Ошибка: '+e.message;}
}
refresh();setInterval(refresh,300);
</script>
</body></html>)END";
        req->send(200, "text/html", PAGE);
    });

    httpServer.begin();
    Serial.printf("[HTTP] Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
}
