#pragma once

#include <ErriezDS1302.h>
#include <time.h>

class RtcClock {
public:
    RtcClock();

    bool begin();
    bool isValid();
    bool read(struct tm& value);
    bool write(const struct tm& value);

private:
    ErriezDS1302 rtc_;
};
