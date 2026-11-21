#include "Config.h"

#include <LittleFS.h>

namespace {

void assignIfPresent(ClockConfig& config, const String& key, const String& value) {
    if (key == "ssid") config.ssid = value;
    else if (key == "password") config.password = value;
    else if (key == "mqtt_enabled") config.mqttEnabled = value == "1";
    else if (key == "mqtt_host") config.mqttHost = value;
    else if (key == "mqtt_port") config.mqttPort = value.toInt();
    else if (key == "mqtt_username") config.mqttUsername = value;
    else if (key == "mqtt_password") config.mqttPassword = value;
    else if (key == "mqtt_topic") config.mqttBaseTopic = value;
    else if (key == "mqtt_discovery") config.mqttDiscoveryPrefix = value;
    else if (key == "mqtt_interval") config.mqttTelemetrySeconds = value.toInt();
    else if (key == "ntp1") config.ntpServer1 = value;
    else if (key == "ntp2") config.ntpServer2 = value;
    else if (key == "timezone") config.timezone = value;
    else if (key == "sync_minutes") config.syncIntervalMinutes = value.toInt();
    else if (key == "hour_12") config.use12Hour = value == "1";
    else if (key == "colon_blink") config.blinkColon = value == "1";
    else if (key == "periodic_date") config.periodicDateEnabled = value == "1";
    else if (key == "day_of_week") config.showDayOfWeek = value == "1";
    else if (key == "year_first") config.dateYearFirst = value == "1";
    else if (key == "date_seconds") config.dateDisplaySeconds = value.toInt();
    else if (key == "date_interval") config.dateIntervalSeconds = value.toInt();
    else if (key == "display_power") config.displayPower = value == "1";
    else if (key == "brightness") config.brightness = value.toInt();
    else if (key == "sensor_override") config.sensorOverride = value == "1";
    else if (key == "auto_brightness") config.autoBrightness = value == "1";
    else if (key == "thp_sensor") config.thpSensorEnabled = value == "1";
    else if (key == "light_dark") config.lightDark = value.toInt();
    else if (key == "light_bright") config.lightBright = value.toInt();
}

} // namespace

bool ConfigStore::begin() {
    if (!LittleFS.begin()) {
        Serial.println(F("config: LittleFS mount failed; using defaults"));
        return false;
    }

    load();
    sanitize(config_);
    return true;
}

void ConfigStore::load() {
    config_ = ClockConfig{};
    File file = LittleFS.open(kPath, "r");
    if (!file) {
        Serial.println(F("config: no saved configuration; using defaults"));
        return;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        const int separator = line.indexOf('=');
        if (separator <= 0) continue;
        assignIfPresent(config_, line.substring(0, separator), line.substring(separator + 1));
    }
}

void ConfigStore::sanitize(ClockConfig& config) {
    if (config.ntpServer1.isEmpty()) config.ntpServer1 = "pool.ntp.org";
    if (config.timezone.isEmpty()) config.timezone = "UTC0";
    config.syncIntervalMinutes = constrain(config.syncIntervalMinutes, 5UL, 1440UL);
    config.dateDisplaySeconds = constrain(config.dateDisplaySeconds, 1UL, 60UL);
    config.dateIntervalSeconds = constrain(config.dateIntervalSeconds, 10UL, 86400UL);
    config.dateDisplaySeconds = min(config.dateDisplaySeconds, config.dateIntervalSeconds);
    config.mqttTelemetrySeconds = constrain(config.mqttTelemetrySeconds, 5UL, 3600UL);
    if (config.mqttPort == 0) config.mqttPort = 1883;
    config.mqttHost.trim();
    config.mqttBaseTopic.trim();
    config.mqttDiscoveryPrefix.trim();
    while (config.mqttBaseTopic.endsWith("/")) config.mqttBaseTopic.remove(config.mqttBaseTopic.length() - 1);
    while (config.mqttDiscoveryPrefix.endsWith("/")) config.mqttDiscoveryPrefix.remove(config.mqttDiscoveryPrefix.length() - 1);
    if (config.mqttBaseTopic.isEmpty()) config.mqttBaseTopic = "7segclock";
    if (config.mqttDiscoveryPrefix.isEmpty()) config.mqttDiscoveryPrefix = "homeassistant";
    if (config.mqttHost.isEmpty()) config.mqttEnabled = false;
    config.brightness = constrain(config.brightness, 1, 8);
    config.lightDark = constrain(config.lightDark, 0, 1023);
    config.lightBright = constrain(config.lightBright, 1, 1023);
    if (config.lightDark >= config.lightBright) {
        config.lightDark = 20;
        config.lightBright = 800;
    }

    // Fixed hardware/protocol limits. Invalid values are discarded rather than
    // being truncated into credentials or hostnames the user did not enter.
    if (config.ssid.length() > 32) config.ssid.clear();
    if (config.password.length() > 64) config.password.clear();
    if (config.mqttHost.length() > 63) {
        config.mqttHost.clear();
        config.mqttEnabled = false;
    }
    if (config.mqttUsername.length() > 64) config.mqttUsername.clear();
    if (config.mqttPassword.length() > 64) config.mqttPassword.clear();
    if (config.mqttBaseTopic.length() > 48) config.mqttBaseTopic = "7segclock";
    if (config.mqttDiscoveryPrefix.length() > 48) config.mqttDiscoveryPrefix = "homeassistant";
    if (config.ntpServer1.length() > 63) config.ntpServer1 = "pool.ntp.org";
    if (config.ntpServer2.length() > 63) config.ntpServer2.clear();
    if (config.timezone.length() > 63) config.timezone = "UTC0";
}

bool ConfigStore::save(const ClockConfig& config) {
    ClockConfig clean = config;
    sanitize(clean);

    File file = LittleFS.open(kTempPath, "w");
    if (!file) return false;

    file.print("ssid="); file.println(clean.ssid);
    file.print("password="); file.println(clean.password);
    file.print("mqtt_enabled="); file.println(clean.mqttEnabled ? 1 : 0);
    file.print("mqtt_host="); file.println(clean.mqttHost);
    file.print("mqtt_port="); file.println(clean.mqttPort);
    file.print("mqtt_username="); file.println(clean.mqttUsername);
    file.print("mqtt_password="); file.println(clean.mqttPassword);
    file.print("mqtt_topic="); file.println(clean.mqttBaseTopic);
    file.print("mqtt_discovery="); file.println(clean.mqttDiscoveryPrefix);
    file.print("mqtt_interval="); file.println(clean.mqttTelemetrySeconds);
    file.print("ntp1="); file.println(clean.ntpServer1);
    file.print("ntp2="); file.println(clean.ntpServer2);
    file.print("timezone="); file.println(clean.timezone);
    file.print("sync_minutes="); file.println(clean.syncIntervalMinutes);
    file.print("hour_12="); file.println(clean.use12Hour ? 1 : 0);
    file.print("colon_blink="); file.println(clean.blinkColon ? 1 : 0);
    file.print("periodic_date="); file.println(clean.periodicDateEnabled ? 1 : 0);
    file.print("day_of_week="); file.println(clean.showDayOfWeek ? 1 : 0);
    file.print("year_first="); file.println(clean.dateYearFirst ? 1 : 0);
    file.print("date_seconds="); file.println(clean.dateDisplaySeconds);
    file.print("date_interval="); file.println(clean.dateIntervalSeconds);
    file.print("display_power="); file.println(clean.displayPower ? 1 : 0);
    file.print("brightness="); file.println(clean.brightness);
    file.print("sensor_override="); file.println(clean.sensorOverride ? 1 : 0);
    file.print("auto_brightness="); file.println(clean.autoBrightness ? 1 : 0);
    file.print("thp_sensor="); file.println(clean.thpSensorEnabled ? 1 : 0);
    file.print("light_dark="); file.println(clean.lightDark);
    file.print("light_bright="); file.println(clean.lightBright);
    file.flush();
    file.close();

    if (!LittleFS.rename(kTempPath, kPath)) {
        LittleFS.remove(kTempPath);
        return false;
    }

    config_ = clean;
    return true;
}

bool ConfigStore::saveBrightness(uint8_t brightness) {
    ClockConfig updated = config_;
    updated.brightness = constrain(brightness, 1, 8);
    return save(updated);
}

bool ConfigStore::factoryReset() {
    if (!LittleFS.format()) return false;
    config_ = ClockConfig{};
    sanitize(config_);
    return true;
}

bool ConfigStore::saveDisplayPower(bool enabled) {
    ClockConfig updated = config_;
    updated.displayPower = enabled;
    return save(updated);
}
