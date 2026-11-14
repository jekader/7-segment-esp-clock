#include "RtcClock.h"

#include <Arduino.h>

namespace {
constexpr uint8_t kClockPin = 16;
constexpr uint8_t kDataPin = 14;
constexpr uint8_t kEnablePin = 5;
constexpr uint8_t kSignatureLength = 8;
const uint8_t kSignature[kSignatureLength] = {'7', 'S', 'E', 'G', 'c', 'l', 'k', '1'};
}

RtcClock::RtcClock() : rtc_(kClockPin, kDataPin, kEnablePin) {}

bool RtcClock::begin() {
    if (!rtc_.begin()) {
        Serial.println(F("rtc: initialization failed"));
        return false;
    }

    // Match the original 303WIFILC01 hardware setup: one diode, 2 kOhm.
    if (!rtc_.writeRegister(0x08, 0xA5)) {
        Serial.println(F("rtc: trickle-charge setup failed"));
        return false;
    }

    return true;
}

bool RtcClock::isValid() {
    if (!rtc_.isRunning()) return false;

    uint8_t signature[kSignatureLength];
    rtc_.readBufferRAM(signature, kSignatureLength);
    if (memcmp(signature, kSignature, kSignatureLength) != 0) return false;

    struct tm value {};
    return read(value) && value.tm_year >= 124 && value.tm_year <= 199;
}

bool RtcClock::read(struct tm& value) {
    return rtc_.read(&value);
}

bool RtcClock::write(const struct tm& value) {
    struct tm copy = value;
    if (!rtc_.clockEnable(true)) return false;
    rtc_.writeBufferRAM(const_cast<uint8_t*>(kSignature), kSignatureLength);
    return rtc_.write(&copy);
}
