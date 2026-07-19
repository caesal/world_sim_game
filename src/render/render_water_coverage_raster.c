#include "render/render_water_coverage_raster.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define COORD_ONE 256

typedef struct {
    const unsigned char *category;
    int width;
    int height;
} CoverageField;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int field_category(const CoverageField *field, int x, int y) {
    x = clamp_int(x, 0, field->width - 1);
    y = clamp_int(y, 0, field->height - 1);
    return field->category[y * field->width + x];
}

static int field_any_water(const CoverageField *field, int x, int y) {
    return field_category(field, x, y) != RENDER_WATER_CATEGORY_LAND;
}

static int field_is_lake(const CoverageField *field, int x, int y) {
    return field_category(field, x, y) == RENDER_WATER_CATEGORY_LAKE;
}

static int cell_code_any(const CoverageField *field, int x, int y) {
    return field_any_water(field, x, y) |
           (field_any_water(field, x + 1, y) << 1) |
           (field_any_water(field, x + 1, y + 1) << 2) |
           (field_any_water(field, x, y + 1) << 3);
}

static int cell_code_lake(const CoverageField *field, int x, int y) {
    return field_is_lake(field, x, y) |
           (field_is_lake(field, x + 1, y) << 1) |
           (field_is_lake(field, x + 1, y + 1) << 2) |
           (field_is_lake(field, x, y + 1) << 3);
}

static int local_balance_any(const CoverageField *field, int x, int y) {
    int inside = 0;
    int samples = 0;
    int dx, dy;
    for (dy = -1; dy <= 2; dy++) {
        for (dx = -1; dx <= 2; dx++) {
            int sx = x + dx;
            int sy = y + dy;
            if (sx < 0 || sy < 0 || sx >= field->width ||
                sy >= field->height) continue;
            inside += field_any_water(field, sx, sy);
            samples++;
        }
    }
    return inside * 2 - samples;
}

static int bilinear_score(int code, int fx, int fy) {
    int ix = COORD_ONE - fx;
    int iy = COORD_ONE - fy;
    int score = 0;
    if (code & 1) score += ix * iy;
    if (code & 2) score += fx * iy;
    if (code & 4) score += fx * fy;
    if (code & 8) score += ix * fy;
    return score;
}

static int saddle_any_score(const CoverageField *field, int code,
                            int cell_x, int cell_y, int fx, int fy) {
    int ix = COORD_ONE - fx;
    int iy = COORD_ONE - fy;
    int center_bias = (fx * ix * fy * iy + 16384) / 32768;
    int balance = local_balance_any(field, cell_x, cell_y);
    int score = bilinear_score(code, fx, fy);
    if (balance > 0) score += center_bias;
    else if (balance < 0) score -= center_bias;
    return score;
}

static void field_coordinate(int coordinate, int limit,
                             int *cell, int *fraction) {
    int maximum = (limit - 1) * COORD_ONE;
    if (coordinate <= 0 || limit <= 1) {
        *cell = 0;
        *fraction = 0;
    } else if (coordinate >= maximum) {
        *cell = limit - 1;
        *fraction = 0;
    } else {
        *cell = coordinate / COORD_ONE;
        *fraction = coordinate % COORD_ONE;
    }
}

static int field_inside_any(const CoverageField *field,
                            int x_q8, int y_q8) {
    int cell_x, cell_y, fx, fy;
    int code;
    field_coordinate(x_q8, field->width, &cell_x, &fx);
    field_coordinate(y_q8, field->height, &cell_y, &fy);
    code = cell_code_any(field, cell_x, cell_y);
    if (code == 5 || code == 10)
        return saddle_any_score(field, code, cell_x, cell_y, fx, fy) >=
               (COORD_ONE * COORD_ONE) / 2;
    return bilinear_score(code, fx, fy) >=
           (COORD_ONE * COORD_ONE) / 2;
}

static int field_inside_lake(const CoverageField *field,
                             int x_q8, int y_q8) {
    int cell_x, cell_y, fx, fy;
    int code;
    field_coordinate(x_q8, field->width, &cell_x, &fx);
    field_coordinate(y_q8, field->height, &cell_y, &fy);
    code = cell_code_lake(field, cell_x, cell_y);
    /* Lake saddles deliberately receive no neighborhood bridge bias.  The
       semantic-tile support clamp below keeps opposite diagonal lobes in
       distinct four-connected components. */
    return bilinear_score(code, fx, fy) >=
           (COORD_ONE * COORD_ONE) / 2;
}

static int subpixel_coordinate_q8(int pixel, int subpixel,
                                  int scale, int subsamples) {
    int step = COORD_ONE / (scale * subsamples);
    return pixel * (COORD_ONE / scale) + subpixel * step + step / 2 -
           COORD_ONE / 2;
}

static unsigned char coverage_alpha(int inside_samples, int subsamples) {
    int total = subsamples * subsamples;
    return (unsigned char)((inside_samples * 255 + total / 2) / total);
}

static int category_at(const CoverageField *field, int x, int y) {
    if (x < 0 || y < 0 || x >= field->width || y >= field->height) return 0;
    return field->category[y * field->width + x];
}

static void feather_partial_alpha(unsigned char *alpha,
                                  unsigned char *scratch,
                                  int width, int height, int scale) {
    static const int weight[3] = {1, 2, 1};
    int x, y;
    memcpy(scratch, alpha, (size_t)width * (size_t)height);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int index = y * width + x;
            int sum = 0;
            int varies = alpha[index] > 0 && alpha[index] < 255;
            int dx, dy;
            int tile_x = x % scale;
            int tile_y = y % scale;
            int center_core = (tile_x == scale / 2 - 1 ||
                               tile_x == scale / 2) &&
                              (tile_y == scale / 2 - 1 ||
                               tile_y == scale / 2);
            if (center_core && !varies) continue;
            if (!varies) {
                for (dy = -1; dy <= 1 && !varies; dy++) {
                    int sy = clamp_int(y + dy, 0, height - 1);
                    for (dx = -1; dx <= 1; dx++) {
                        int sx = clamp_int(x + dx, 0, width - 1);
                        if (alpha[sy * width + sx] != alpha[index]) {
                            varies = 1;
                            break;
                        }
                    }
                }
            }
            if (!varies) continue;
            for (dy = -1; dy <= 1; dy++) {
                int sy = clamp_int(y + dy, 0, height - 1);
                for (dx = -1; dx <= 1; dx++) {
                    int sx = clamp_int(x + dx, 0, width - 1);
                    sum += alpha[sy * width + sx] *
                           weight[dx + 1] * weight[dy + 1];
                }
            }
            scratch[index] = (unsigned char)((sum + 8) / 16);
        }
    }
}

static void rasterize_legacy_ocean(unsigned char *ocean_alpha,
                                   unsigned char *lake_scratch,
                                   const CoverageField *field,
                                   int scale, int subsamples,
                                   RenderWaterCoverageRasterMetrics *metrics) {
    int width = field->width * scale;
    int height = field->height * scale;
    int x, y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int inside_samples = 0;
            int sx, sy;
            for (sy = 0; sy < subsamples; sy++) {
                int y_q8 = subpixel_coordinate_q8(y, sy, scale, subsamples);
                for (sx = 0; sx < subsamples; sx++) {
                    int x_q8 = subpixel_coordinate_q8(x, sx, scale, subsamples);
                    inside_samples += field_inside_any(field, x_q8, y_q8);
                }
            }
            ocean_alpha[y * width + x] =
                coverage_alpha(inside_samples, subsamples);
        }
    }
    feather_partial_alpha(ocean_alpha, lake_scratch, width, height, scale);
    for (y = 0; y < height; y++) {
        int tile_y = y / scale;
        for (x = 0; x < width; x++) {
            int index = y * width + x;
            int tile_x = x / scale;
            unsigned char alpha = lake_scratch[index];
            if (category_at(field, tile_x, tile_y) !=
                RENDER_WATER_CATEGORY_OCEAN) {
                metrics->ocean_land_pixels_rejected += alpha != 0;
                ocean_alpha[index] = 0;
                continue;
            }
            ocean_alpha[index] = alpha;
        }
    }
}

static void rasterize_supported_lake(
    unsigned char *ocean_alpha, unsigned char *lake_alpha,
    unsigned char *scratch, const CoverageField *field,
    int scale, int subsamples, RenderWaterCoverageRasterMetrics *metrics) {
    int width = field->width * scale;
    int height = field->height * scale;
    int x, y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int inside_samples = 0;
            int sx, sy;
            for (sy = 0; sy < subsamples; sy++) {
                int y_q8 = subpixel_coordinate_q8(y, sy, scale, subsamples);
                for (sx = 0; sx < subsamples; sx++) {
                    int x_q8 = subpixel_coordinate_q8(x, sx, scale, subsamples);
                    inside_samples += field_inside_lake(field, x_q8, y_q8);
                }
            }
            lake_alpha[y * width + x] =
                coverage_alpha(inside_samples, subsamples);
        }
    }
    feather_partial_alpha(lake_alpha, scratch, width, height, scale);
    for (y = 0; y < height; y++) {
        int tile_y = y / scale;
        for (x = 0; x < width; x++) {
            int index = y * width + x;
            int tile_x = x / scale;
            unsigned char alpha = scratch[index];
            if (category_at(field, tile_x, tile_y) !=
                RENDER_WATER_CATEGORY_LAKE) {
                metrics->lake_land_pixels_rejected += alpha != 0;
                lake_alpha[index] = 0;
                continue;
            }
            lake_alpha[index] = alpha;
            if (alpha && ocean_alpha[index]) {
                ocean_alpha[index] = 0;
                metrics->ocean_overlap_pixels_removed++;
            }
        }
    }
}

static uint64_t count_lake_diagonal_cells(const CoverageField *field) {
    uint64_t count = 0;
    int x, y;
    for (y = 0; y + 1 < field->height; y++) {
        for (x = 0; x + 1 < field->width; x++) {
            int code = cell_code_lake(field, x, y);
            count += code == 5 || code == 10;
        }
    }
    return count;
}

int render_water_coverage_rasterize(
    const unsigned char *source, int map_w, int map_h,
    int scale, int subsamples, unsigned char *ocean_alpha,
    unsigned char *lake_alpha, unsigned char *scratch,
    RenderWaterCoverageRasterMetrics *metrics) {
    CoverageField field;
    uint64_t raster_count;
    int product;
    if (!source || !ocean_alpha || !lake_alpha || !scratch || !metrics ||
        map_w <= 0 || map_h <= 0 || scale < 2 || subsamples <= 0 ||
        map_w > INT_MAX / scale || map_h > INT_MAX / scale) return 0;
    product = scale * subsamples;
    if (product <= 0 || product > COORD_ONE || COORD_ONE % product != 0)
        return 0;
    raster_count = (uint64_t)map_w * (uint64_t)map_h *
                   (uint64_t)scale * (uint64_t)scale;
    memset(metrics, 0, sizeof(*metrics));
    field.category = source;
    field.width = map_w;
    field.height = map_h;
    rasterize_legacy_ocean(ocean_alpha, lake_alpha, &field,
                           scale, subsamples, metrics);
    rasterize_supported_lake(ocean_alpha, lake_alpha, scratch, &field,
                             scale, subsamples, metrics);
    metrics->lake_diagonal_cells = count_lake_diagonal_cells(&field);
    metrics->raster_samples = raster_count * (uint64_t)subsamples *
                              (uint64_t)subsamples * 2u;
    return 1;
}
