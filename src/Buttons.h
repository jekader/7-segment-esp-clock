#pragma once

#include <Arduino.h>

class Buttons {
public:
    enum Button : uint8_t {
        Set = 1 << 0,
        Date = 1 << 1,
        Brightness = 1 << 2,
    };

    void begin();
    void update();
    bool wasPressed(Button button);
    bool setWasHeld();
    bool setIsDown() const { return stableState_ & Set; }

private:
    static constexpr uint32_t kDebounceMs = 30;
    static constexpr uint32_t kSetHoldMs = 3000;

    uint8_t readRaw() const;

    uint8_t rawState_ = 0;
    uint8_t stableState_ = 0;
    uint8_t pressedEvents_ = 0;
    uint32_t rawChangedAt_ = 0;
    uint32_t setPressedAt_ = 0;
    bool setHoldSent_ = false;
};
