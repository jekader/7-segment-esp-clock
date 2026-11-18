#include "MqttHomeAssistant.h"
#include "Version.h"

namespace {
constexpr uint32_t kReconnectDelayMs = 5000;
constexpr size_t kMqttBufferSize = 1024;
}

MqttHomeAssistant::MqttHomeAssistant(ConfigStore& config, Sensors& sensors,
                                     Tm1650Display& display)
    : config_(config), sensors_(sensors), display_(display), client_(networkClient_) {}

void MqttHomeAssistant::begin() {
    const ClockConfig& config = config_.get();
    enabled_ = config.mqttEnabled && !config.mqttHost.isEmpty();
    host_ = config.mqttHost;
    port_ = config.mqttPort;
    telemetryIntervalMs_ = config.mqttTelemetrySeconds * 1000UL;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    clientId_ = "7SEG-" + String(suffix);
    deviceId_ = "7seg_" + String(suffix);
    deviceId_.toLowerCase();
    deviceName_ = "7 Segment Clock " + String(suffix);

    rootTopic_ = config.mqttBaseTopic + "/" + deviceId_;
    availabilityTopic_ = rootTopic_ + "/status";
    telemetryTopic_ = rootTopic_ + "/tele/SENSOR";
    powerStateTopic_ = rootTopic_ + "/display/power";
    brightnessStateTopic_ = rootTopic_ + "/display/brightness";
    powerCommandTopic_ = powerStateTopic_ + "/set";
    brightnessCommandTopic_ = brightnessStateTopic_ + "/set";

    client_.setBufferSize(kMqttBufferSize);
    client_.setKeepAlive(60);
    client_.setSocketTimeout(2);
    if (enabled_) {
        client_.setServer(host_.c_str(), port_);
        client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
            handleMessage(topic, payload, length);
        });
        nextConnectAt_ = millis();
    }
}

String MqttHomeAssistant::deviceMetadata() const {
    return "\"device\":{\"identifiers\":[\"" + deviceId_ +
           "\"],\"name\":\"" + deviceName_ +
           "\",\"model\":\"303WIFILC01\",\"manufacturer\":\"DIY\",\"sw_version\":\"" VERSION "\"}";
}

void MqttHomeAssistant::connect() {
    const ClockConfig& config = config_.get();
    bool connected;
    if (config.mqttUsername.isEmpty()) {
        connected = client_.connect(clientId_.c_str(), availabilityTopic_.c_str(),
                                    0, true, "offline");
    } else {
        connected = client_.connect(clientId_.c_str(), config.mqttUsername.c_str(),
                                    config.mqttPassword.c_str(), availabilityTopic_.c_str(),
                                    0, true, "offline");
    }
    if (!connected) {
        Serial.printf("mqtt: connection failed (%d)\n", client_.state());
        return;
    }

    Serial.printf("mqtt: connected to %s:%u\n", host_.c_str(), port_);
    client_.publish(availabilityTopic_.c_str(), "online", true);
    client_.subscribe(powerCommandTopic_.c_str());
    client_.subscribe(brightnessCommandTopic_.c_str());
    displayStateKnown_ = false;
    publishDiscovery();
    publishDisplayState();
    publishTelemetry();
    lastTelemetryAt_ = millis();
}

void MqttHomeAssistant::publishSensorDiscovery(const char* objectId, const char* name,
                                                const char* valueKey, const char* unit,
                                                const char* deviceClass, const char* icon,
                                                bool available) {
    const ClockConfig& config = config_.get();
    String topic = config.mqttDiscoveryPrefix + "/sensor/" + deviceId_ + "/" + objectId + "/config";
    if (!available) {
        client_.publish(topic.c_str(), "", true);
        return;
    }

    String payload = "{\"name\":\"" + String(name) + "\",\"unique_id\":\"" + deviceId_ +
                     "_" + objectId + "\",\"state_topic\":\"" + telemetryTopic_ +
                     "\",\"value_template\":\"{{ value_json." + valueKey +
                     " }}\",\"unit_of_measurement\":\"" + unit +
                     "\",\"state_class\":\"measurement\"";
    if (deviceClass[0] != '\0') payload += ",\"device_class\":\"" + String(deviceClass) + "\"";
    if (icon[0] != '\0') payload += ",\"icon\":\"" + String(icon) + "\"";
    payload += ",\"availability_topic\":\"" + availabilityTopic_ +
               "\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"," +
               deviceMetadata() + "}";
    client_.publish(topic.c_str(), payload.c_str(), true);
    yield();
}

void MqttHomeAssistant::publishDiscovery() {
    const ClockConfig& config = config_.get();
    const String common = ",\"availability_topic\":\"" + availabilityTopic_ +
                          "\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"," +
                          deviceMetadata() + "}";

    String topic = config.mqttDiscoveryPrefix + "/switch/" + deviceId_ + "/display/config";
    String payload = "{\"name\":\"Display\",\"unique_id\":\"" + deviceId_ +
                     "_display\",\"state_topic\":\"" + powerStateTopic_ +
                     "\",\"command_topic\":\"" + powerCommandTopic_ +
                     "\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:clock-digital\"" +
                     common;
    client_.publish(topic.c_str(), payload.c_str(), true);
    yield();

    topic = config.mqttDiscoveryPrefix + "/number/" + deviceId_ + "/brightness/config";
    payload = "{\"name\":\"Brightness\",\"unique_id\":\"" + deviceId_ +
              "_brightness\",\"state_topic\":\"" + brightnessStateTopic_ +
              "\",\"command_topic\":\"" + brightnessCommandTopic_ +
              "\",\"min\":1,\"max\":8,\"step\":1,\"mode\":\"slider\",\"icon\":\"mdi:brightness-6\"" +
              common;
    client_.publish(topic.c_str(), payload.c_str(), true);
    yield();

    publishSensorDiscovery("temperature", "Temperature", "temperature", "°C", "temperature", "", sensors_.thpAvailable());
    publishSensorDiscovery("humidity", "Humidity", "humidity", "%", "humidity", "", sensors_.thpAvailable());
    publishSensorDiscovery("pressure", "Pressure", "pressure", "hPa", "pressure", "", sensors_.thpAvailable());
    publishSensorDiscovery("light_raw", "Light sensor (raw)", "lightRaw", "raw", "", "mdi:brightness-5", sensors_.autoBrightness());

    publishButtonTrigger("set", "PRESS");
    publishButtonTrigger("set", "HOLD");
    publishButtonTrigger("date", "PRESS");
    publishButtonTrigger("brightness", "PRESS");
}

void MqttHomeAssistant::publishTelemetry() {
    if (!sensors_.thpAvailable() && !sensors_.autoBrightness()) return;

    String payload = "{";
    bool hasValue = false;
    if (sensors_.thpAvailable()) {
        if (!isnan(sensors_.temperatureC())) {
            payload += "\"temperature\":" + String(sensors_.temperatureC(), 2);
            hasValue = true;
        }
        if (!isnan(sensors_.humidityPercent())) {
            if (hasValue) payload += ',';
            payload += "\"humidity\":" + String(sensors_.humidityPercent(), 1);
            hasValue = true;
        }
        if (!isnan(sensors_.pressureHpa())) {
            if (hasValue) payload += ',';
            payload += "\"pressure\":" + String(sensors_.pressureHpa(), 1);
            hasValue = true;
        }
    }
    if (sensors_.autoBrightness()) {
        if (hasValue) payload += ',';
        payload += "\"lightRaw\":" + String(sensors_.lightRaw());
        hasValue = true;
    }
    payload += "}";
    if (hasValue) client_.publish(telemetryTopic_.c_str(), payload.c_str(), true);
}

void MqttHomeAssistant::publishDisplayState() {
    const bool power = display_.powered();
    const uint8_t brightness = display_.brightness();
    client_.publish(powerStateTopic_.c_str(), power ? "ON" : "OFF", true);
    client_.publish(brightnessStateTopic_.c_str(), String(brightness).c_str(), true);
    lastPower_ = power;
    lastBrightness_ = brightness;
    displayStateKnown_ = true;
}

void MqttHomeAssistant::publishButton(const char* button, const char* event) {
    if (!enabled_ || !client_.connected()) return;
    const String topic = rootTopic_ + "/button/" + button;
    client_.publish(topic.c_str(), event, false);
}

void MqttHomeAssistant::publishButtonTrigger(const char* button, const char* event) {
    const ClockConfig& config = config_.get();
    String objectId = String(button) + "_";
    String type = "button_short_press";
    if (strcmp(event, "HOLD") == 0) {
        objectId += "hold";
        type = "button_long_press";
    } else {
        objectId += "press";
    }
    const String configTopic = config.mqttDiscoveryPrefix + "/device_automation/" + deviceId_ +
                               "/" + objectId + "/config";
    const String eventTopic = rootTopic_ + "/button/" + button;
    const String payload = "{\"automation_type\":\"trigger\",\"topic\":\"" + eventTopic +
                           "\",\"type\":\"" + type + "\",\"subtype\":\"" + button +
                           "\",\"payload\":\"" + event + "\"," + deviceMetadata() + "}";
    client_.publish(configTopic.c_str(), payload.c_str(), true);
    yield();
}

void MqttHomeAssistant::handleMessage(const char* topic, const uint8_t* payload,
                                       unsigned int length) {
    if (powerCommandTopic_ == topic) {
        const bool on = length == 2 && payload[0] == 'O' && payload[1] == 'N';
        const bool off = length == 3 && payload[0] == 'O' && payload[1] == 'F' && payload[2] == 'F';
        if (!on && !off) return;
        display_.setPower(on);
        if (!config_.saveDisplayPower(on)) Serial.println(F("mqtt: failed to save display power"));
        return;
    }

    if (brightnessCommandTopic_ == topic) {
        if (config_.get().sensorOverride && config_.get().autoBrightness) return;
        if (length == 0 || length > 2) return;
        char text[3];
        memcpy(text, payload, length);
        text[length] = '\0';
        char* end = nullptr;
        const long level = strtol(text, &end, 10);
        if (end == text || *end != '\0' || level < 1 || level > 8) return;
        display_.setBrightness(static_cast<uint8_t>(level));
        if (!config_.saveBrightness(static_cast<uint8_t>(level))) {
            Serial.println(F("mqtt: failed to save brightness"));
        }
    }
}

void MqttHomeAssistant::handle() {
    if (!enabled_) return;
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
        nextConnectAt_ = now + kReconnectDelayMs;
        return;
    }

    if (!client_.connected()) {
        if (static_cast<int32_t>(now - nextConnectAt_) < 0) return;
        nextConnectAt_ = now + kReconnectDelayMs;
        connect();
        if (!client_.connected()) return;
    }

    client_.loop();
    if (client_.connected()) {
        if (!displayStateKnown_ || lastPower_ != display_.powered() ||
            lastBrightness_ != display_.brightness()) {
            publishDisplayState();
        }
        if (now - lastTelemetryAt_ >= telemetryIntervalMs_) {
            lastTelemetryAt_ = now;
            publishTelemetry();
        }
    }
}
