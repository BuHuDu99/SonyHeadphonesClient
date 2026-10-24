#pragma once

#include <cstdint>
#include <string>

// MDR battery payloads carry literal percentages, including values below 10%.
constexpr int decodeSonyBatteryPercentage(uint8_t raw)
{
    return raw <= 100 ? static_cast<int>(raw) : -1;
}

enum class SonyBatteryLayout { Unknown, Single, Dual };

struct SonyBatteryDisplay {
    std::string compact = "--";
    std::string description = "Waiting for battery status";
};

inline SonyBatteryDisplay formatSonyBattery(SonyBatteryLayout layout, int singleOrLeft, int right, int batteryCase)
{
    const auto percent = [](int value) {
        return value >= 0 && value <= 100 ? std::to_string(value) + "%" : std::string("--");
    };
    if (layout == SonyBatteryLayout::Single) {
        const auto value = percent(singleOrLeft);
        return {value, "Battery: " + value};
    }
    if (layout == SonyBatteryLayout::Dual) {
        const auto left = percent(singleOrLeft);
        const auto rightText = percent(right);
        SonyBatteryDisplay result{"L " + left + " · R " + rightText,
            "Left: " + left + "   Right: " + rightText};
        if (batteryCase >= 0 && batteryCase <= 100)
            result.description += "   Case: " + percent(batteryCase);
        return result;
    }
    return {};
}
