#include "ui/color_picker.h"

#include "game/game.h"
#include "render/render_common.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    PICKER_CONTEXT_SETUP,
    PICKER_CONTEXT_CIV
} PickerContext;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *bits;
    int width;
    int height;
} PickerBitmap;

typedef struct {
    int active;
    PickerContext context;
    int civ_id;
    Color32 original;
    Color32 pending;
    double hue;
    double sat;
    double value;
    int dragging;
} ColorPickerState;

static ColorPickerState picker;
static PickerBitmap wheel_cache;
static PickerBitmap slider_cache;
static int wheel_size;
static double wheel_value = -1.0;
static int slider_w;
static int slider_h;
static double slider_hue = -1.0;
static double slider_sat = -1.0;

static int cr(Color32 c) { return (int)(c & 255); }
static int cg(Color32 c) { return (int)((c >> 8) & 255); }
static int cb(Color32 c) { return (int)((c >> 16) & 255); }

static Color32 color32_from_rgb(int r, int g, int b) {
    return COLOR32_RGB(clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255));
}

static COLORREF colorref_from_color32(Color32 color) {
    return RGB(cr(color), cg(color), cb(color));
}

static unsigned int dib_pixel_from_color32(Color32 color) {
    return ((unsigned int)cr(color) << 16) | ((unsigned int)cg(color) << 8) |
           (unsigned int)cb(color);
}

static void rgb_to_hsv(Color32 color, double *h, double *s, double *v) {
    double r = cr(color) / 255.0;
    double g = cg(color) / 255.0;
    double b = cb(color) / 255.0;
    double maxc = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double minc = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double delta = maxc - minc;
    *v = maxc;
    *s = maxc <= 0.0 ? 0.0 : delta / maxc;
    if (delta <= 0.0001) *h = 0.0;
    else if (maxc == r) *h = fmod((g - b) / delta, 6.0) / 6.0;
    else if (maxc == g) *h = ((b - r) / delta + 2.0) / 6.0;
    else *h = ((r - g) / delta + 4.0) / 6.0;
    if (*h < 0.0) *h += 1.0;
}

static Color32 hsv_to_rgb(double h, double s, double v) {
    double sector = h * 6.0;
    int i = (int)floor(sector);
    double f = sector - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    double r = v, g = t, b = p;
    switch (i % 6) {
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: break;
    }
    return color32_from_rgb((int)(r * 255.0 + 0.5), (int)(g * 255.0 + 0.5),
                            (int)(b * 255.0 + 0.5));
}

static void release_bitmap(PickerBitmap *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int ensure_bitmap(PickerBitmap *cache, int width, int height) {
    BITMAPINFO info;
    void *bits = NULL;
    if (cache->dc && cache->bitmap && cache->width == width && cache->height == height) return 1;
    release_bitmap(cache);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    cache->dc = CreateCompatibleDC(NULL);
    cache->bitmap = CreateDIBSection(cache->dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!cache->dc || !cache->bitmap || !bits) {
        release_bitmap(cache);
        return 0;
    }
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    cache->bits = (unsigned int *)bits;
    cache->width = width;
    cache->height = height;
    return 1;
}

static RECT picker_rect(RECT client) {
    int client_w = client.right - client.left;
    int client_h = client.bottom - client.top;
    int w = min(460, max(380, client_w - 32));
    int h = min(420, max(360, client_h - 32));
    RECT r = {client.left + (client_w - w) / 2, client.top + (client_h - h) / 2,
              client.left + (client_w + w) / 2, client.top + (client_h + h) / 2};
    return r;
}

static int picker_wheel_size(RECT panel) {
    int panel_w = panel.right - panel.left;
    int panel_h = panel.bottom - panel.top;
    return min(220, max(160, min(panel_w - 250, panel_h - 190)));
}

static RECT wheel_rect(RECT panel) {
    int size = picker_wheel_size(panel);
    RECT r = {panel.left + 24, panel.top + 56, panel.left + 24 + size, panel.top + 56 + size};
    return r;
}

static RECT slider_rect(RECT panel) {
    RECT wheel = wheel_rect(panel);
    RECT r = {wheel.left, wheel.bottom + 20, wheel.right, wheel.bottom + 40};
    return r;
}

static int right_column_left(RECT panel) {
    return wheel_rect(panel).right + 24;
}

static RECT preview_rect(RECT panel) {
    int left = right_column_left(panel);
    RECT r = {left, panel.top + 58, panel.right - 24, panel.top + 118};
    return r;
}

static RECT auto_rect(RECT panel) {
    RECT r = {right_column_left(panel), panel.bottom - 82, panel.right - 24, panel.bottom - 54};
    return r;
}

static RECT apply_rect(RECT panel) {
    int left = right_column_left(panel);
    int gap = 10;
    int width = (panel.right - 24 - left - gap) / 2;
    RECT r = {left, panel.bottom - 40, left + width, panel.bottom - 12};
    return r;
}

static RECT cancel_rect(RECT panel) {
    RECT apply = apply_rect(panel);
    RECT r = {apply.right + 10, apply.top, panel.right - 24, apply.bottom};
    return r;
}

static RECT picker_dirty_rect(RECT client) {
    RECT dirty = picker_rect(client);
    InflateRect(&dirty, 10, 10);
    return dirty;
}

static void invalidate_picker(HWND hwnd, RECT client) {
    RECT dirty = picker_dirty_rect(client);
    InvalidateRect(hwnd, &dirty, FALSE);
}

static void set_pending(Color32 color) {
    picker.pending = color;
    rgb_to_hsv(color, &picker.hue, &picker.sat, &picker.value);
}

void color_picker_open_setup(Color32 color) {
    game_pause_for_modal_or_action();
    memset(&picker, 0, sizeof(picker));
    picker.active = 1;
    picker.context = PICKER_CONTEXT_SETUP;
    picker.civ_id = -1;
    picker.original = color;
    set_pending(color);
}

void color_picker_open_civ(int civ_id, Color32 color) {
    color_picker_open_setup(color);
    picker.context = PICKER_CONTEXT_CIV;
    picker.civ_id = civ_id;
}

void color_picker_close(void) {
    if (picker.dragging) ReleaseCapture();
    picker.active = 0;
    picker.dragging = 0;
}

int color_picker_active(void) { return picker.active; }

static void rebuild_wheel(int size) {
    int x, y;
    double radius = (double)(size - 1) * 0.5;
    if (wheel_cache.dc && wheel_cache.bitmap && wheel_size == size &&
        fabs(wheel_value - picker.value) < 0.001) return;
    if (!ensure_bitmap(&wheel_cache, size, size)) return;
    wheel_size = size;
    wheel_value = picker.value;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            double dx = x - radius;
            double dy = y - radius;
            double dist = sqrt(dx * dx + dy * dy) / radius;
            Color32 color = COLOR32_RGB(25, 30, 34);
            if (dist <= 1.0) {
                double hue = atan2(dy, dx) / (2.0 * M_PI);
                if (hue < 0.0) hue += 1.0;
                color = hsv_to_rgb(hue, dist, picker.value);
            }
            wheel_cache.bits[y * size + x] = dib_pixel_from_color32(color);
        }
    }
}

static void rebuild_slider(int width, int height) {
    int x, y;
    if (slider_cache.dc && slider_cache.bitmap && slider_w == width && slider_h == height &&
        fabs(slider_hue - picker.hue) < 0.001 && fabs(slider_sat - picker.sat) < 0.001) return;
    if (!ensure_bitmap(&slider_cache, width, height)) return;
    slider_w = width;
    slider_h = height;
    slider_hue = picker.hue;
    slider_sat = picker.sat;
    for (x = 0; x < width; x++) {
        double v = (double)x / max(1, width - 1);
        unsigned int pixel = dib_pixel_from_color32(hsv_to_rgb(picker.hue, picker.sat, v));
        for (y = 0; y < height; y++) slider_cache.bits[y * width + x] = pixel;
    }
}

static void draw_button(HDC hdc, RECT rect, const char *label) {
    HBRUSH border;
    fill_rect(hdc, rect, RGB(52, 59, 62));
    border = CreateSolidBrush(RGB(92, 104, 110));
    FrameRect(hdc, &rect, border);
    DeleteObject(border);
    draw_text_rect(hdc, rect, label, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_marker(HDC hdc, RECT wheel, Color32 color, int filled) {
    double h, s, v;
    int cx = (wheel.left + wheel.right) / 2;
    int cy = (wheel.top + wheel.bottom) / 2;
    int radius = (wheel.right - wheel.left) / 2;
    int x, y;
    HPEN pen;
    HBRUSH brush;
    rgb_to_hsv(color, &h, &s, &v);
    x = cx + (int)(cos(h * 2.0 * M_PI) * s * radius);
    y = cy + (int)(sin(h * 2.0 * M_PI) * s * radius);
    pen = CreatePen(PS_SOLID, 2, filled ? RGB(18, 22, 25) : RGB(248, 248, 238));
    brush = filled ? CreateSolidBrush(colorref_from_color32(color)) : GetStockObject(NULL_BRUSH);
    SelectObject(hdc, pen);
    SelectObject(hdc, brush);
    Ellipse(hdc, x - 6, y - 6, x + 6, y + 6);
    DeleteObject(pen);
    if (filled) DeleteObject(brush);
}

static void draw_rgb_row(HDC hdc, RECT row, const char *label, int value) {
    char text[24];
    RECT label_rect = {row.left, row.top, row.left + 24, row.bottom};
    RECT value_rect = {row.left + 30, row.top, row.right, row.bottom};
    snprintf(text, sizeof(text), "%d", value);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    draw_text_rect(hdc, value_rect, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

void color_picker_draw(HDC hdc, RECT client) {
    RECT panel, wheel, slider, preview, row, original, current, knob;
    HBRUSH border;
    if (!picker.active) return;
    panel = picker_rect(client);
    wheel = wheel_rect(panel);
    slider = slider_rect(panel);
    preview = preview_rect(panel);
    fill_rect_alpha(hdc, client, RGB(0, 0, 0), 90);
    fill_rect(hdc, panel, RGB(31, 37, 41));
    border = CreateSolidBrush(RGB(94, 106, 114));
    FrameRect(hdc, &panel, border);
    DeleteObject(border);
    draw_text_rect(hdc, (RECT){panel.left + 18, panel.top + 14, panel.right - 18, panel.top + 40},
                   tr("Color Picker", "颜色选择器"), ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    rebuild_wheel(wheel.right - wheel.left);
    BitBlt(hdc, wheel.left, wheel.top, wheel.right - wheel.left, wheel.bottom - wheel.top,
           wheel_cache.dc, 0, 0, SRCCOPY);
    draw_marker(hdc, wheel, picker.original, 0);
    draw_marker(hdc, wheel, picker.pending, 1);
    rebuild_slider(slider.right - slider.left, slider.bottom - slider.top);
    BitBlt(hdc, slider.left, slider.top, slider.right - slider.left, slider.bottom - slider.top,
           slider_cache.dc, 0, 0, SRCCOPY);
    FrameRect(hdc, &slider, GetStockObject(GRAY_BRUSH));
    knob = slider;
    knob.left = slider.left + (int)(picker.value * (slider.right - slider.left - 1)) - 2;
    knob.right = knob.left + 4;
    fill_rect(hdc, knob, RGB(245, 245, 230));
    draw_text_rect(hdc, (RECT){preview.left, preview.top - 22, preview.right, preview.top - 2},
                   tr("Current", "当前"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, preview, colorref_from_color32(picker.pending));
    FrameRect(hdc, &preview, GetStockObject(GRAY_BRUSH));
    original = (RECT){preview.left, preview.bottom + 14, preview.left + 34, preview.bottom + 38};
    current = (RECT){preview.left, original.bottom + 8, preview.left + 34, original.bottom + 32};
    fill_rect(hdc, original, colorref_from_color32(picker.original));
    fill_rect(hdc, current, colorref_from_color32(picker.pending));
    FrameRect(hdc, &original, GetStockObject(GRAY_BRUSH));
    FrameRect(hdc, &current, GetStockObject(GRAY_BRUSH));
    draw_text_rect(hdc, (RECT){original.right + 8, original.top, preview.right, original.bottom},
                   tr("Original", "原色"), ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER);
    draw_text_rect(hdc, (RECT){current.right + 8, current.top, preview.right, current.bottom},
                   tr("Current", "当前"), ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER);
    row = (RECT){preview.left, current.bottom + 16, preview.right, current.bottom + 36};
    draw_text_rect(hdc, row, tr("RGB", "RGB"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER);
    row.top += 22; row.bottom += 22; draw_rgb_row(hdc, row, "R:", cr(picker.pending));
    row.top += 22; row.bottom += 22; draw_rgb_row(hdc, row, "G:", cg(picker.pending));
    row.top += 22; row.bottom += 22; draw_rgb_row(hdc, row, "B:", cb(picker.pending));
    row.top += 10; row.bottom += 10;
    draw_text_rect(hdc, row, tr("Hollow: original", "空心：原色"),
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    row.top += 18; row.bottom += 18;
    draw_text_rect(hdc, row, tr("Filled: current", "实心：当前"),
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_button(hdc, auto_rect(panel), tr("Auto avoid neighbors", "自动避让邻国"));
    draw_button(hdc, apply_rect(panel), tr("Apply", "应用"));
    draw_button(hdc, cancel_rect(panel), tr("Cancel", "取消"));
}

static void update_from_wheel(RECT wheel, int x, int y) {
    double cx = (wheel.left + wheel.right) * 0.5;
    double cy = (wheel.top + wheel.bottom) * 0.5;
    double radius = (wheel.right - wheel.left) * 0.5;
    double dx = x - cx;
    double dy = y - cy;
    double ratio = sqrt(dx * dx + dy * dy) / (radius > 1.0 ? radius : 1.0);
    picker.sat = ratio > 1.0 ? 1.0 : ratio;
    picker.hue = atan2(dy, dx) / (2.0 * M_PI);
    if (picker.hue < 0.0) picker.hue += 1.0;
    picker.pending = hsv_to_rgb(picker.hue, picker.sat, picker.value);
}

static void update_from_slider(RECT slider, int x) {
    picker.value = clamp(x - slider.left, 0, slider.right - slider.left - 1) /
                   (double)max(1, slider.right - slider.left - 1);
    picker.pending = hsv_to_rgb(picker.hue, picker.sat, picker.value);
    wheel_value = -1.0;
}

int color_picker_mouse_down(HWND hwnd, RECT client, int x, int y) {
    RECT panel = picker_rect(client);
    RECT wheel = wheel_rect(panel);
    RECT slider = slider_rect(panel);
    if (!picker.active) return 0;
    if (point_in_rect_local(apply_rect(panel), x, y)) {
        if (picker.context == PICKER_CONTEXT_CIV) {
            game_request_set_civilization_color_exact(picker.civ_id, picker.pending);
        } else {
            selected_civ_color = picker.pending;
            selected_civ_color_index = -1;
        }
        color_picker_close();
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (point_in_rect_local(cancel_rect(panel), x, y)) {
        color_picker_close();
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (point_in_rect_local(auto_rect(panel), x, y)) {
        set_pending(game_preview_civilization_color_auto_avoid(
            picker.context == PICKER_CONTEXT_CIV ? picker.civ_id : -1, picker.pending));
        wheel_value = -1.0;
        slider_hue = -1.0;
        invalidate_picker(hwnd, client);
        return 1;
    }
    if (point_in_rect_local(wheel, x, y)) {
        picker.dragging = 1;
        update_from_wheel(wheel, x, y);
        slider_hue = -1.0;
        SetCapture(hwnd);
        invalidate_picker(hwnd, client);
        return 1;
    }
    if (point_in_rect_local(slider, x, y)) {
        picker.dragging = 2;
        update_from_slider(slider, x);
        SetCapture(hwnd);
        invalidate_picker(hwnd, client);
        return 1;
    }
    return 1;
}

int color_picker_mouse_move(HWND hwnd, RECT client, int x, int y) {
    RECT panel = picker_rect(client);
    if (!picker.active) return 0;
    if (!picker.dragging) return 1;
    if (picker.dragging == 1) {
        update_from_wheel(wheel_rect(panel), x, y);
        slider_hue = -1.0;
    } else {
        update_from_slider(slider_rect(panel), x);
    }
    invalidate_picker(hwnd, client);
    return 1;
}

int color_picker_mouse_up(HWND hwnd) {
    (void)hwnd;
    if (!picker.active || !picker.dragging) return 0;
    picker.dragging = 0;
    ReleaseCapture();
    return 1;
}
