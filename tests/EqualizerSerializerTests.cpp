#include "CommandSerializer.h"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    const std::vector<int> bands{-10, -5, 0, 5, 10};

    const Buffer modern = CommandSerializer::serializeEqualizerCustom(0x00, 2, bands);
    const Buffer expectedModern{0x58, 0x00, 0xa0, 0x06, 12, 0, 5, 10, 15, 20};
    assert(modern == expectedModern);

    const Buffer legacy = CommandSerializer::serializeEqualizerCustom(0x01, -2, bands);
    const Buffer expectedLegacy{0x58, 0x01, 0xff, 0x06, 8, 0, 5, 10, 15, 20};
    assert(legacy == expectedLegacy);

    const Buffer preset = CommandSerializer::serializeEqualizerPreset(0x00, 0x16);
    const Buffer expectedPreset{0x58, 0x00, 0x16, 0x00};
    assert(preset == expectedPreset);

    const Buffer clamped = CommandSerializer::serializeEqualizerCustom(0x00, 100, {-50, -10, 0, 10, 50});
    const Buffer expectedClamped{0x58, 0x00, 0xa0, 0x06, 20, 0, 0, 10, 20, 20};
    assert(clamped == expectedClamped);

    bool rejectedInvalidBandCount = false;
    try {
        (void)CommandSerializer::serializeEqualizerCustom(0x00, 0, {0, 0, 0});
    }
    catch (const std::invalid_argument&) {
        rejectedInvalidBandCount = true;
    }
    assert(rejectedInvalidBandCount);
}
