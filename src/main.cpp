#include "Buttons.h"
#include "Config.h"
#include "Display.h"
#include "RtcClock.h"
#include "Sensors.h"
#include "WebPortal.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <coredecls.h>
#include <time.h>

namespace {
constexpr uint8_t kStatusLedPin = 2;
constexpr uint32_t kWifiFailurePortalDelayMs = 30000;
constexpr time_t kMinimumValidTime = 1704067200; // 2024-01-01 00:00:00 UTC

ConfigStore configStore;
Tm1650Display hardwareDisplay;
DisplayController display(hardwareDisplay);
RtcClock rtc;
Buttons buttons;
Sensors sensors;
WebPortal portal(configStore, hardwareDisplay);

volatile bool ntpSyncPending = false;
uint32_t ntpUpdateDelayMs = 60UL * 60UL * 1000UL;
uint32_t disconnectedSince = 0;
time_t lastNtpSync = 0;
bool rtcValid = false;
bool showIpAfterBoot = true;
bool showDate = false;
uint32_t lastRenderAt = 0;
uint32_t lastAutoBrightnessAt = 0;

String jsonEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c == '\"' || c == '\\') escaped += '\\';
        if (c == '\r' || c == '\n') escaped += ' ';
        else escaped += c;
    }
    return escaped;
}

String formatTime(time_t value) {
    if (value < kMinimumValidTime) return "Waiting for time";
    struct tm local {};
    localtime_r(&value, &local);
    char text[24];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
    return String(text);
}

String statusJson() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    const time_t now = time(nullptr);
    String json = "{";
    json += "\"time\":\"" + jsonEscape(formatTime(now)) + "\",";
    json += "\"wifi\":" + String(connected ? "true" : "false") + ",";
    json += "\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",";
    json += "\"ip\":\"" + (connected ? WiFi.localIP().toString() : "") + "\",";
    json += "\"lastSync\":\"" + (lastNtpSync ? jsonEscape(formatTime(lastNtpSync)) : "Never this boot") + "\",";
    json += "\"rtcValid\":" + String(rtcValid ? "true" : "false") + ",";
    json += "\"brightness\":" + String(hardwareDisplay.brightness()) + ",";
    json += "\"sensorsEnabled\":" + String(configStore.get().sensorOverride ? "true" : "false") + ",";
    json += "\"autoBrightness\":" + String(sensors.autoBrightness() ? "true" : "false") + ",";
    json += "\"lightRaw\":" + String(sensors.lightRaw()) + ",";
    json += "\"thpEnabled\":" + String(sensors.thpEnabled() ? "true" : "false") + ",";
    json += "\"thpAvailable\":" + String(sensors.thpAvailable() ? "true" : "false") + ",";
    json += "\"temperature\":";
    json += sensors.thpAvailable() ? String(sensors.temperatureC(), 2) : "null";
    json += ",\"humidity\":";
    json += sensors.thpAvailable() ? String(sensors.humidityPercent(), 1) : "null";
    json += ",\"pressure\":";
    json += sensors.thpAvailable() ? String(sensors.pressureHpa(), 1) : "null";
    json += ",";
    json += "\"ap\":" + String(portal.accessPointActive() ? "true" : "false") + ",";
    json += "\"apName\":\"" + jsonEscape(portal.accessPointName()) + "\",";
    json += "\"apIp\":\"" + (portal.accessPointActive() ? WiFi.softAPIP().toString() : "") + "\",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"otaSpace\":" + String(ESP.getFreeSketchSpace());
    json += "}";
    return json;
}

void restoreTimeFromRtc() {
    if (!rtc.isValid()) {
        Serial.println(F("rtc: no valid saved time"));
        return;
    }

    struct tm saved {};
    if (!rtc.read(saved)) return;
    saved.tm_isdst = -1;
    const time_t value = mktime(&saved);
    if (value < kMinimumValidTime) return;

    const timeval tv = {value, 0};
    settimeofday(&tv, nullptr);
    rtcValid = true;
    Serial.printf("rtc: restored %s\n", formatTime(value).c_str());
}

void startWifi() {
    const ClockConfig& config = configStore.get();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.hostname("7-segment-clock");

    if (config.ssid.isEmpty()) {
        Serial.println(F("wifi: no credentials; starting configuration AP"));
        portal.startAccessPoint();
        return;
    }

    // Preserve a manually requested configuration AP while joining station mode.
    WiFi.mode(portal.accessPointActive() ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    disconnectedSince = millis();
    Serial.printf("wifi: connecting to %s\n", config.ssid.c_str());
}
}

// The ESP8266 core calls this weak hook when configuring its SNTP client.
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000() {
    return ntpUpdateDelayMs < 15000UL ? 15000UL : ntpUpdateDelayMs;
}

void setup() {
    Serial.begin(115200);
    Serial.printf("\n7 Segment Clock %s (%s %s)\n", VERSION, __DATE__, __TIME__);

    pinMode(kStatusLedPin, OUTPUT);
    digitalWrite(kStatusLedPin, HIGH);

    configStore.begin();
    const ClockConfig& config = configStore.get();
    ntpUpdateDelayMs = config.syncIntervalMinutes * 60UL * 1000UL;

    if (!hardwareDisplay.begin()) Serial.println(F("display: TM1650 not responding"));
    hardwareDisplay.setBrightness(config.brightness);
    hardwareDisplay.showText("7SEG");

    buttons.begin();
    sensors.begin(config);
    rtcValid = rtc.begin();
    setTZ(config.timezone.c_str());
    if (rtcValid) restoreTimeFromRtc();

    portal.begin(statusJson);
    if (buttons.setIsDown()) portal.startAccessPoint();
    startWifi();

    configTime(config.timezone.c_str(), config.ntpServer1.c_str(),
               config.ntpServer2.isEmpty() ? nullptr : config.ntpServer2.c_str());
    settimeofday_cb([]() { ntpSyncPending = true; });

    Serial.println(F("setup: complete"));
    Serial.printf("system: free heap %u bytes, OTA space %u bytes\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreeSketchSpace()));
}

void loop() {
    const uint32_t nowMs = millis();
    buttons.update();
    portal.handle();
    display.update();
    sensors.update();

    if (buttons.setWasHeld()) portal.startAccessPoint();

    const bool datePressed = buttons.wasPressed(Buttons::Date);
    const bool brightnessPressed = buttons.wasPressed(Buttons::Brightness);
    if (datePressed && !configStore.get().sensorOverride) showDate = !showDate;

    if (brightnessPressed && !configStore.get().sensorOverride) {
        const uint8_t level = hardwareDisplay.brightness() % 8 + 1;
        hardwareDisplay.setBrightness(level);
        configStore.saveBrightness(level);
    }

    if (sensors.autoBrightness() && nowMs - lastAutoBrightnessAt >= 2000) {
        lastAutoBrightnessAt = nowMs;
        hardwareDisplay.setBrightness(sensors.brightnessLevel());
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected) {
        if (disconnectedSince != 0) {
            const String ip = WiFi.localIP().toString();
            Serial.printf("wifi: connected, IP %s\n", ip.c_str());
            if (showIpAfterBoot) {
                display.showMessage("IP " + ip, 12000);
                showIpAfterBoot = false;
            }
            disconnectedSince = 0;
        }
        portal.stopAccessPoint();
    } else {
        if (disconnectedSince == 0) disconnectedSince = nowMs;
        if (nowMs - disconnectedSince >= kWifiFailurePortalDelayMs) portal.startAccessPoint();
    }
    digitalWrite(kStatusLedPin, connected && !portal.accessPointActive() ? HIGH : LOW);

    if (ntpSyncPending) {
        ntpSyncPending = false;
        const time_t now = time(nullptr);
        if (now >= kMinimumValidTime) {
            struct tm local {};
            localtime_r(&now, &local);
            if (rtcValid && rtc.write(local)) Serial.println(F("rtc: synchronized from NTP"));
            else if (rtcValid) Serial.println(F("rtc: NTP write failed"));
            lastNtpSync = now;
        }
    }

    if (nowMs - lastRenderAt >= 200) {
        lastRenderAt = nowMs;
        const time_t now = time(nullptr);
        if (now >= kMinimumValidTime) {
            struct tm local {};
            localtime_r(&now, &local);
            if (showDate) display.showDate(local);
            else display.showTime(local, configStore.get().use12Hour,
                                  !configStore.get().blinkColon || (nowMs / 500) % 2 == 0);
        } else {
            hardwareDisplay.showText(portal.accessPointActive() ? "SET" : "NtP");
        }
    }

    delay(1);
}
