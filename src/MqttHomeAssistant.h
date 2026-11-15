#pragma once

#include "Config.h"
#include "Display.h"
#include "Sensors.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

// Non-blocking MQTT/Home Assistant integration. Discovery is sent on every
// broker connection so a restarted Home Assistant can recover all entities.
class MqttHomeAssistant {
public:
    MqttHomeAssistant(ConfigStore& config, Sensors& sensors, Tm1650Display& display);

    void begin();
    void handle();
    void publishButton(const char* button, const char* event);

private:
    ConfigStore& config_;
    Sensors& sensors_;
    Tm1650Display& display_;
    WiFiClient networkClient_;
    PubSubClient client_;

    bool enabled_ = false;
    String host_;
    uint16_t port_ = 1883;
    uint32_t telemetryIntervalMs_ = 30000;
    String clientId_;
    String deviceId_;
    String deviceName_;
    String rootTopic_;
    String availabilityTopic_;
    String telemetryTopic_;
    String powerCommandTopic_;
    String brightnessCommandTopic_;
    String powerStateTopic_;
    String brightnessStateTopic_;
    uint32_t nextConnectAt_ = 0;
    uint32_t lastTelemetryAt_ = 0;
    bool displayStateKnown_ = false;
    bool lastPower_ = false;
    uint8_t lastBrightness_ = 0;

    void connect();
    void publishDiscovery();
    void publishSensorDiscovery(const char* objectId, const char* name,
                                const char* valueKey, const char* unit,
                                const char* deviceClass, const char* icon,
                                bool available);
    void publishTelemetry();
    void publishDisplayState();
    void publishButtonTrigger(const char* button, const char* event);
    void handleMessage(const char* topic, const uint8_t* payload, unsigned int length);
    String deviceMetadata() const;
};
