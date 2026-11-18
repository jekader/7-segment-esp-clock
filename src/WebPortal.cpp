#include "WebPortal.h"
#include "Version.h"

#include <ESP8266WiFi.h>
#include <FS.h>
#include <StreamString.h>
#include <Updater.h>
#include <flash_hal.h>

namespace {

const char kPageStyle[] PROGMEM = R"CSS(:root{color-scheme:dark;--bg:#101418;--card:#1a2229;--text:#edf4f7;--muted:#9aabb5;--accent:#55d6a7;--line:#30404a}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.5 system-ui,sans-serif}main{max-width:680px;margin:auto;padding:24px 16px 48px}h1{font-size:1.7rem;margin:0 0 4px}h2{font-size:1.05rem;margin:0 0 18px}.muted{color:var(--muted)}.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:20px;margin-top:18px}.time{font-size:2.8rem;font-weight:650;letter-spacing:.06em;font-variant-numeric:tabular-nums}.status{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:14px}.status span{color:var(--muted)}.status strong{display:block;overflow-wrap:anywhere}label{display:block;margin:16px 0 6px;font-weight:600}input,select{width:100%;padding:11px 12px;border:1px solid var(--line);border-radius:9px;background:#10171c;color:var(--text);font:inherit}input[type=checkbox]{width:auto;margin-right:8px}.check{font-weight:500}.row{display:flex;align-items:center;gap:14px}.row input{flex:1}.level{font-size:1.4rem;min-width:2ch;text-align:center;color:var(--accent)}button{border:0;border-radius:9px;padding:11px 18px;background:var(--accent);color:#082019;font:600 16px system-ui;cursor:pointer;margin-top:20px}button.secondary{background:#34444e;color:var(--text)}.actions{display:flex;gap:10px;flex-wrap:wrap}small{display:block;color:var(--muted);margin-top:5px}#message{min-height:24px;margin-top:12px;color:var(--accent)}a{color:var(--accent)}.networkRow{display:flex;align-items:stretch;gap:10px}.networkRow select{flex:1;min-width:0}.networkRow button{margin:0;white-space:nowrap}button:disabled{cursor:wait;opacity:.65}@media(max-width:420px){.status{grid-template-columns:1fr}.time{font-size:2.3rem}.networkRow{flex-direction:column}})CSS";

const char kPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>7 Segment Clock</title>
<style>{{STYLES}}</style>
</head>
<body><main><noscript><p class="card">JavaScript is disabled. Open http://192.168.4.1/ in your full browser to configure the clock.</p></noscript>
<header><h1>7 Segment Clock</h1><div class="muted">303WIFILC01 · firmware {{VERSION}}</div></header>
<section class="card"><div class="time" id="time">--:--</div><div class="status">
<div><span>Wi-Fi</span><strong id="wifi">Checking…</strong></div><div><span>IP address</span><strong id="ip">—</strong></div>
<div><span>Last NTP sync</span><strong id="sync">Never</strong></div><div><span>Configuration AP</span><strong id="ap">Off</strong></div>
<div><span>Uptime</span><strong id="uptime">—</strong></div><div><span>Last reboot reason</span><strong id="resetReason">—</strong></div>
</div></section>
<section class="card" id="sensorCard" hidden><h2>Environment</h2><div class="status">
<div><span>Temperature</span><strong id="temperature">—</strong></div><div><span>Humidity</span><strong id="humidity">—</strong></div>
<div><span>Pressure</span><strong id="pressure">—</strong></div><div><span>Light sensor (raw)</span><strong id="lightRaw">—</strong></div>
</div><small id="sensorStatus"></small></section>
<section class="card"><h2>Display</h2><label class="check"><input id="displayPower" type="checkbox" checked onchange="setDisplayPower(this.checked)">Screen on</label><div class="row"><input id="brightness" type="range" min="1" max="8" step="1" value="{{BRIGHTNESS}}" oninput="byId('brightnessValue').textContent=this.value" onchange="setBrightness(this.value)"><output class="level" id="brightnessValue">{{BRIGHTNESS}}</output></div></section>
<section class="card" id="otaCard"><h2>Firmware update</h2><p>Installed firmware: <strong>{{VERSION}}</strong></p><p>Upload a firmware BIN and the clock restarts after a successful update.</p><div class="actions"><a href="/update">Open firmware updater</a><button type="button" class="secondary" id="checkRelease">Check latest release</button><a id="releasePage" href="https://github.com/jekader/7-segment-esp-clock/releases/latest" target="_blank" rel="noopener">Releases page</a></div><p id="releaseStatus" role="status"></p><div id="releaseAsset" hidden><strong id="latestVersion"></strong><br><a id="downloadRelease" href="#" target="_blank" rel="noopener">Download OTA firmware.bin</a><small>Download the BIN, then choose it in the firmware updater.</small></div></section>
<form class="card" id="settings" method="post" action="/api/settings"><h2>Settings</h2>
<label for="networks">Available Wi-Fi networks</label><div class="networkRow"><select id="networks"><option value="">Scan to see nearby networks</option></select><button type="button" class="secondary" id="scanWifiButton">Scan networks</button></div><small id="scanStatus" role="status">Select a network to fill in its name. Filled bars indicate signal strength.</small>
<label for="ssid">Wi-Fi network</label><input id="ssid" name="ssid" maxlength="32" value="{{SSID}}" autocomplete="off" required>
<label for="password">Wi-Fi password</label><input id="password" name="password" type="password" maxlength="64" value="{{PASSWORD}}" autocomplete="new-password"><small>Leave empty for an open network.</small>
<label class="check"><input id="showPassword" type="checkbox">Show Wi-Fi password</label>
<label for="timezone">POSIX timezone</label><input id="timezone" name="timezone" maxlength="63" value="{{TIMEZONE}}" required><small>Examples: <code>UTC0</code> or <code>CET-1CEST,M3.5.0,M10.5.0/3</code>. DST switches automatically when daylight-saving rules are included; fixed offsets such as <code>UTC0</code> never switch.</small>
<label for="ntp1">Primary NTP server</label><input id="ntp1" name="ntp1" maxlength="63" value="{{NTP1}}" required>
<label for="ntp2">Secondary NTP server</label><input id="ntp2" name="ntp2" maxlength="63" value="{{NTP2}}">
<label for="sync">NTP sync interval (minutes)</label><input id="sync" name="sync" type="number" min="5" max="1440" value="{{SYNC}}" required>
<label class="check"><input name="use12Hour" type="checkbox" value="1" {{HOUR12}}>Use 12-hour format (fourth dot indicates PM)</label>
<label class="check"><input name="blinkColon" type="checkbox" value="1" {{COLONBLINK}}>Flash the time colon</label>
<label class="check"><input name="periodicDate" type="checkbox" value="1" {{PERIODICDATE}}>Show the date periodically</label>
<label for="dateDisplaySeconds">Date display duration (seconds)</label><input id="dateDisplaySeconds" name="dateDisplaySeconds" type="number" min="1" max="60" value="{{DATEDURATION}}" required>
<label for="dateIntervalSeconds">Date display interval (seconds)</label><input id="dateIntervalSeconds" name="dateIntervalSeconds" type="number" min="10" max="86400" value="{{DATEINTERVAL}}" required><small>The display returns to the time after each duration. Duration cannot exceed the interval.</small>
<h2>MQTT / Home Assistant</h2>
<label class="check"><input name="mqttEnabled" type="checkbox" value="1" {{MQTTENABLED}}>Enable MQTT</label>
<label for="mqttHost">Broker host or IP</label><input id="mqttHost" name="mqttHost" maxlength="63" value="{{MQTTHOST}}" placeholder="mqtt.local">
<label for="mqttPort">Broker port</label><input id="mqttPort" name="mqttPort" type="number" min="1" max="65535" value="{{MQTTPORT}}" required>
<label for="mqttUsername">MQTT username (optional)</label><input id="mqttUsername" name="mqttUsername" maxlength="64" value="{{MQTTUSERNAME}}" autocomplete="off">
<label for="mqttPassword">MQTT password (optional)</label><input id="mqttPassword" name="mqttPassword" type="password" maxlength="64" value="{{MQTTPASSWORD}}" autocomplete="new-password">
<label for="mqttBaseTopic">Base topic</label><input id="mqttBaseTopic" name="mqttBaseTopic" maxlength="48" value="{{MQTTTOPIC}}" required>
<label for="mqttDiscoveryPrefix">Home Assistant discovery prefix</label><input id="mqttDiscoveryPrefix" name="mqttDiscoveryPrefix" maxlength="48" value="{{MQTTDISCOVERY}}" required>
<label for="mqttTelemetrySeconds">Sensor publish interval (seconds)</label><input id="mqttTelemetrySeconds" name="mqttTelemetrySeconds" type="number" min="5" max="3600" value="{{MQTTINTERVAL}}" required><small>Home Assistant discovery is published automatically. MQTT uses an unencrypted connection.</small>
<h2>Hardware override</h2>
<label class="check"><input name="sensorOverride" id="sensorOverride" type="checkbox" value="1" {{SENSOROVERRIDE}} onchange="byId('sensorOptions').hidden=!this.checked"><strong>I have sensor(s) connected</strong></label>
<small>Disables UP and DOWN buttons</small>
<div id="sensorOptions" {{SENSORHIDDEN}}>
<label class="check"><input name="autoBrightness" type="checkbox" value="1" {{AUTOBRIGHTNESS}}>LDR on TOUT/A0 — automatic brightness</label>
<label for="lightDark">LDR dark calibration (0–1023)</label><input id="lightDark" name="lightDark" type="number" min="0" max="1022" value="{{LIGHTDARK}}">
<label for="lightBright">LDR bright calibration (1–1023)</label><input id="lightBright" name="lightBright" type="number" min="1" max="1023" value="{{LIGHTBRIGHT}}">
<label class="check"><input name="thpSensor" type="checkbox" value="1" {{THPSENSOR}}>BME280 temperature / humidity / pressure sensor</label><small>BME280 SDA → UP/GPIO4, SCL → P1.RX/GPIO3. Use 3.3 V and external I²C pull-ups. TX remains available for logging.</small>
</div>
<input type="hidden" name="brightness" id="brightnessSetting" value="{{BRIGHTNESS}}">
<div class="actions"><button type="submit">Save & restart</button><button type="button" class="secondary" onclick="restartClock()">Restart</button></div><div id="message" role="status"></div>
</form>
<p class="muted"><small>Configuration is stored on the device. Wi-Fi and MQTT passwords are not encrypted in flash.</small></p>
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
function refreshStatus(){apiRequest('GET','/api/status',null,function(ok,text){if(!ok)return;var s;try{s=JSON.parse(text);}catch(e){return;}byId('time').textContent=s.time;byId('wifi').textContent=s.wifi?('Connected · '+s.ssid):'Disconnected';byId('ip').textContent=s.ip||'—';byId('sync').textContent=s.lastSync;byId('ap').textContent=s.ap?(s.apName+' · '+s.apIp):'Off';byId('uptime').textContent=s.uptime;byId('resetReason').textContent=s.resetReason;var brightness=byId('brightness');if(document.activeElement!==brightness){brightness.value=s.brightness;byId('brightnessValue').textContent=s.brightness;}byId('displayPower').checked=s.power;byId('brightnessSetting').value=s.brightness;brightness.disabled=s.autoBrightness;byId('sensorCard').hidden=!s.sensorsEnabled;byId('temperature').textContent=s.temperature===null?'—':s.temperature.toFixed(1)+' °C';byId('humidity').textContent=s.humidity===null?'—':s.humidity.toFixed(1)+' %';byId('pressure').textContent=s.pressure===null?'—':s.pressure.toFixed(1)+' hPa';byId('lightRaw').textContent=s.autoBrightness?s.lightRaw:'Disabled';byId('sensorStatus').textContent=s.thpEnabled?(s.thpAvailable?'BME280 connected':'BME280 not detected'):'THP sensor disabled';});}
function setBrightness(level){byId('brightnessSetting').value=level;apiRequest('POST','/api/brightness','level='+encodeURIComponent(level),function(){});}
function setDisplayPower(enabled){apiRequest('POST','/api/display/power','enabled='+(enabled?'1':'0'),function(){});}
function checkLatestRelease(){
var button=byId('checkRelease'),status=byId('releaseStatus'),assetSection=byId('releaseAsset');
button.disabled=true;status.textContent='Checking GitHub releases…';assetSection.hidden=true;
apiRequest('GET','https://api.github.com/repos/jekader/7-segment-esp-clock/releases/latest',null,function(ok,text,httpStatus){
button.disabled=false;
if(!ok){status.textContent=httpStatus?'GitHub request failed (HTTP '+httpStatus+'). Use the releases page instead.':'Could not reach GitHub. Check Internet access or use the releases page.';return;}
try{
var release=JSON.parse(text),asset=null;
if(!release.tag_name)throw new Error('Release has no version tag');
byId('latestVersion').textContent='Latest release: '+release.tag_name;
if(release.html_url)byId('releasePage').href=release.html_url;
for(var i=0;i<release.assets.length;i++){if(release.assets[i].name==='7-segment-esp-clock-firmware.bin'){asset=release.assets[i];break;}}
if(asset){byId('downloadRelease').href=asset.browser_download_url;assetSection.hidden=false;status.textContent='Download the OTA BIN, then upload it through the firmware updater.';}else{status.textContent='The latest release has no OTA BIN. Use the releases page to inspect its assets.';}
}catch(e){status.textContent='Could not read the GitHub release response. Use the releases page instead.';}
});
}
byId('checkRelease').addEventListener('click',checkLatestRelease);
function signalBars(rssi){return rssi>=-55?'[▂▄▆█]':rssi>=-65?'[▂▄▆_]':rssi>=-75?'[▂▄__]':'[▂___]';}
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
network=networks[i];var option=document.createElement('option');option.value=network.ssid;option.textContent=signalBars(network.rssi)+' '+network.ssid;select.add(option);
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

const char kUpdatePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Firmware update · 7 Segment Clock</title><style>{{STYLES}}</style></head>
<body><main><header><h1>Firmware update</h1><div class="muted">303WIFILC01 · installed {{VERSION}}</div></header>
<section class="card"><h2>Choose an image</h2>
<form id="updateForm" method="post" action="/update" enctype="multipart/form-data">
<label for="imageType">Update type</label><select id="imageType"><option value="firmware">Firmware (recommended)</option><option value="filesystem">LittleFS filesystem (advanced)</option></select>
<label for="imageFile">Image file</label><input id="imageFile" name="firmware" type="file" accept=".bin,.bin.gz" required>
<div id="firmwareHelp"><p>Use this for normal firmware upgrades or to restore a release. Accepted formats are <strong>.bin</strong> and <strong>.bin.gz</strong>.</p><small>From GitHub releases: <code>7-segment-esp-clock-firmware.bin</code><br>From a local build: <code>.pio/build/303WIFILC01/firmware.bin</code><br>Do not upload the ELF or SHA256SUMS file.</small></div>
<div id="filesystemHelp" hidden><p><strong>Advanced:</strong> use this only to replace the 64 KB LittleFS configuration partition. Accepted formats are <strong>.bin</strong> and <strong>.bin.gz</strong>.</p><small>Build with <code>pio run --target buildfs</code> and select <code>.pio/build/303WIFILC01/littlefs.bin</code>. This is not a firmware update and can erase saved Wi-Fi, MQTT, and clock settings.</small></div>
<button id="updateButton" type="submit">Start upgrade</button>
<p id="updateStatus" role="status" aria-live="polite"></p>
<p class="muted"><small>Keep this page open and the clock powered on until it reboots.</small></p>
</form></section>
<section class="card"><h2>Downloads</h2><p>Get prebuilt images from the <a href="https://github.com/jekader/7-segment-esp-clock/releases/latest" target="_blank" rel="noopener">latest GitHub release</a>, or return to the <a href="/">clock portal</a> to check for updates.</p></section>
</main><script>
function byId(id){return document.getElementById(id);}
function updateImageHelp(){var filesystem=byId('imageType').value==='filesystem';byId('firmwareHelp').hidden=filesystem;byId('filesystemHelp').hidden=!filesystem;}
byId('imageType').addEventListener('change',updateImageHelp);
byId('updateForm').addEventListener('submit',function(){byId('imageFile').name=byId('imageType').value;byId('updateButton').disabled=true;byId('imageType').disabled=true;byId('updateStatus').textContent='Upgrade in progress. Do not close this page or disconnect power until the clock reboots.';});
</script></body></html>)HTML";

const char kUpdateResultPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">{{REDIRECT}}<title>Update result · 7 Segment Clock</title><style>{{STYLES}}</style></head>
<body><main><section class="card"><h1>{{RESULT_TITLE}}</h1><p>{{RESULT_DETAIL}}</p><p><a href="/">Return to the clock portal</a></p></section></main></body></html>)HTML";

} // namespace

WebPortal::WebPortal(ConfigStore& config, Tm1650Display& display)
    : config_(config), display_(display) {}

void WebPortal::begin(StatusProvider statusProvider) {
    statusProvider_ = statusProvider;

    server_.on("/update", HTTP_GET, [this]() { handleUpdate(); });
    server_.on("/update", HTTP_POST, [this]() { handleUpdateResult(); },
               [this]() { handleUpdateUpload(); });
    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/wifi/networks", HTTP_GET, [this]() { handleWifiNetworks(); });
    server_.on("/api/brightness", HTTP_POST, [this]() { handleBrightness(); });
    server_.on("/api/display/power", HTTP_POST, [this]() { handlePower(); });
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
    page.replace("{{STYLES}}", FPSTR(kPageStyle));
    page.replace("{{VERSION}}", VERSION);
    page.replace("{{SSID}}", htmlEscape(config.ssid));
    page.replace("{{PASSWORD}}", htmlEscape(config.password));
    page.replace("{{TIMEZONE}}", htmlEscape(config.timezone));
    page.replace("{{MQTTHOST}}", htmlEscape(config.mqttHost));
    page.replace("{{MQTTPORT}}", String(config.mqttPort));
    page.replace("{{MQTTUSERNAME}}", htmlEscape(config.mqttUsername));
    page.replace("{{MQTTPASSWORD}}", htmlEscape(config.mqttPassword));
    page.replace("{{MQTTTOPIC}}", htmlEscape(config.mqttBaseTopic));
    page.replace("{{MQTTDISCOVERY}}", htmlEscape(config.mqttDiscoveryPrefix));
    page.replace("{{MQTTINTERVAL}}", String(config.mqttTelemetrySeconds));
    page.replace("{{MQTTENABLED}}", config.mqttEnabled ? "checked" : "");
    page.replace("{{NTP1}}", htmlEscape(config.ntpServer1));
    page.replace("{{NTP2}}", htmlEscape(config.ntpServer2));
    page.replace("{{SYNC}}", String(config.syncIntervalMinutes));
    page.replace("{{BRIGHTNESS}}", String(display_.brightness()));
    page.replace("{{HOUR12}}", config.use12Hour ? "checked" : "");
    page.replace("{{COLONBLINK}}", config.blinkColon ? "checked" : "");
    page.replace("{{PERIODICDATE}}", config.periodicDateEnabled ? "checked" : "");
    page.replace("{{DATEDURATION}}", String(config.dateDisplaySeconds));
    page.replace("{{DATEINTERVAL}}", String(config.dateIntervalSeconds));
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

void WebPortal::handleUpdate() {
    String page = FPSTR(kUpdatePage);
    page.replace("{{STYLES}}", FPSTR(kPageStyle));
    page.replace("{{VERSION}}", VERSION);
    server_.sendHeader("Cache-Control", "no-store, max-age=0");
    server_.send(200, "text/html", page);
}

void WebPortal::handleUpdateResult() {
    if (updateSucceeded_) {
        sendUpdateResult(true);
    } else {
        if (updaterError_.isEmpty()) updaterError_ = "No valid image was received.";
        sendUpdateResult(false);
    }
}

void WebPortal::sendUpdateResult(bool success) {
    String page = FPSTR(kUpdateResultPage);
    page.replace("{{STYLES}}", FPSTR(kPageStyle));
    page.replace("{{REDIRECT}}", success ? "<meta http-equiv=\"refresh\" content=\"15;url=/\">" : "");
    page.replace("{{RESULT_TITLE}}", success ? "Update complete" : "Update failed");
    page.replace("{{RESULT_DETAIL}}", success
        ? "The image was installed. The clock is restarting; allow it time to reconnect before returning to the portal."
        : "The image was not installed. " + htmlEscape(updaterError_));
    if (success) server_.client().setNoDelay(true);
    server_.send(success ? 200 : 500, "text/html", page);
    if (success) {
        delay(100);
        server_.client().stop();
        ESP.restart();
    }
}

void WebPortal::setUpdaterError() {
    StreamString error;
    Update.printError(error);
    updaterError_ = error.c_str();
    Update.printError(Serial);
}

void WebPortal::handleUpdateUpload() {
    HTTPUpload& upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
        updaterError_.clear();
        updateSucceeded_ = false;
        Serial.printf("ota: %s update started for %s; do not close page until reboot\n",
                      upload.name.c_str(), upload.filename.c_str());
        if (upload.name == "filesystem") {
            const size_t size = static_cast<size_t>(FS_end - FS_start);
            close_all_fs();
            if (!Update.begin(size, U_FS)) setUpdaterError();
        } else if (upload.name == "firmware") {
            const uint32_t size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(size, U_FLASH)) setUpdaterError();
        } else {
            updaterError_ = "Select Firmware or LittleFS filesystem before uploading.";
        }
    } else if (upload.status == UPLOAD_FILE_WRITE && updaterError_.isEmpty()) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) setUpdaterError();
    } else if (upload.status == UPLOAD_FILE_END && updaterError_.isEmpty()) {
        if (Update.end(true)) {
            updateSucceeded_ = true;
            Serial.printf("ota: %s update complete (%u bytes); restarting\n",
                          upload.name.c_str(), static_cast<unsigned>(upload.totalSize));
        } else {
            setUpdaterError();
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        updaterError_ = "Upload aborted before installation completed.";
        Serial.println(F("ota: update aborted"));
    }
    yield();
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

void WebPortal::handlePower() {
    if (!server_.hasArg("enabled")) {
        server_.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    const bool enabled = server_.arg("enabled") == "1";
    display_.setPower(enabled);
    const bool saved = config_.saveDisplayPower(enabled);
    server_.send(saved ? 200 : 500, "application/json", saved ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebPortal::handleSettings() {
    ClockConfig updated = config_.get();
    updated.ssid = server_.arg("ssid");
    updated.password = server_.arg("password");
    updated.mqttEnabled = server_.arg("mqttEnabled") == "1";
    updated.mqttHost = server_.arg("mqttHost");
    updated.mqttPort = server_.arg("mqttPort").toInt();
    updated.mqttUsername = server_.arg("mqttUsername");
    updated.mqttPassword = server_.arg("mqttPassword");
    updated.mqttBaseTopic = server_.arg("mqttBaseTopic");
    updated.mqttDiscoveryPrefix = server_.arg("mqttDiscoveryPrefix");
    updated.mqttTelemetrySeconds = server_.arg("mqttTelemetrySeconds").toInt();
    updated.ntpServer1 = server_.arg("ntp1");
    updated.ntpServer2 = server_.arg("ntp2");
    updated.timezone = server_.arg("timezone");
    updated.syncIntervalMinutes = server_.arg("sync").toInt();
    updated.use12Hour = server_.arg("use12Hour") == "1";
    updated.blinkColon = server_.arg("blinkColon") == "1";
    updated.periodicDateEnabled = server_.arg("periodicDate") == "1";
    updated.dateDisplaySeconds = server_.arg("dateDisplaySeconds").toInt();
    updated.dateIntervalSeconds = server_.arg("dateIntervalSeconds").toInt();
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
