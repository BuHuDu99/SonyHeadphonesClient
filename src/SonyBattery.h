#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

// MDR battery payloads carry literal percentages, including values below 10%.
constexpr int decodeSonyBatteryPercentage(uint8_t raw)
{
    return raw <= 100 ? static_cast<int>(raw) : -1;
}

// Sizes include opcode and inquired type. Threshold bytes are not battery levels.
constexpr size_t sonyBatteryPayloadSize(uint8_t type)
{
    switch (type) {
    case 0x00: case 0x02: return 4;
    case 0x01: return 6;
    case 0x08: case 0x0a: return 5;
    case 0x09: return 8;
    default: return 0;
    }
}

constexpr int sonyBatteryTypeForFunction(uint8_t function)
{
    switch (function) {
    case 0x20: return 0x00;
    case 0x21: return 0x01;
    case 0x22: return 0x02;
    case 0x28: return 0x08;
    case 0x29: return 0x09;
    case 0x2a: return 0x0a;
    default: return -1;
    }
}

constexpr uint16_t selectSonyBatteryTypes(uint16_t supported)
{
    const bool single = supported & ((1u << 0) | (1u << 8));
    const bool dual = supported & ((1u << 1) | (1u << 9));
    if (single == dual) return 0;
    if (single) return supported & (1u << 8) ? 1u << 8 : 1u << 0;
    if (supported & (1u << 9)) supported &= ~(1u << 1);
    if (supported & (1u << 10)) supported &= ~(1u << 2);
    return supported;
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
