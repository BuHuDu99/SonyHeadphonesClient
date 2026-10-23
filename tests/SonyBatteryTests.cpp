#include "SonyBattery.h"

#include <cassert>

int main()
{
    assert(decodeSonyBatteryPercentage(0) == 0);
    assert(decodeSonyBatteryPercentage(1) == 10);
    assert(decodeSonyBatteryPercentage(9) == 90);
    assert(decodeSonyBatteryPercentage(10) == 100);
    assert(decodeSonyBatteryPercentage(75) == 75);
    assert(decodeSonyBatteryPercentage(0xff) == -1);
    return 0;
}
