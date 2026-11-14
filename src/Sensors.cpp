#include "Sensors.h"

#include <Wire.h>

namespace {
constexpr uint8_t kBmeSdaPin = 4;
constexpr uint8_t kBmeSclPin = 3;
constexpr uint32_t kLightSampleIntervalMs = 250;
constexpr uint32_t kThpReadIntervalMs = 30000;
}

bool Sensors::begin(const ClockConfig& config) {
    autoBrightness_ = config.sensorOverride && config.autoBrightness;
    thpEnabled_ = config.sensorOverride && config.thpSensorEnabled;
    lightDark_ = config.lightDark;
    lightBright_ = config.lightBright;

    if (!thpEnabled_) return true;

    Wire.begin(kBmeSdaPin, kBmeSclPin);
    Wire.setClock(100000);
    thpAvailable_ = bme_.begin(0x76, &Wire) || bme_.begin(0x77, &Wire);
    Serial.println(thpAvailable_ ? F("sensors: BME280 ready") : F("sensors: BME280 not found"));
    if (thpAvailable_) {
        temperatureC_ = bme_.readTemperature();
        humidityPercent_ = bme_.readHumidity();
        pressureHpa_ = bme_.readPressure() / 100.0f;
        lastThpReadAt_ = millis();
    }
    return thpAvailable_;
}

void Sensors::update() {
    const uint32_t now = millis();
    if (autoBrightness_ && now - lastLightSampleAt_ >= kLightSampleIntervalMs) {
        lastLightSampleAt_ = now;
        lightRaw_ = analogRead(A0);
        if (!lightFilterReady_) {
            filteredLight_ = lightRaw_;
            lightFilterReady_ = true;
        } else {
            filteredLight_ = filteredLight_ * 0.875f + lightRaw_ * 0.125f;
        }
    }

    if (thpAvailable_ && now - lastThpReadAt_ >= kThpReadIntervalMs) {
        lastThpReadAt_ = now;
        temperatureC_ = bme_.readTemperature();
        humidityPercent_ = bme_.readHumidity();
        pressureHpa_ = bme_.readPressure() / 100.0f;
    }
}

uint8_t Sensors::brightnessLevel() const {
    const int32_t filtered = static_cast<int32_t>(filteredLight_ + 0.5f);
    const int32_t bounded = constrain(filtered, static_cast<int32_t>(lightDark_), static_cast<int32_t>(lightBright_));
    return map(bounded, lightDark_, lightBright_, 1, 8);
}
