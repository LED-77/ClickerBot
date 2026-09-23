#include "WebServer.h"
#include "../Config.h"
#include "../hal/SettingsStore.h"
#include "../skins/SkinRegistry.h"
#include <WebServer.h>
#include <WiFi.h>

namespace {
  WebServer server(80);
  bool running = false;

  // JSON-экранирование строки
  // JSON string escaping
  String jsonEscape(const char* s) {
    String out;
    for (const char* p = s; *p; p++) {
      char c = *p;
      if (c == '"' || c == '\\') { out += '\\'; out += c; }
      else out += c;
    }
    return out;
  }

  // --- HTML страница ---
  // --- HTML page ---
  const char PAGE[] PROGMEM = R"raw(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ClickerBot</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#1a1a2e;color:#eee;padding:16px;max-width:400px;margin:0 auto}
h1{text-align:center;color:#e94560;margin-bottom:20px;font-size:1.6em}
h2{font-size:1em;margin:16px 0 8px;color:#aaa}
.card{background:#16213e;border-radius:12px;padding:14px;margin-bottom:12px}
select{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#0f3460;color:#eee;font-size:1em;margin:4px 0}
.btn{display:block;width:100%;padding:12px;border:none;border-radius:8px;font-size:1em;cursor:pointer}
.btn-primary{background:#e94560;color:#fff}
.btn-secondary{background:#0f3460;color:#eee}
.btn:active{opacity:.8}
.net-list{list-style:none;margin:8px 0}
.net-list li{padding:10px;border-radius:8px;background:#0f3460;margin:4px 0;cursor:pointer;display:flex;justify-content:space-between;align-items:center}
.net-list li.selected{background:#533483;outline:2px solid #e94560}
.rssi{color:#888;font-size:.9em}
input{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#0f3460;color:#eee;font-size:1em;margin:4px 0}
.status{text-align:center;padding:8px;border-radius:8px;margin:8px 0;display:none}
.status.ok{background:#1b5e20;display:block}
.status.err{background:#b71c1c;display:block}
.hint{color:#888;font-size:.85em;margin-top:4px}
</style>
</head>
<body>
<h1>⚙ ClickerBot</h1>
<div class="card">
  <h2>Skin</h2>
  <select id="skinSelect"></select>
</div>
<div class="card">
  <h2>WiFi Status</h2>
  <div id="wifiStatus">Loading...</div>
</div>
<div class="card">
  <h2>WiFi Client</h2>
  <button class="btn btn-secondary" onclick="scanWifi()">🔍 Scan</button>
  <ul class="net-list" id="netList"></ul>
  <input id="manualSsid" placeholder="Or type SSID manually (hidden nets)">
  <input id="wifiPass" type="password" placeholder="Password">
</div>
<div class="card">
  <h2>Saved WiFi</h2>
  <ul class="net-list" id="savedList"></ul>
</div>
<div class="card">
  <h2>Nickname</h2>
  <input id="nickInput" maxlength="20" placeholder="My nick (empty = Clicker-XXXX)">
  <div id="nickHint" class="hint"></div>
</div>
<div class="card">
  <h2>Stats</h2>
  <div id="statsContent">Loading...</div>
</div>
<button class="btn btn-primary" onclick="saveSettings()">💾 Save</button>
<div id="statusMsg" class="status"></div>
<script>
function loadSkins(){fetch('/api/skins').then(r=>r.json()).then(d=>{const s=document.getElementById('skinSelect');s.innerHTML='';d.skins.forEach((sk,i)=>{const o=document.createElement('option');o.value=i;o.text=sk.name;if(sk.active)o.selected=true;s.appendChild(o)})})}
function loadStatus(){fetch('/api/status').then(r=>r.json()).then(d=>{let h='Mode: <b>'+d.mode+'</b><br>';h+='SSID: <b>'+d.ssid+'</b><br>';h+='IP: <b>'+d.ip+'</b><br>';h+='Status: <b>'+d.status+'</b>';document.getElementById('wifiStatus').innerHTML=h})}
function loadStats(){fetch('/api/stats').then(r=>r.json()).then(d=>{let h='';d.stats.forEach(s=>{h+=s.name+': <b>'+s.count+(s.wins>0?('/'+s.wins):'')+'</b><br>'});h+='<hr><b>total: '+d.total+'</b>';document.getElementById('statsContent').innerHTML=h})}
function scanWifi(){const btn=event.target;btn.disabled=true;btn.textContent='⏳ Scanning...';fetch('/api/wifi/scan').then(r=>r.json()).then(d=>{const ul=document.getElementById('netList');ul.innerHTML='';d.networks.forEach(n=>{const li=document.createElement('li');li.textContent=n.ssid||'[hidden]';const rssi=document.createElement('span');rssi.className='rssi';rssi.textContent=n.rssi+'dBm';li.appendChild(rssi);li.onclick=()=>{document.querySelectorAll('#netList li').forEach(l=>l.classList.remove('selected'));li.classList.add('selected');document.getElementById('wifiPass').focus()};ul.appendChild(li)})}).finally(()=>{btn.disabled=false;btn.textContent='🔍 Scan'})}
function loadNick(){fetch('/api/nick').then(r=>r.json()).then(d=>{document.getElementById('nickInput').value=d.nick||'';document.getElementById('nickHint').textContent=d.nick?('Saved: '+d.nick):''})}
function loadSavedWifi(){fetch('/api/wifi/saved').then(r=>r.json()).then(d=>{const ul=document.getElementById('savedList');ul.innerHTML='';if(!d.networks.length){const li=document.createElement('li');li.textContent='(none)';ul.appendChild(li);return}d.networks.forEach(n=>{const li=document.createElement('li');const span=document.createElement('span');span.textContent=n.ssid;li.appendChild(span);const del=document.createElement('button');del.type='button';del.textContent='✕';del.style.cssText='background:#533483;color:#fff;border:none;border-radius:6px;padding:4px 10px;font-size:1em;cursor:pointer;flex-shrink:0';del.onclick=(e)=>{e.stopPropagation();e.preventDefault();delSaved(n.ssid,del)};li.appendChild(del);ul.appendChild(li)})})}
function delSaved(ssid,btn){if(btn){btn.textContent='…';btn.disabled=true}fetch('/api/wifi/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid})}).then(r=>r.json()).then(()=>loadSavedWifi()).catch(()=>{if(btn){btn.textContent='✕';btn.disabled=false}})}
function saveSettings(){const skin=document.getElementById('skinSelect').value;const sel=document.querySelector('#netList li.selected');const manual=document.getElementById('manualSsid').value.trim();const ssid=manual||(sel?sel.firstChild.textContent.trim():'');const pass=document.getElementById('wifiPass').value;const nick=document.getElementById('nickInput').value;const msg=document.getElementById('statusMsg');msg.className='status';fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({skin:parseInt(skin),wifi_ssid:ssid,wifi_pass:pass,nick:nick})}).then(r=>r.json()).then(d=>{if(d.ok){msg.textContent='✅ Saved! Rebooting...';msg.className='status ok';setTimeout(()=>location.reload(),2000)}else{msg.textContent='❌ Error saving';msg.className='status err'}}).catch(()=>{msg.textContent='❌ Network error';msg.className='status err'})}
loadSkins();loadStatus();loadStats();loadNick();loadSavedWifi();
</script>
</body>
</html>
)raw";
}

void WebSrv::begin() {
  if (running) return; // уже запущен — не перерегистрируем обработчики
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });

  // API: список скинов (включая виртуальный пункт "Random")
  // API: skin list (including the virtual "Random" entry)
  server.on("/api/skins", []() {
    String json = "{\"skins\":[";
    for (uint8_t i = 0; i <= SkinRegistry::COUNT; i++) {
      if (i > 0) json += ",";
      const char* name = (i < SkinRegistry::COUNT) ? SkinRegistry::nameOf(i) : "Random";
      json += "{\"index\":" + String(i) +
              ",\"name\":\"" + name +
              "\",\"active\":" + (i == SettingsStore::loadSkinIndex() ? "true" : "false") + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  // API: сканирование WiFi сетей
  // API: WiFi scan
  server.on("/api/wifi/scan", []() {
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      String ssid = WiFi.SSID(i);
      // Экранируем кавычки в SSID
      // Escape quotes in the SSID
      ssid.replace("\"", "\\\"");
      json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
  });

  // API: список сохранённых сетей
  // API: saved networks
  server.on("/api/wifi/saved", []() {
    SettingsStore::WifiNetwork nets[SettingsStore::MAX_WIFI_NETWORKS];
    uint8_t n = SettingsStore::loadWiFiNetworks(nets, SettingsStore::MAX_WIFI_NETWORKS);
    String json = "{\"networks\":[";
    for (uint8_t i = 0; i < n; i++) {
      if (i > 0) json += ",";
      String ssid = String(nets[i].ssid);
      ssid.replace("\"", "\\\"");
      json += "{\"ssid\":\"" + ssid + "\"}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  // API: удаление сохранённой сети
  // API: delete a saved network
  server.on("/api/wifi/delete", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"no data\"}");
      return;
    }
    String body = server.arg("plain");
    String ssid;
    int p = body.indexOf("\"ssid\":\"");
    if (p >= 0) {
      p += 8; // "ssid":" = 8 символов / "ssid":" is 8 characters
      int e = body.indexOf("\"", p);
      if (e > p) ssid = body.substring(p, e);
    }
    if (ssid.length() > 0) SettingsStore::removeWiFiNetwork(ssid.c_str());
    server.send(200, "application/json", "{\"ok\":true}");
  });

  // API: статус
  // API: status
  server.on("/api/status", []() {
    String mode = "off";
    String ssid = "";
    String ip = "";
    String status = "off";

    wifi_mode_t wm = WiFi.getMode();
    if (wm == WIFI_AP || wm == WIFI_AP_STA) {
      mode = "AP";
      ssid = WiFi.softAPSSID();
      ip = WiFi.softAPIP().toString();
      status = "running";
    } else if (wm == WIFI_STA) {
      mode = "Client";
      ssid = SettingsStore::loadWiFiSSID();
      if (WiFi.status() == WL_CONNECTED) {
        status = "connected";
        ip = WiFi.localIP().toString();
      } else {
        status = "connecting";
      }
    }

    String json = "{\"mode\":\"" + mode +
                  "\",\"ssid\":\"" + ssid +
                  "\",\"ip\":\"" + ip +
                  "\",\"status\":\"" + status +
                  "\",\"currentSkin\":" + String(SettingsStore::loadSkinIndex()) + "}";
    server.send(200, "application/json", json);
  });

  // API: статистика по скинам
  // API: per-skin statistics
  server.on("/api/stats", []() {
    String json = "{\"stats\":[";
    uint32_t total = 0;
    for (uint8_t i = 0; i < SkinRegistry::COUNT; i++) {
      if (i > 0) json += ",";
      // Ключ счётчика у самого скина — новый скин попадает в веб автоматически
      // The counter key comes from the skin itself, so a new skin shows up here automatically
      uint32_t c = SettingsStore::loadCounter(SkinRegistry::get(i)->counterKey());
      uint32_t sub = SkinRegistry::get(i)->statsSubCounter();
      json += "{\"name\":\"" + String(SkinRegistry::nameOf(i)) +
              "\",\"count\":" + String(c) +
              ",\"wins\":" + String(sub) + "}";
      total += c;
    }
    json += "],\"total\":" + String(total) + "}";
    server.send(200, "application/json", json);
  });

  // API: текущий никнейм
  // API: current nickname
  server.on("/api/nick", []() {
    String nick = SettingsStore::loadNick();
    String json = "{\"nick\":\"" + jsonEscape(nick.c_str()) + "\"}";
    server.send(200, "application/json", json);
  });

  // API: сохранение настроек
  // API: save settings
  server.on("/api/settings", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"no data\"}");
      return;
    }
    String body = server.arg("plain");

    // Парсим JSON вручную: "skin":N, "wifi_ssid":"...", "wifi_pass":"...", "nick":"..."
    // Parsing the JSON by hand: "skin":N, "wifi_ssid":"...", "wifi_pass":"...", "nick":"..."
    int skinIdx = -1;
    String wifiSSID, wifiPass, nick;

    int p = body.indexOf("\"skin\":");
    if (p >= 0) skinIdx = body.substring(p + 7).toInt();

    p = body.indexOf("\"wifi_ssid\":\"");
    if (p >= 0) {
      p += 13; // "wifi_ssid":" = 13 символов
      int e = body.indexOf("\"", p);
      if (e > p) wifiSSID = body.substring(p, e);
    }

    p = body.indexOf("\"wifi_pass\":\"");
    if (p >= 0) {
      p += 13;
      int e = body.indexOf("\"", p);
      if (e > p) wifiPass = body.substring(p, e);
    }

    p = body.indexOf("\"nick\":\"");
    if (p >= 0) {
      p += 8; // "nick":" = 8 символов
      int e = body.indexOf("\"", p);
      if (e > p) nick = body.substring(p, e);
    }

    if (nick.length() > 0) SettingsStore::saveNick(nick);

    // Разрешаем и маркер Random (индекс == COUNT)
    // The Random marker is allowed too (index == COUNT)
    if (skinIdx >= 0 && skinIdx <= SkinRegistry::COUNT)
      SettingsStore::saveSkinIndex(skinIdx);
    if (wifiSSID.length() > 0)
      SettingsStore::upsertWiFiNetwork(wifiSSID.c_str(), wifiPass.c_str());

    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.begin();
  running = true;
}

void WebSrv::stop() {
  server.stop();
  running = false;
}

void WebSrv::handle() {
  if (running) server.handleClient();
}

bool WebSrv::isRunning() {
  return running;
}
