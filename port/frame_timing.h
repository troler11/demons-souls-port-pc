#pragma once

/* PS3 vblank rates are in millihertz; all host deadlines use microseconds. */
inline double des_vblank_period_us(int millihz, double fallback_us)
{
    return millihz > 0 ? 1.0e9 / millihz : fallback_us;
}

inline int des_present_period_us(int fps, bool cutscene)
{
    if (cutscene || fps == 0 || fps == 30) return 33367;
    if (fps == 1000) return 0;  // explicit experimental uncapped mode
    if (fps == 60) return 16683;
    return fps > 0 ? (1000000 + fps / 2) / fps : 33367;
}
