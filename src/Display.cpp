#include "Display.h"


namespace {
constexpr uint8_t kSdaPin = 13;
constexpr uint8_t kSclPin = 12;

// Standard segment bits (pgfedcba), indexed by 7-bit ASCII. The lowercase
// glyphs are intentionally distinct where that improves readability.
const uint8_t kFont[128] PROGMEM = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0b00000000,0b00101000,0b00100010,0b01100011,0b01001001,0b00100100,0b01111110,0b00000010,
    0b00111001,0b00001111,0b00000001,0b01000010,0b00001100,0b01000000,0b00010000,0b01010010,
    0b00111111,0b00000110,0b01011011,0b01001111,0b01100110,0b01101101,0b01111101,0b00000111,
    0b01111111,0b01101111,0b00001001,0b00001010,0b01011000,0b01001000,0b01001100,0b01001011,
    0b00111011,0b01110111,0b01111100,0b00111001,0b01011110,0b01111001,0b01110001,0b00111101,
    0b01110110,0b00110000,0b00011110,0b01110101,0b00111100,0b01010101,0b00110111,0b00111111,
    0b01110011,0b01101011,0b00110011,0b01101101,0b01111000,0b00111110,0b01110010,0b01101010,
    0b00110110,0b01101110,0b01011011,0b00111001,0b01100100,0b00001111,0b00100011,0b00001000,
    0b00100000,0b01011111,0b01111100,0b01011000,0b01011110,0b01111011,0b01110001,0b01101111,
    0b01110100,0b00000101,0b00001101,0b01110101,0b00111000,0b01010101,0b01010100,0b01011100,
    0b01110011,0b01100111,0b01010000,0b01101101,0b01111000,0b00011100,0b01110010,0b01101010,
    0b00010100,0b00101110,0b01011011,0b01000110,0b00000110,0b01110000,0b01000001,0b01011101,
};

const char kMonthNames[] = "JaFeMrApMYJnJlAuSeOcNoDe";
}

void Tm1650Display::releasePin(uint8_t pin) {
    pinMode(pin, INPUT);
}

void Tm1650Display::drivePinLow(uint8_t pin) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
}

void Tm1650Display::startWireTransaction() {
    releasePin(kSdaPin);
    releasePin(kSclPin);
    delayMicroseconds(4);
    drivePinLow(kSdaPin);
    delayMicroseconds(4);
    drivePinLow(kSclPin);
}

void Tm1650Display::stopWireTransaction() {
    drivePinLow(kSdaPin);
    releasePin(kSclPin);
    delayMicroseconds(4);
    releasePin(kSdaPin);
    delayMicroseconds(4);
}

bool Tm1650Display::writeByte(uint8_t value) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        if (value & mask) releasePin(kSdaPin);
        else drivePinLow(kSdaPin);
        delayMicroseconds(4);
        releasePin(kSclPin);
        delayMicroseconds(4);
        drivePinLow(kSclPin);
    }

    // The board has external pull-ups. Release SDA and sample the ACK bit.
    releasePin(kSdaPin);
    delayMicroseconds(4);
    releasePin(kSclPin);
    delayMicroseconds(4);
    const bool acknowledged = digitalRead(kSdaPin) == LOW;
    drivePinLow(kSclPin);
    delayMicroseconds(4);
    return acknowledged;
}

bool Tm1650Display::writeRegister(uint8_t address, uint8_t value) {
    startWireTransaction();
    const bool addressAck = writeByte(address << 1);
    const bool dataAck = writeByte(value);
    stopWireTransaction();
    return addressAck && dataAck;
}

bool Tm1650Display::begin() {
    digitalWrite(kSdaPin, LOW);
    digitalWrite(kSclPin, LOW);
    releasePin(kSdaPin);
    releasePin(kSclPin);
    brightness_ = 8;
    power_ = false;
    const bool ok = writeControl();
    power_ = true;
    return writeControl() && ok;
}

bool Tm1650Display::writeControl() {
    // Level 8 is encoded as zero by the TM1650; levels 1..7 map directly.
    const uint8_t value = ((brightness_ & 0x07) << 4) | (power_ ? 1 : 0);
    return writeRegister(0x24, value);
}

void Tm1650Display::setBrightness(uint8_t level) {
    brightness_ = constrain(level, 1, 8);
    writeControl();
}

void Tm1650Display::setPower(bool enabled) {
    power_ = enabled;
    writeControl();
}

uint8_t Tm1650Display::glyph(char value) {
    const uint8_t index = static_cast<uint8_t>(value) & 0x7f;
    return pgm_read_byte(&kFont[index]);
}

uint8_t Tm1650Display::remapSegments(uint8_t value) {
    // Board wiring: A->F, B->P, C->C, D->D, E->E, F->G, G->B, P->A.
    return ((value & 0x01) << 5) |
           ((value & 0x02) << 6) |
           (value & 0x1c) |
           ((value & 0x20) << 1) |
           ((value & 0x40) >> 5) |
           ((value & 0x80) >> 7);
}

void Tm1650Display::showText(const String& text, uint8_t dots) {
    for (uint8_t digit = 0; digit < 4; ++digit) {
        uint8_t segments = digit < text.length() ? glyph(text[digit]) : 0;
        if (dots & (1 << digit)) segments |= 0x80;
        writeRegister(0x34 + digit, remapSegments(segments));
    }
}

void Tm1650Display::showTime(const struct tm& value, bool use12Hour, bool colonVisible) {
    int hour = value.tm_hour;
    if (use12Hour) {
        hour %= 12;
        if (hour == 0) hour = 12;
    }

    char text[5];
    snprintf(text, sizeof(text), "%2d%02d", hour, value.tm_min);
    uint8_t dots = colonVisible ? DisplayDot::Colon : DisplayDot::None;
    if (use12Hour && value.tm_hour >= 12) dots |= DisplayDot::Fourth;
    showText(text, dots);
}

void Tm1650Display::showDate(const struct tm& value) {
    const int month = constrain(value.tm_mon, 0, 11);
    char text[5];
    snprintf(text, sizeof(text), "%2d%c%c", value.tm_mday,
             kMonthNames[month * 2], kMonthNames[month * 2 + 1]);
    showText(text);
}

void DisplayController::showTime(const struct tm& value, bool use12Hour, bool colonVisible) {
    lastTime_ = value;
    hasClockContent_ = true;
    lastContentWasDate_ = false;
    use12Hour_ = use12Hour;
    colonVisible_ = colonVisible;
    if (!messageActive_) display_.showTime(value, use12Hour, colonVisible);
}

void DisplayController::showDate(const struct tm& value) {
    lastTime_ = value;
    hasClockContent_ = true;
    lastContentWasDate_ = true;
    if (!messageActive_) display_.showDate(value);
}

void DisplayController::showMessage(const String& text, uint32_t durationMs) {
    if (text.isEmpty()) {
        clearMessage();
        return;
    }

    scrollText_ = "    " + text + "    ";
    scrollOffset_ = 0;
    messageActive_ = true;
    nextScrollAt_ = millis() + 300;
    messageExpiresAt_ = durationMs == 0 ? 0 : millis() + durationMs;
    display_.showText("    ");
}

void DisplayController::clearMessage() {
    messageActive_ = false;
    scrollText_ = "";
    messageExpiresAt_ = 0;
    redrawClock();
}

void DisplayController::redrawClock() {
    if (!hasClockContent_) return;
    if (lastContentWasDate_) display_.showDate(lastTime_);
    else display_.showTime(lastTime_, use12Hour_, colonVisible_);
}

void DisplayController::update() {
    if (!messageActive_) return;
    const uint32_t now = millis();

    if (messageExpiresAt_ != 0 && static_cast<int32_t>(now - messageExpiresAt_) >= 0) {
        clearMessage();
        return;
    }

    if (static_cast<int32_t>(now - nextScrollAt_) < 0) return;
    nextScrollAt_ = now + 300;
    display_.showText(scrollText_.substring(scrollOffset_, scrollOffset_ + 4));
    ++scrollOffset_;
    if (scrollOffset_ + 4 > scrollText_.length()) scrollOffset_ = 0;
}
