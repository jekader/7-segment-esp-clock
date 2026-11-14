#include "Buttons.h"

namespace {
constexpr uint8_t kSetPin = 0;
constexpr uint8_t kDatePin = 4;
constexpr uint8_t kBrightnessPin = 15;
}

void Buttons::begin() {
    pinMode(kSetPin, INPUT_PULLUP);
    pinMode(kDatePin, INPUT_PULLUP);
    pinMode(kBrightnessPin, INPUT);
    rawState_ = stableState_ = readRaw();
    rawChangedAt_ = setPressedAt_ = millis();
    setHoldSent_ = !(stableState_ & Set);
}

uint8_t Buttons::readRaw() const {
    uint8_t state = 0;
    if (digitalRead(kSetPin) == LOW) state |= Set;
    if (digitalRead(kDatePin) == LOW) state |= Date;
    if (digitalRead(kBrightnessPin) == HIGH) state |= Brightness;
    return state;
}

void Buttons::update() {
    const uint32_t now = millis();
    const uint8_t raw = readRaw();
    if (raw != rawState_) {
        rawState_ = raw;
        rawChangedAt_ = now;
    }

    if (raw != stableState_ && now - rawChangedAt_ >= kDebounceMs) {
        const uint8_t previous = stableState_;
        stableState_ = raw;
        pressedEvents_ |= stableState_ & ~previous;

        if ((stableState_ & Set) && !(previous & Set)) {
            setPressedAt_ = now;
            setHoldSent_ = false;
        }
    }

    if ((stableState_ & Set) && !setHoldSent_ && now - setPressedAt_ >= kSetHoldMs) {
        setHoldSent_ = true;
    }
}

bool Buttons::wasPressed(Button button) {
    const bool pressed = pressedEvents_ & button;
    pressedEvents_ &= ~button;
    return pressed;
}

bool Buttons::setWasHeld() {
    if (!setHoldSent_) return false;
    setHoldSent_ = false;
    return true;
}
