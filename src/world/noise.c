#include "noise.h"

#include "core/game_state.h"

static int smooth_step_255(int value) {
    value = clamp(value, 0, 255);
    return value * value * (765 - 2 * value) / 65025;
}

static int lerp_int(int a, int b, int weight) {
    return (a * (255 - weight) + b * weight) / 255;
}

static int noise_corner(int x, int y, int seed) {
    unsigned int n = (unsigned int)(x * 374761393u + y * 668265263u + seed * 2246822519u);
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= n >> 16;
    return (int)(n % 101u);
}

static int value_noise(int x, int y, int scale, int seed) {
    int x0 = x / scale;
    int y0 = y / scale;
    int fx = smooth_step_255((x % scale) * 255 / scale);
    int fy = smooth_step_255((y % scale) * 255 / scale);
    int a = noise_corner(x0, y0, seed);
    int b = noise_corner(x0 + 1, y0, seed);
    int c = noise_corner(x0, y0 + 1, seed);
    int d = noise_corner(x0 + 1, y0 + 1, seed);
    int top = lerp_int(a, b, fx);
    int bottom = lerp_int(c, d, fx);
    return lerp_int(top, bottom, fy);
}

int world_fractal_noise(int x, int y, int seed) {
    int total = 0;

    total += value_noise(x, y, 240, seed) * 46;
    total += value_noise(x, y, 128, seed + 31) * 31;
    total += value_noise(x, y, 68, seed + 79) * 17;
    total += value_noise(x, y, 34, seed + 137) * 6;
    return total / 100;
}
