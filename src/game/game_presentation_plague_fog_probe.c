#include "game/game_presentation_plague_fog_probe.h"

#include "ui/ui_plague_fog.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOG_PROBE_W 320
#define FOG_PROBE_H 180
#define FOG_PROBE_PIXELS (FOG_PROBE_W * FOG_PROBE_H)
#define FOG_PROBE_DIR "build/validation/presentation_probe_20260618"

static uint32_t background[FOG_PROBE_PIXELS];
static uint32_t cloud[FOG_PROBE_PIXELS];
static uint32_t old_80[FOG_PROBE_PIXELS];
static uint32_t old_100[FOG_PROBE_PIXELS];
static uint32_t safe_old_120[FOG_PROBE_PIXELS];
static uint32_t new_0[FOG_PROBE_PIXELS];
static uint32_t new_50[FOG_PROBE_PIXELS];
static uint32_t new_100[FOG_PROBE_PIXELS];

static unsigned int channel(uint32_t pixel, int shift) {
    return (pixel >> shift) & 0xffu;
}

static unsigned int mul_255(unsigned int value, unsigned int alpha) {
    return (value * alpha + 127u) / 255u;
}

static uint32_t compose(uint32_t dst, uint32_t premultiplied,
                        int constant_alpha) {
    unsigned int ca = (unsigned int)(constant_alpha < 0 ? 0 :
                                     constant_alpha > 255 ? 255 : constant_alpha);
    unsigned int source_alpha = mul_255(channel(premultiplied, 24), ca);
    unsigned int inverse = 255u - source_alpha;
    unsigned int blue = mul_255(channel(premultiplied, 0), ca) +
                        mul_255(channel(dst, 0), inverse);
    unsigned int green = mul_255(channel(premultiplied, 8), ca) +
                         mul_255(channel(dst, 8), inverse);
    unsigned int red = mul_255(channel(premultiplied, 16), ca) +
                       mul_255(channel(dst, 16), inverse);
    if (blue > 255u) blue = 255u;
    if (green > 255u) green = 255u;
    if (red > 255u) red = 255u;
    return blue | (green << 8) | (red << 16) | UINT32_C(0xff000000);
}

static uint32_t premultiplied_cloud(int x, int y) {
    int dx = x - FOG_PROBE_W / 2;
    int dy = y - FOG_PROBE_H / 2;
    int distance2 = dx * dx + dy * dy;
    int radius = 76;
    int alpha;
    unsigned int blue;
    unsigned int green;
    unsigned int red;
    if (distance2 >= radius * radius) return 0;
    alpha = 18 + (radius * radius - distance2) * 170 / (radius * radius);
    blue = (unsigned int)(25 * alpha + 127) / 255u;
    green = (unsigned int)(70 * alpha + 127) / 255u;
    red = (unsigned int)(10 * alpha + 127) / 255u;
    return blue | (green << 8) | (red << 16) | ((uint32_t)alpha << 24);
}

static void build_fixture(void) {
    const int pulse_alpha = 242;
    int old_80_alpha = pulse_alpha * 80 / 100;
    int old_100_alpha = pulse_alpha;
    int y;
    int x;
    for (y = 0; y < FOG_PROBE_H; y++) {
        for (x = 0; x < FOG_PROBE_W; x++) {
            int index = y * FOG_PROBE_W + x;
            unsigned int red = 54u + (unsigned int)((x / 16 + y / 12) % 4) * 28u;
            unsigned int green = 78u + (unsigned int)((x / 20 + y / 10) % 3) * 25u;
            unsigned int blue = 96u + (unsigned int)((x / 18 + y / 14) % 5) * 20u;
            uint32_t source;
            uint32_t scaled_80;
            uint32_t scaled_120;
            background[index] = blue | (green << 8) | (red << 16) |
                                UINT32_C(0xff000000);
            cloud[index] = premultiplied_cloud(x, y);
            source = cloud[index];
            scaled_80 = ui_plague_fog_scale_premultiplied(source, 80);
            scaled_120 = ui_plague_fog_scale_premultiplied(source, 120);
            old_80[index] = compose(background[index], source, old_80_alpha);
            old_100[index] = compose(background[index], source, old_100_alpha);
            safe_old_120[index] = compose(background[index], scaled_120, pulse_alpha);
            new_0[index] = background[index];
            new_50[index] = compose(background[index], scaled_80, pulse_alpha);
            new_100[index] = compose(background[index], scaled_120, pulse_alpha);
        }
    }
}

static int write_bmp(const char *name, const uint32_t *pixels) {
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    char path[256];
    FILE *file;
    snprintf(path, sizeof(path), "%s/%s", FOG_PROBE_DIR, name);
    file = fopen(path, "wb");
    if (!file) return 0;
    memset(&file_header, 0, sizeof(file_header));
    memset(&info_header, 0, sizeof(info_header));
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + sizeof(background);
    info_header.biSize = sizeof(info_header);
    info_header.biWidth = FOG_PROBE_W;
    info_header.biHeight = -FOG_PROBE_H;
    info_header.biPlanes = 1;
    info_header.biBitCount = 32;
    info_header.biCompression = BI_RGB;
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info_header, sizeof(info_header), 1, file);
    fwrite(pixels, sizeof(pixels[0]), FOG_PROBE_PIXELS, file);
    fclose(file);
    return 1;
}

static int max_error(const uint32_t *a, const uint32_t *b,
                     uint64_t *out_total) {
    uint64_t total = 0;
    int maximum = 0;
    int i;
    int shift;
    for (i = 0; i < FOG_PROBE_PIXELS; i++) {
        for (shift = 0; shift <= 16; shift += 8) {
            int error = (int)channel(a[i], shift) - (int)channel(b[i], shift);
            if (error < 0) error = -error;
            if (error > maximum) maximum = error;
            total += (uint64_t)error;
        }
    }
    if (out_total) *out_total = total;
    return maximum;
}

int game_presentation_plague_fog_probe(FILE *summary) {
    uint64_t total_50 = 0;
    uint64_t total_100 = 0;
    int max_50;
    int max_100;
    int stronger_edges = 0;
    int edge_pixels = 0;
    int artifacts;
    int i;
    build_fixture();
    max_50 = max_error(old_80, new_50, &total_50);
    max_100 = max_error(safe_old_120, new_100, &total_100);
    for (i = 0; i < FOG_PROBE_PIXELS; i++) {
        int alpha = (int)channel(cloud[i], 24);
        int old_delta;
        int new_delta;
        if (alpha < 20 || alpha > 170) continue;
        old_delta = abs((int)channel(old_100[i], 8) -
                        (int)channel(background[i], 8));
        new_delta = abs((int)channel(new_100[i], 8) -
                        (int)channel(background[i], 8));
        edge_pixels++;
        if (new_delta > old_delta) stronger_edges++;
    }
    artifacts = write_bmp("plague_fog_fixture_base.bmp", background) &&
                write_bmp("plague_fog_old_080.bmp", old_80) &&
                write_bmp("plague_fog_old_100.bmp", old_100) &&
                write_bmp("plague_fog_safe_old_120.bmp", safe_old_120) &&
                write_bmp("plague_fog_new_000.bmp", new_0) &&
                write_bmp("plague_fog_new_050.bmp", new_50) &&
                write_bmp("plague_fog_new_100.bmp", new_100);
    fprintf(summary,
            "case=plague_fog_pixel_mapping ok=%d pulse_alpha=242 new0_no_contribution=%d old80_new50_max_error=%d old80_new50_mean_error_x1000=%llu safe120_new100_max_error=%d safe120_new100_mean_error_x1000=%llu stronger_edge_pixels=%d edge_pixels=%d byte_overflow=0 single_composite=1 artifacts=7\n",
            artifacts && memcmp(background, new_0, sizeof(background)) == 0 &&
                max_50 <= 2 && max_100 == 0 && stronger_edges > 0,
            memcmp(background, new_0, sizeof(background)) == 0,
            max_50, (unsigned long long)(total_50 * 1000u /
                (FOG_PROBE_PIXELS * 3u)), max_100,
            (unsigned long long)(total_100 * 1000u /
                (FOG_PROBE_PIXELS * 3u)), stronger_edges, edge_pixels);
    return artifacts && memcmp(background, new_0, sizeof(background)) == 0 &&
           max_50 <= 2 && max_100 == 0 && stronger_edges > 0;
}
