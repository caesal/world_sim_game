#include "render/top_world_announcement_resources.h"

#include "render/icons.h"

#include <string.h>

#define ANNOUNCEMENT_WIDTH_CACHE_MAX 128
#define ANNOUNCEMENT_ICON_CACHE_SIZE 32
#define ANNOUNCEMENT_ICON_TRANSPARENT RGB(1, 2, 3)

typedef struct {
    char text[192];
    int width;
} AnnouncementWidthCacheEntry;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP default_bitmap;
    int valid;
} AnnouncementIconCacheEntry;

static HFONT header_font;
static HFONT body_font;
static HFONT metadata_font;
static HBRUSH border_brush;
static HDC prewarm_dc;
static HBITMAP prewarm_bitmap;
static HBITMAP prewarm_default_bitmap;
static int text_prewarm_index;
static AnnouncementWidthCacheEntry width_cache[ANNOUNCEMENT_WIDTH_CACHE_MAX];
static int width_cache_count;
static int width_cache_next;
static int peak_layout_ms;
static int peak_content_ms;
static int peak_composite_ms;
static AnnouncementIconCacheEntry icon_cache[ICON_COUNT];
static HBRUSH icon_transparent_brush;

static int ensure_icon_cache(HDC target, IconId icon, COLORREF fallback) {
    AnnouncementIconCacheEntry *entry;
    RECT rect = {0, 0, ANNOUNCEMENT_ICON_CACHE_SIZE, ANNOUNCEMENT_ICON_CACHE_SIZE};
    if (!target || icon < 0 || icon >= ICON_COUNT) return 0;
    entry = &icon_cache[icon];
    if (entry->valid) return 1;
    if (!entry->dc) entry->dc = CreateCompatibleDC(target);
    if (!entry->dc) return 0;
    if (!entry->bitmap) {
        entry->bitmap = CreateCompatibleBitmap(target, ANNOUNCEMENT_ICON_CACHE_SIZE,
                                                ANNOUNCEMENT_ICON_CACHE_SIZE);
        if (!entry->bitmap) return 0;
        entry->default_bitmap = SelectObject(entry->dc, entry->bitmap);
    }
    if (!icon_transparent_brush)
        icon_transparent_brush = CreateSolidBrush(ANNOUNCEMENT_ICON_TRANSPARENT);
    if (!icon_transparent_brush) return 0;
    FillRect(entry->dc, &rect, icon_transparent_brush);
    draw_icon_fit(entry->dc, icon, rect, fallback);
    GdiFlush();
    entry->valid = 1;
    return 1;
}

static int ensure_prewarm_surface(HDC target) {
    HBITMAP bitmap;
    if (prewarm_dc && prewarm_bitmap) return 1;
    if (!target) return 0;
    prewarm_dc = CreateCompatibleDC(target);
    if (!prewarm_dc) return 0;
    bitmap = CreateCompatibleBitmap(target, 1024, 48);
    if (!bitmap) return 0;
    prewarm_default_bitmap = SelectObject(prewarm_dc, bitmap);
    prewarm_bitmap = bitmap;
    SetBkMode(prewarm_dc, TRANSPARENT);
    return 1;
}

void top_world_announcement_resources_ensure(void) {
    if (!header_font) header_font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    if (!body_font) body_font = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    if (!metadata_font) metadata_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    if (!border_brush) border_brush = CreateSolidBrush(RGB(112, 124, 130));
}

void top_world_announcement_resources_prewarm(HDC target) {
    static const IconId icons[] = {
        ICON_INNOVATION, ICON_TERRITORY, ICON_DISORDER, ICON_COHESION,
        ICON_ADAPTATION, ICON_ATTACK, ICON_COUNTRY_DEFENSE, ICON_GOVERNANCE
    };
    static const wchar_t *samples[] = {
        L"World Announcement Alliance Union Year Month 0123456789 "
        L"\u4e16\u754c\u52a8\u6001 \u8054\u76df\u8054\u5408 \u5e74\u6708",
        L"Country alliance absorbed members continued technology milestone "
        L"\u56fd\u5bb6 \u8054\u76df \u6210\u5458 \u6280\u672f \u91cc\u7a0b\u7891",
        L"queued Previous Next Locate Dismiss 0123456789 "
        L"\u961f\u5217 \u4e0a\u4e00\u9875 \u4e0b\u4e00\u9875 \u5b9a\u4f4d \u5173\u95ed"
    };
    static HFONT *fonts[] = {&header_font, &body_font, &metadata_font};
    static int index;
    HFONT old_font;
    top_world_announcement_resources_ensure();
    if (!ensure_prewarm_surface(target)) return;
    if (text_prewarm_index < (int)(sizeof(samples) / sizeof(samples[0]))) {
        old_font = SelectObject(prewarm_dc, *fonts[text_prewarm_index]);
        TextOutW(prewarm_dc, 0, 0, samples[text_prewarm_index],
                 lstrlenW(samples[text_prewarm_index]));
        SelectObject(prewarm_dc, old_font);
        text_prewarm_index++;
    }
    if (index < (int)(sizeof(icons) / sizeof(icons[0]))) {
        preload_icon(icons[index]);
        ensure_icon_cache(target, icons[index], RGB(220, 224, 228));
        index++;
    }
}

void top_world_announcement_draw_cached_icon(HDC target, IconId icon,
                                             RECT rect, COLORREF fallback) {
    if (!ensure_icon_cache(target, icon, fallback)) {
        draw_icon_fit(target, icon, rect, fallback);
        return;
    }
    TransparentBlt(target, rect.left, rect.top, rect.right - rect.left,
                   rect.bottom - rect.top, icon_cache[icon].dc, 0, 0,
                   ANNOUNCEMENT_ICON_CACHE_SIZE, ANNOUNCEMENT_ICON_CACHE_SIZE,
                   ANNOUNCEMENT_ICON_TRANSPARENT);
}

int top_world_announcement_text_width(HDC target, const char *text) {
    WCHAR wide[192];
    SIZE size = {0, 0};
    int len;
    int i;
    int slot;
    if (!target || !text || !text[0]) return 0;
    for (i = 0; i < width_cache_count; i++) {
        if (strcmp(width_cache[i].text, text) == 0) return width_cache[i].width;
    }
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide,
                              (int)(sizeof(wide) / sizeof(wide[0])));
    if (len > 0) GetTextExtentPoint32W(target, wide, len - 1, &size);
    slot = width_cache_count < ANNOUNCEMENT_WIDTH_CACHE_MAX ? width_cache_count++ : width_cache_next++;
    slot %= ANNOUNCEMENT_WIDTH_CACHE_MAX;
    lstrcpynA(width_cache[slot].text, text, (int)sizeof(width_cache[slot].text));
    width_cache[slot].width = size.cx;
    return size.cx;
}

void top_world_announcement_note_draw_phases(int layout_ms, int content_ms,
                                             int composite_ms) {
    if (layout_ms > peak_layout_ms) peak_layout_ms = layout_ms;
    if (content_ms > peak_content_ms) peak_content_ms = content_ms;
    if (composite_ms > peak_composite_ms) peak_composite_ms = composite_ms;
}

void top_world_announcement_reset_phase_metrics(void) {
    peak_layout_ms = peak_content_ms = peak_composite_ms = 0;
}

int top_world_announcement_peak_layout_ms(void) { return peak_layout_ms; }
int top_world_announcement_peak_content_ms(void) { return peak_content_ms; }
int top_world_announcement_peak_composite_ms(void) { return peak_composite_ms; }

HFONT top_world_announcement_header_font(void) { return header_font; }
HFONT top_world_announcement_body_font(void) { return body_font; }
HFONT top_world_announcement_metadata_font(void) { return metadata_font; }
HBRUSH top_world_announcement_border_brush(void) { return border_brush; }
