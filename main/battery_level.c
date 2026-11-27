#include "battery_level.h"

int battery_level(int percent)
{
    if (percent < 10) {
        return 0;
    }
    if (percent < 35) {
        return 1;
    }
    if (percent < 60) {
        return 2;
    }
    return percent < 85 ? 3 : 4;
}
