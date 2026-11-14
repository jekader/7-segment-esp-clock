#pragma once

#include "Config.h"

#include <Adafruit_BME280.h>

// Sensor hardware enabled by the board-override setting. The BME280 uses the
// standard Wire bus on UP/GPIO4 (SDA) and P1.RX/GPIO3 (SCL); the LDR uses A0.
class Sensors {
public:
    bool begin(const ClockConfig& config);
    void update();
    uint8_t brightnessLevel() const;

    bool autoBrightness() const { return autoBrightness_; }
    uint16_t lightRaw() const { return lightRaw_; }
    bool thpEnabled() const { return thpEnabled_; }
    bool thpAvailable() const { return thpAvailable_; }
    float temperatureC() const { return temperatureC_; }
    float humidityPercent() const { return humidityPercent_; }
    float pressureHpa() const { return pressureHpa_; }

private:
    Adafruit_BME280 bme_;
    bool autoBrightness_ = false;
    bool thpEnabled_ = false;
    bool thpAvailable_ = false;
    uint16_t lightDark_ = 20;
    uint16_t lightBright_ = 800;
    uint16_t lightRaw_ = 0;
    float filteredLight_ = 0;
    bool lightFilterReady_ = false;
    uint32_t lastLightSampleAt_ = 0;
    uint32_t lastThpReadAt_ = 0;
    float temperatureC_ = 0;
    float humidityPercent_ = 0;
    float pressureHpa_ = 0;
};
