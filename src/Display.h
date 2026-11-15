#pragma once

#include <Arduino.h>
#include <time.h>

namespace DisplayDot {
constexpr uint8_t None = 0;
constexpr uint8_t First = 1 << 0;
constexpr uint8_t Colon = 1 << 1;
constexpr uint8_t Third = 1 << 2;
constexpr uint8_t Fourth = 1 << 3;
}

// Hardware adapter for the 303WIFILC01's four-digit TM1650 display.
// showText accepts printable ASCII and is also the low-level path for future
// message sources such as MQTT.
class Tm1650Display {
public:
    bool begin();
    void setBrightness(uint8_t level);
    uint8_t brightness() const { return brightness_; }
    void setPower(bool enabled);
    bool powered() const { return power_; }
    void showText(const String& text, uint8_t dots = DisplayDot::None);
    void showTime(const struct tm& value, bool use12Hour, bool colonVisible);
    void showDate(const struct tm& value);

private:
    uint8_t brightness_ = 8;
    bool power_ = true;

    bool writeControl();
    bool writeRegister(uint8_t address, uint8_t value);
    bool writeByte(uint8_t value);
    void startWireTransaction();
    void stopWireTransaction();
    static void releasePin(uint8_t pin);
    static void drivePinLow(uint8_t pin);
    static uint8_t remapSegments(uint8_t segments);
    static uint8_t glyph(char value);
};

// Owns display content policy. A future MQTT adapter only needs showMessage();
// clock rendering resumes automatically after a timed message.
class DisplayController {
public:
    explicit DisplayController(Tm1650Display& display) : display_(display) {}

    void showTime(const struct tm& value, bool use12Hour, bool colonVisible);
    void showDate(const struct tm& value);
    void showMessage(const String& text, uint32_t durationMs = 10000);
    void clearMessage();
    bool messageActive() const { return messageActive_; }
    void update();

private:
    Tm1650Display& display_;
    String scrollText_;
    size_t scrollOffset_ = 0;
    uint32_t nextScrollAt_ = 0;
    uint32_t messageExpiresAt_ = 0;
    bool messageActive_ = false;

    struct tm lastTime_ {};
    bool hasClockContent_ = false;
    bool lastContentWasDate_ = false;
    bool use12Hour_ = false;
    bool colonVisible_ = false;

    void redrawClock();
};
