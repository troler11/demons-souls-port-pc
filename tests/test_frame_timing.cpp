#include "../port/frame_timing.h"

static bool near(double a, double b) { return a > b - 0.001 && a < b + 0.001; }
int main()
{
    if (!near(des_vblank_period_us(120000, 16683.35), 8333.333333)) return 1;
    if (!near(des_vblank_period_us(59940, 0), 16683.350017)) return 2;
    if (!near(des_vblank_period_us(0, 16683.35), 16683.35)) return 3;
    if (des_present_period_us(0, false) != 33367) return 4;
    if (des_present_period_us(60, false) != 16683) return 5;
    if (des_present_period_us(120, false) != 8333) return 6;
    if (des_present_period_us(120, true) != 33367) return 7;
    if (des_present_period_us(1000, false) != 0) return 8;
    return 0;
}
