#include "render/render_coast_pattern.h"

enum { COMB_MIN_RUN = 3, COMB_MAX_RUN = 96 };

static int mask_at(const unsigned char *mask, int width, int height,
                   int x, int y) {
    return x >= 0 && y >= 0 && x < width && y < height &&
           mask[y * width + x] != 0;
}

static int run_length(const unsigned char *mask, int width, int height,
                      int x, int y, int dx, int dy) {
    int length = 0;
    while (length < COMB_MAX_RUN &&
           mask_at(mask, width, height, x, y)) {
        length++;
        x += dx;
        y += dy;
    }
    return length;
}

static int parallel_support(const unsigned char *mask, int width, int height,
                            int x, int y, int dx, int dy, int length) {
    int samples = length < 12 ? length : 12;
    int support = 0;
    int i;
    for (i = 0; i < samples; i++)
        support += mask_at(mask, width, height,
                           x + i * dx, y + i * dy);
    return support;
}

static void scan_pattern(const unsigned char *mask, int width, int height,
                         int *thin_runs, int *comb_clusters) {
    static const int directions[4][2] = {
        {0, 1}, {1, 1}, {-1, 1}, {1, 0}
    };
    int direction;
    *thin_runs = 0;
    *comb_clusters = 0;
    for (direction = 0; direction < 4; direction++) {
        int dx = directions[direction][0];
        int dy = directions[direction][1];
        int px = -dy;
        int py = dx;
        int x, y;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                int length;
                int side_support;
                int parallel = 0;
                int offset;
                if (!mask_at(mask, width, height, x, y) ||
                    mask_at(mask, width, height, x - dx, y - dy)) continue;
                length = run_length(mask, width, height, x, y, dx, dy);
                if (length < COMB_MIN_RUN) continue;
                side_support = parallel_support(
                    mask, width, height, x + px, y + py,
                    dx, dy, length) + parallel_support(
                    mask, width, height, x - px, y - py,
                    dx, dy, length);
                if (side_support * 2 > (length < 12 ? length : 12)) continue;
                (*thin_runs)++;
                for (offset = 2; offset <= 8; offset += 2) {
                    int sampled = length < 12 ? length : 12;
                    int needed = sampled < 6 ? COMB_MIN_RUN : sampled / 2;
                    int positive = parallel_support(
                        mask, width, height, x + px * offset,
                        y + py * offset, dx, dy, length);
                    int negative = parallel_support(
                        mask, width, height, x - px * offset,
                        y - py * offset, dx, dy, length);
                    parallel += positive >= needed;
                    parallel += negative >= needed;
                }
                if (parallel < 2) continue;
                (*comb_clusters)++;
            }
        }
    }
}

void render_coast_pattern_detect(
    const unsigned char *mask, int width, int height,
    int *thin_runs, int *comb_clusters) {
    if (!mask || width <= 0 || height <= 0 ||
        !thin_runs || !comb_clusters) return;
    scan_pattern(mask, width, height, thin_runs, comb_clusters);
}
