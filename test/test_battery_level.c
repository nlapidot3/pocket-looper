#include <assert.h>
#include <stdio.h>

#include "battery_level.h"

static int s_cases;

static void expect_level(int percent, int level)
{
    assert(battery_level(percent) == level);
    s_cases++;
}

int main(void)
{
    expect_level(0, 0);
    expect_level(9, 0);
    expect_level(10, 1);
    expect_level(34, 1);
    expect_level(35, 2);
    expect_level(59, 2);
    expect_level(60, 3);
    expect_level(84, 3);
    expect_level(85, 4);
    expect_level(100, 4);
    printf("battery_level: %d checks passed\n", s_cases);
    return 0;
}
