#include "WebPortal.h"

#include <ESP8266WiFi.h>

namespace {

const char kPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>7 Segment Clock</title>
<style>
:root{color-scheme:dark;--bg:#101418;--card:#1a2229;--text:#edf4f7;--muted:#9aabb5;--accent:#55d6a7;--line:#30404a}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.5 system-ui,sans-serif}main{max-width:680px;margin:auto;padding:24px 16px 48px}h1{font-size:1.7rem;margin:0 0 4px}h2{font-size:1.05rem;margin:0 0 18px}.muted{color:var(--muted)}.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:20px;margin-top:18px}.time{font-size:2.8rem;font-weight:650;letter-spacing:.06em;font-variant-numeric:tabular-nums}.status{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:14px}.status span{color:var(--muted)}.status strong{display:block;overflow-wrap:anywhere}label{display:block;margin:16px 0 6px;font-weight:600}input,select{width:100%;padding:11px 12px;border:1px solid var(--line);border-radius:9px;background:#10171c;color:var(--text);font:inherit}input[type=checkbox]{width:auto;margin-right:8px}.check{font-weight:500}.row{display:flex;align-items:center;gap:14px}.row input{flex:1}.level{font-size:1.4rem;min-width:2ch;text-align:center;color:var(--accent)}button{border:0;border-radius:9px;padding:11px 18px;background:var(--accent);color:#082019;font:600 16px system-ui;cursor:pointer;margin-top:20px}button.secondary{background:#34444e;color:var(--text)}.actions{display:flex;gap:10px;flex-wrap:wrap}small{display:block;color:var(--muted);margin-top:5px}#message{min-height:24px;margin-top:12px;color:var(--accent)}a{color:var(--accent)}.networkRow{display:flex;align-items:stretch;gap:10px}.networkRow select{flex:1;min-width:0}.networkRow button{margin:0;white-space:nowrap}button:disabled{cursor:wait;opacity:.65}@media(max-width:420px){.status{grid-template-columns:1fr}.time{font-size:2.3rem}.networkRow{flex-direction:column}}
</style>
</head>
<body><main><noscript><p class="card">JavaScript is disabled. Open http://192.168.4.1/ in your full browser to configure the clock.</p></noscript>
<header><h1>7 Segment Clock</h1><div class="muted">303WIFILC01 · firmware {{VERSION}}</div></header>
<section class="card"><div class="time" id="time">--:--</div><div class="status">
<div><span>Wi-Fi</span><strong id="wifi">Checking…</strong></div><div><span>IP address</span><strong id="ip">—</strong></div>
<div><span>Last NTP sync</span><strong id="sync">Never</strong></div><div><span>Configuration AP</span><strong id="ap">Off</strong></div>
</div></section>
<section class="card" id="sensorCard" hidden><h2>Environment</h2><div class="status">
<div><span>Temperature</span><strong id="temperature">—</strong></div><div><span>Humidity</span><strong id="humidity">—</strong></div>
<div><span>Pressure</span><strong id="pressure">—</strong></div><div><span>Light sensor (raw)</span><strong id="lightRaw">—</strong></div>
</div><small id="sensorStatus"></small></section>
<section class="card"><h2>Brightness</h2><div class="row"><input id="brightness" type="range" min="1" max="8" step="1" value="{{BRIGHTNESS}}" oninput="byId('brightnessValue').textContent=this.value" onchange="setBrightness(this.value)"><output class="level" id="brightnessValue">{{BRIGHTNESS}}</output></div></section>
<section class="card" id="otaCard"><h2>Firmware update</h2><p>Upload a new firmware binary. The clock restarts after a successful update.</p><a href="/update">Open firmware updater</a></section>
<form class="card" id="settings" method="post" action="/api/settings"><h2>Settings</h2>
<label for="networks">Available Wi-Fi networks</label><div class="networkRow"><select id="networks"><option value="">Scan to see nearby networks</option></select><button type="button" class="secondary" id="scanWifiButton">Scan networks</button></div><small id="scanStatus" role="status">Select a network to fill in its name. Signal strength is shown in dBm (higher is better).</small>
<label for="ssid">Wi-Fi network</label><input id="ssid" name="ssid" maxlength="32" value="{{SSID}}" autocomplete="off" required>
<label for="password">Wi-Fi password</label><input id="password" name="password" type="password" maxlength="64" value="{{PASSWORD}}" autocomplete="new-password"><small>Leave empty for an open network.</small>
<label class="check"><input id="showPassword" type="checkbox">Show Wi-Fi password</label>
<label for="timezone">POSIX timezone</label><input id="timezone" name="timezone" maxlength="63" value="{{TIMEZONE}}" required><small>Examples: <code>UTC0</code> or <code>CET-1CEST,M3.5.0,M10.5.0/3</code>. DST switches automatically when daylight-saving rules are included; fixed offsets such as <code>UTC0</code> never switch.</small>
<label for="ntp1">Primary NTP server</label><input id="ntp1" name="ntp1" maxlength="63" value="{{NTP1}}" required>
<label for="ntp2">Secondary NTP server</label><input id="ntp2" name="ntp2" maxlength="63" value="{{NTP2}}">
<label for="sync">NTP sync interval (minutes)</label><input id="sync" name="sync" type="number" min="5" max="1440" value="{{SYNC}}" required>
<label class="check"><input name="use12Hour" type="checkbox" value="1" {{HOUR12}}>Use 12-hour format (fourth dot indicates PM)</label>
<label class="check"><input name="blinkColon" type="checkbox" value="1" {{COLONBLINK}}>Flash the time colon (unchecked keeps it solid)</label>
<h2>Hardware override</h2>
<label class="check"><input name="sensorOverride" id="sensorOverride" type="checkbox" value="1" {{SENSOROVERRIDE}} onchange="byId('sensorOptions').hidden=!this.checked"><strong>I have sensor(s) connected</strong></label>
<small>Disables UP and DOWN. Leave unchecked for the unmodified board and normal buttons. SET remains available for the configuration portal.</small>
<div id="sensorOptions" {{SENSORHIDDEN}}>
<label class="check"><input name="autoBrightness" type="checkbox" value="1" {{AUTOBRIGHTNESS}}>LDR on TOUT/A0 — automatic brightness</label>
<label for="lightDark">LDR dark calibration (0–1023)</label><input id="lightDark" name="lightDark" type="number" min="0" max="1022" value="{{LIGHTDARK}}">
<label for="lightBright">LDR bright calibration (1–1023)</label><input id="lightBright" name="lightBright" type="number" min="1" max="1023" value="{{LIGHTBRIGHT}}">
<label class="check"><input name="thpSensor" type="checkbox" value="1" {{THPSENSOR}}>BME280 temperature / humidity / pressure sensor</label><small>BME280 SDA → UP/GPIO4, SCL → P1.RX/GPIO3. Use 3.3 V and external I²C pull-ups. TX remains available for logging.</small>
</div>
<input type="hidden" name="brightness" id="brightnessSetting" value="{{BRIGHTNESS}}">
<div class="actions"><button type="submit">Save & restart</button><button type="button" class="secondary" onclick="restartClock()">Restart</button></div><div id="message" role="status"></div>
</form>
<p class="muted"><small>Configuration is stored on the device. The Wi-Fi password is not encrypted in flash.</small></p>
</main><script>
function byId(id){return document.getElementById(id);}
function apiRequest(method,url,body,callback){
var xhr=new XMLHttpRequest(),finished=false;
function finish(ok,status,text){if(finished)return;finished=true;callback(ok,text,status);}
if(method==='GET')url+=(url.indexOf('?')===-1?'?':'&')+'_='+new Date().getTime();
xhr.open(method,url,true);
xhr.timeout=30000;
xhr.onreadystatechange=function(){if(xhr.readyState===4)finish(xhr.status>=200&&xhr.status<300,xhr.status,xhr.responseText);};
xhr.onerror=function(){finish(false,0,'');};
xhr.ontimeout=function(){finish(false,0,'');};
if(body!==null)xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
xhr.send(body);
}
function refreshStatus(){apiRequest('GET','/api/status',null,function(ok,text){if(!ok)return;var s;try{s=JSON.parse(text);}catch(e){return;}byId('time').textContent=s.time;byId('wifi').textContent=s.wifi?('Connected · '+s.ssid):'Disconnected';byId('ip').textContent=s.ip||'—';byId('sync').textContent=s.lastSync;byId('ap').textContent=s.ap?(s.apName+' · '+s.apIp):'Off';var brightness=byId('brightness');if(document.activeElement!==brightness){brightness.value=s.brightness;byId('brightnessValue').textContent=s.brightness;}byId('brightnessSetting').value=s.brightness;brightness.disabled=s.autoBrightness;byId('sensorCard').hidden=!s.sensorsEnabled;byId('temperature').textContent=s.temperature===null?'—':s.temperature.toFixed(1)+' °C';byId('humidity').textContent=s.humidity===null?'—':s.humidity.toFixed(1)+' %';byId('pressure').textContent=s.pressure===null?'—':s.pressure.toFixed(1)+' hPa';byId('lightRaw').textContent=s.autoBrightness?s.lightRaw:'Disabled';byId('sensorStatus').textContent=s.thpEnabled?(s.thpAvailable?'BME280 connected':'BME280 not detected'):'THP sensor disabled';});}
function setBrightness(level){byId('brightnessSetting').value=level;apiRequest('POST','/api/brightness','level='+encodeURIComponent(level),function(){});}
function signalQuality(rssi){return rssi>=-50?'excellent':rssi>=-60?'good':rssi>=-70?'fair':'weak';}
function scanWifi(){
var button=byId('scanWifiButton'),select=byId('networks'),status=byId('scanStatus');
button.disabled=true;select.disabled=true;status.textContent='Scanning for networks…';
apiRequest('GET','/api/wifi/networks',null,function(ok,text,httpStatus){
button.disabled=false;select.disabled=false;
if(!ok){status.textContent=httpStatus?'Scan request failed (HTTP '+httpStatus+').':'Scan request failed. Check the connection and try again.';return;}
try{
var data=JSON.parse(text),networks=[],i,j,network,existing;
if(!data.networks||typeof data.networks.length!=='number')throw new Error('Response has no network list');
for(i=0;i<data.networks.length;i++){
network=data.networks[i];existing=null;
for(j=0;j<networks.length;j++){if(networks[j].ssid===network.ssid){existing=networks[j];break;}}
if(!existing)networks.push(network);else if(network.rssi>existing.rssi)existing.rssi=network.rssi;
}
networks.sort(function(a,b){return b.rssi-a.rssi;});
status.textContent='Rendering '+networks.length+' network'+(networks.length===1?'':'s')+'…';
select.options.length=0;
var placeholder=document.createElement('option');placeholder.value='';placeholder.textContent=networks.length?'Select a network':'No networks found';select.add(placeholder);
for(i=0;i<networks.length;i++){
network=networks[i];var option=document.createElement('option');option.value=network.ssid;option.textContent=network.ssid+' · '+network.rssi+' dBm · '+signalQuality(network.rssi);select.add(option);
}
status.textContent=networks.length?networks.length+' network'+(networks.length===1?'':'s')+' found. Select one to fill in its name.':'No networks found. Try again or enter the name manually.';
}catch(e){status.textContent='Scan display error: '+(e.message||e);}
});
}
byId('showPassword').addEventListener('change',function(){byId('password').type=this.checked?'text':'password';});
byId('networks').addEventListener('change',function(e){if(e.target.value)byId('ssid').value=e.target.value;});
byId('scanWifiButton').addEventListener('click',scanWifi);
function serializeForm(form){var fields=[];for(var i=0;i<form.elements.length;i++){var field=form.elements[i];if(!field.name||field.disabled)continue;if((field.type==='checkbox'||field.type==='radio')&&!field.checked)continue;fields.push(encodeURIComponent(field.name)+'='+encodeURIComponent(field.value));}return fields.join('&');}
byId('settings').addEventListener('submit',function(e){e.preventDefault();byId('message').textContent='Saving…';apiRequest('POST','/api/settings',serializeForm(e.target),function(ok,text,httpStatus){byId('message').textContent=ok||httpStatus===0?'Saved. Restarting…':'Could not save settings.';});});
function restartClock(){byId('message').textContent='Restarting…';apiRequest('POST','/api/restart',null,function(){});}
refreshStatus();setInterval(refreshStatus,2000);
</script></body></html>)HTML";

} // namespace

WebPortal::WebPortal(ConfigStore& config, Tm1650Display& display)
    : config_(config), display_(display) {}

void WebPortal::begin(StatusProvider statusProvider) {
    statusProvider_ = statusProvider;

    httpUpdater_.setup(&server_);

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/wifi/networks", HTTP_GET, [this]() { handleWifiNetworks(); });
    server_.on("/api/brightness", HTTP_POST, [this]() { handleBrightness(); });
    server_.on("/api/settings", HTTP_POST, [this]() { handleSettings(); });
    server_.on("/api/restart", HTTP_POST, [this]() { handleRestart(); });
    server_.on("/generate_204", HTTP_GET, [this]() { handleCaptiveRedirect(); });
    server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleCaptiveRedirect(); });
    server_.on("/library/test/success.html", HTTP_GET, [this]() { handleCaptiveRedirect(); });
    server_.on("/ncsi.txt", HTTP_GET, [this]() { handleCaptiveRedirect(); });
    server_.on("/fwlink", HTTP_GET, [this]() { handleCaptiveRedirect(); });
    server_.onNotFound([this]() {
        if (apActive_) handleCaptiveRedirect();
        else server_.send(404, "text/plain", "Not found");
    });
    server_.begin();
}

void WebPortal::handle() {
    if (apActive_) dnsServer_.processNextRequest();
    server_.handleClient();

    if (restartAt_ != 0 && static_cast<int32_t>(millis() - restartAt_) >= 0) {
        restartAt_ = 0;
        ESP.restart();
    }
}

void WebPortal::startAccessPoint() {
    if (apActive_) return;

    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    apName_ = "7SEG-" + String(suffix);

    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(apName_.c_str())) {
        Serial.println(F("portal: failed to start access point"));
        return;
    }
    dnsServer_.start(53, "*", WiFi.softAPIP());
    apActive_ = true;
    Serial.printf("portal: %s at %s\n", apName_.c_str(), WiFi.softAPIP().toString().c_str());
}

void WebPortal::stopAccessPoint() {
    if (!apActive_) return;
    dnsServer_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive_ = false;
    Serial.println(F("portal: access point stopped"));
}

String WebPortal::htmlEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 16);
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '\"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += value[i];
        }
    }
    return escaped;
}

String WebPortal::jsonEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
            case '\"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<uint8_t>(value[i]) < 0x20) {
                    char encoded[7];
                    snprintf(encoded, sizeof(encoded), "\\u%04x", value[i]);
                    escaped += encoded;
                } else {
                    escaped += value[i];
                }
        }
    }
    return escaped;
}

void WebPortal::handleRoot() {
    const ClockConfig& config = config_.get();
    String page = FPSTR(kPage);
    page.replace("{{VERSION}}", VERSION);
    page.replace("{{SSID}}", htmlEscape(config.ssid));
    page.replace("{{PASSWORD}}", htmlEscape(config.password));
    page.replace("{{TIMEZONE}}", htmlEscape(config.timezone));
    page.replace("{{NTP1}}", htmlEscape(config.ntpServer1));
    page.replace("{{NTP2}}", htmlEscape(config.ntpServer2));
    page.replace("{{SYNC}}", String(config.syncIntervalMinutes));
    page.replace("{{BRIGHTNESS}}", String(display_.brightness()));
    page.replace("{{HOUR12}}", config.use12Hour ? "checked" : "");
    page.replace("{{COLONBLINK}}", config.blinkColon ? "checked" : "");
    page.replace("{{SENSOROVERRIDE}}", config.sensorOverride ? "checked" : "");
    page.replace("{{AUTOBRIGHTNESS}}", config.autoBrightness ? "checked" : "");
    page.replace("{{THPSENSOR}}", config.thpSensorEnabled ? "checked" : "");
    page.replace("{{SENSORHIDDEN}}", config.sensorOverride ? "" : "hidden");
    page.replace("{{LIGHTDARK}}", String(config.lightDark));
    page.replace("{{LIGHTBRIGHT}}", String(config.lightBright));
    server_.sendHeader("Cache-Control", "no-store, max-age=0");
    server_.sendHeader("Pragma", "no-cache");
    server_.send(200, "text/html", page);
}

void WebPortal::handleStatus() {
    server_.send(200, "application/json", statusProvider_ ? statusProvider_() : "{}");
}

void WebPortal::handleWifiNetworks() {
    const int16_t count = WiFi.scanNetworks(false, false);
    if (count < 0) {
        WiFi.scanDelete();
        server_.send(500, "application/json", "{\"ok\":false,\"error\":\"Wi-Fi scan failed\"}");
        return;
    }

    String json = "{\"ok\":true,\"networks\":[";
    for (int16_t i = 0; i < count; ++i) {
        if (i != 0) json += ',';
        json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    server_.send(200, "application/json", json);
}

void WebPortal::handleBrightness() {
    if (!server_.hasArg("level")) {
        server_.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    if (config_.get().sensorOverride && config_.get().autoBrightness) {
        server_.send(409, "application/json", "{\"ok\":false,\"error\":\"Automatic brightness is active\"}");
        return;
    }
    const int level = server_.arg("level").toInt();
    if (level < 1 || level > 8) {
        server_.send(400, "application/json", "{\"ok\":false}");
        return;
    }

    display_.setBrightness(level);
    const bool saved = config_.saveBrightness(level);
    server_.send(saved ? 200 : 500, "application/json", saved ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebPortal::handleSettings() {
    ClockConfig updated = config_.get();
    updated.ssid = server_.arg("ssid");
    updated.password = server_.arg("password");
    updated.ntpServer1 = server_.arg("ntp1");
    updated.ntpServer2 = server_.arg("ntp2");
    updated.timezone = server_.arg("timezone");
    updated.syncIntervalMinutes = server_.arg("sync").toInt();
    updated.use12Hour = server_.arg("use12Hour") == "1";
    updated.blinkColon = server_.arg("blinkColon") == "1";
    updated.brightness = server_.arg("brightness").toInt();
    updated.sensorOverride = server_.arg("sensorOverride") == "1";
    updated.autoBrightness = server_.arg("autoBrightness") == "1";
    updated.thpSensorEnabled = server_.arg("thpSensor") == "1";
    updated.lightDark = server_.arg("lightDark").toInt();
    updated.lightBright = server_.arg("lightBright").toInt();

    if (!config_.save(updated)) {
        server_.send(500, "application/json", "{\"ok\":false}");
        return;
    }
    server_.send(200, "application/json", "{\"ok\":true}");
    scheduleRestart();
}

void WebPortal::handleRestart() {
    server_.send(200, "application/json", "{\"ok\":true}");
    scheduleRestart();
}

void WebPortal::handleCaptiveRedirect() {
    server_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server_.send(302, "text/plain", "Open the clock configuration portal");
}

void WebPortal::scheduleRestart() {
    restartAt_ = millis() + 750;
}
