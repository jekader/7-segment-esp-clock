#pragma once

#include "Config.h"
#include "Display.h"

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <functional>

class WebPortal {
public:
    using StatusProvider = std::function<String()>;

    WebPortal(ConfigStore& config, Tm1650Display& display);

    void begin(StatusProvider statusProvider);
    void handle();
    void startAccessPoint();
    void stopAccessPoint();
    bool accessPointActive() const { return apActive_; }
    String accessPointName() const { return apName_; }

private:
    ConfigStore& config_;
    Tm1650Display& display_;
    ESP8266WebServer server_{80};
    DNSServer dnsServer_;
    StatusProvider statusProvider_;
    bool apActive_ = false;
    uint32_t restartAt_ = 0;
    String apName_;
    String updaterError_;
    bool updateSucceeded_ = false;

    void handleRoot();
    void handleUpdate();
    void handleUpdateResult();
    void handleUpdateUpload();
    void handleStatus();
    void handleWifiNetworks();
    void handleBrightness();
    void handlePower();
    void handleSettings();
    void handleFactoryReset();
    void handleRestart();
    void handleCaptiveRedirect();
    void scheduleRestart();
    void sendUpdateResult(bool success);
    void setUpdaterError();
    static String htmlEscape(const String& value);
    static String jsonEscape(const String& value);
};
