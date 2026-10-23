#pragma once

#include <cstdint>

// Sony's control protocol reports the common battery range as steps 0..10,
// where each step represents ten percentage points. Preserve larger values
// for models/firmware that already return a literal percentage.
constexpr int decodeSonyBatteryPercentage(uint8_t raw)
{
    if (raw <= 10)
        return static_cast<int>(raw) * 10;
    if (raw <= 100)
        return static_cast<int>(raw);
    return -1;
}
