#pragma once

#include <Arduino.h>

struct ClockConfig {
    String ssid;
    String password;
    String ntpServer1 = "pool.ntp.org";
    String ntpServer2 = "time.nist.gov";
    String timezone = "UTC0";
    uint32_t syncIntervalMinutes = 60;
    bool use12Hour = false;
    bool blinkColon = true;
    uint8_t brightness = 8;
    bool sensorOverride = false;
    bool autoBrightness = false;
    bool thpSensorEnabled = false;
    uint16_t lightDark = 20;
    uint16_t lightBright = 800;
};

class ConfigStore {
public:
    bool begin();
    const ClockConfig& get() const { return config_; }
    bool save(const ClockConfig& config);
    bool saveBrightness(uint8_t brightness);

private:
    static constexpr const char* kPath = "/config.txt";
    static constexpr const char* kTempPath = "/config.tmp";

    ClockConfig config_;

    void load();
    static void sanitize(ClockConfig& config);
};
