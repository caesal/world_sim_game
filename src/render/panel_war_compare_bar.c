#include "render/panel_war_compare_bar.h"

#include "render/icons.h"
#include "render/ui_format.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int left, right, single;
    COLORREF lc, rc;
    const char *label;
} WarSegment;

static int positive(int v) { return v > 0 ? v : 0; }

static int total_left(const WarCompareBarModel *m) {
    return positive(m->left_regular) + positive(m->left_vassal) +
           positive(m->left_mercenary) + positive(m->left_alliance);
}

static int total_right(const WarCompareBarModel *m) {
    return positive(m->right_regular) + positive(m->right_vassal) +
           positive(m->right_mercenary) + positive(m->right_alliance);
}

int panel_war_compare_bar_height(const WarCompareBarModel *m) {
    int mixed = m && (m->left_vassal || m->right_vassal || m->left_mercenary ||
                m->right_mercenary || m->left_alliance || m->right_alliance);
    return mixed ? 146 : 126;
}

static COLORREF visible_color(COLORREF color, COLORREF fallback) {
    if (GetRValue(color) + GetGValue(color) + GetBValue(color) < 24) return fallback;
    return color;
}

static void swatch(HDC hdc, RECT r, COLORREF c) {
    fill_rect(hdc, r, c);
    FrameRect(hdc, &r, GetStockObject(BLACK_BRUSH));
}

static int build_segments(const WarCompareBarModel *m, WarSegment *out) {
    int n = 0;
    out[n++] = (WarSegment){m->left_regular, m->right_regular, 0,
        visible_color(m->left_regular_color, RGB(86,150,208)),
        visible_color(m->right_regular_color, RGB(172,116,214)),
        tr("Regular army", "正规军")};
    if (m->left_vassal || m->right_vassal)
        out[n++] = (WarSegment){m->left_vassal, m->right_vassal, 0,
            visible_color(m->left_vassal_color, RGB(146,104,170)),
            visible_color(m->right_vassal_color, RGB(92,128,190)),
            tr("Vassal army", "附庸军")};
    if (m->left_mercenary || m->right_mercenary)
        out[n++] = (WarSegment){m->left_mercenary, m->right_mercenary, 1,
            RGB(245,245,238), RGB(245,245,238), tr("Mercenary army", "雇佣军")};
    if (m->left_alliance || m->right_alliance)
        out[n++] = (WarSegment){m->left_alliance, m->right_alliance, 0,
            visible_color(m->left_alliance_color, RGB(76,64,128)),
            visible_color(m->right_alliance_color, RGB(206,92,82)),
            tr("Alliance army", "联盟军")};
    return n;
}

static int divider_x(RECT bar, int left_total, int right_total) {
    int total = max(1, left_total + right_total);
    int min_side = left_total && right_total ? 24 : 12;
    int x = bar.left + (bar.right - bar.left) * left_total / total;
    return clamp(x, bar.left + min_side, bar.right - min_side);
}

static void draw_side_segments(HDC hdc, RECT r, const WarSegment *seg, int n, int left_side) {
    int total = 0, remaining_total, remaining_px, cursor = r.left, pos;
    for (pos = 0; pos < n; pos++) total += positive(left_side ? seg[pos].left : seg[pos].right);
    remaining_total = total;
    remaining_px = r.right - r.left;
    if (total <= 0 || remaining_px <= 0) return;
    for (pos = 0; pos < n; pos++) {
        int idx = left_side ? (n - 1 - pos) : pos;
        int value = positive(left_side ? seg[idx].left : seg[idx].right);
        int w;
        if (value <= 0) continue;
        w = value >= remaining_total ? remaining_px :
            max(1, remaining_px * value / max(1, remaining_total));
        fill_rect(hdc, (RECT){cursor, r.top, min(r.right, cursor + w), r.bottom},
                  left_side ? seg[idx].lc : seg[idx].rc);
        cursor = min(r.right, cursor + w);
        remaining_px = max(0, r.right - cursor);
        remaining_total -= value;
    }
}

static void legend_item(HDC hdc, int left, int *x, int *y, int right, const WarSegment *seg) {
    int w = seg->single ? 140 : 134;
    RECT s = {*x, *y + 1, *x + 12, *y + 9};
    RECT t = {*x + 18, *y - 2, *x + w, *y + 18};
    if (*x + w > right) {
        *x = left;
        *y += 20;
        s = (RECT){*x, *y + 1, *x + 12, *y + 9};
        t = (RECT){*x + 18, *y - 2, *x + w, *y + 18};
    }
    if (seg->single) swatch(hdc, (RECT){s.left, *y + 5, s.right, *y + 13}, seg->lc);
    else {
        if (seg->left > 0) swatch(hdc, s, seg->lc);
        if (seg->right > 0) swatch(hdc, (RECT){s.left, *y + 10, s.right, *y + 18}, seg->rc);
    }
    draw_text_rect(hdc, t, seg->label, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    *x += w;
}

void panel_war_compare_bar_draw(HDC hdc, UiCursor *cursor, const WarCompareBarModel *m) {
    WarSegment seg[4];
    int n, lt, rt, split, x, y, i;
    RECT row, left_name, right_name, left_value, right_value, bar, icon, badge;
    RECT left_swatch, right_swatch;
    char lv[48], rv[48];
    int center, cy;
    HBRUSH badge_brush;
    HPEN badge_pen;
    HGDIOBJ old_brush, old_pen;
    if (!m || !cursor) return;
    n = build_segments(m, seg);
    lt = total_left(m);
    rt = total_right(m);
    row = ui_take_rect(cursor, panel_war_compare_bar_height(m));
    center = (row.left + row.right) / 2;
    left_swatch = (RECT){row.left, row.top + 8, row.left + 18, row.top + 26};
    right_swatch = (RECT){row.right - 18, row.top + 8, row.right, row.top + 26};
    left_name = (RECT){left_swatch.right + 8, row.top, center - 30, row.top + 24};
    right_name = (RECT){center + 30, row.top, right_swatch.left - 8, row.top + 24};
    left_value = (RECT){left_name.left, row.top + 25, left_name.right, row.top + 46};
    right_value = (RECT){right_name.left, row.top + 25, right_name.right, row.top + 46};
    bar = (RECT){row.left, row.top + 60, row.right, row.top + 98};
    split = divider_x(bar, lt, rt);
    format_metric_value(lt, lv, sizeof(lv));
    format_metric_value(rt, rv, sizeof(rv));
    swatch(hdc, left_swatch, seg[0].lc);
    swatch(hdc, right_swatch, seg[0].rc);
    draw_text_rect(hdc, left_name, m->left_name, seg[0].lc, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, left_value, lv, ui_theme_color(UI_COLOR_TEXT), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, right_name, m->right_name, seg[0].rc, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, right_value, rv, ui_theme_color(UI_COLOR_TEXT), DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, bar, RGB(42, 47, 50));
    draw_side_segments(hdc, (RECT){bar.left, bar.top, split, bar.bottom}, seg, n, 1);
    draw_side_segments(hdc, (RECT){split, bar.top, bar.right, bar.bottom}, seg, n, 0);
    FrameRect(hdc, &bar, GetStockObject(BLACK_BRUSH));
    fill_rect(hdc, (RECT){split - 1, bar.top - 4, split + 1, bar.bottom + 4}, RGB(236, 228, 198));
    cy = (bar.top + bar.bottom) / 2;
    badge = (RECT){split - 16, cy - 16, split + 16, cy + 16};
    badge_brush = CreateSolidBrush(RGB(35, 41, 45));
    badge_pen = CreatePen(PS_SOLID, 1, RGB(236, 228, 198));
    old_brush = SelectObject(hdc, badge_brush);
    old_pen = SelectObject(hdc, badge_pen);
    Ellipse(hdc, badge.left, badge.top, badge.right, badge.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(badge_pen);
    DeleteObject(badge_brush);
    icon = (RECT){split - 9, cy - 9, split + 9, cy + 9};
    draw_icon(hdc, ICON_ATTACK, icon, RGB(236, 228, 198));
    x = row.left;
    y = row.top + 112;
    for (i = 0; i < n; i++) legend_item(hdc, row.left, &x, &y, row.right, &seg[i]);
}

static WarCompareBarModel sample_model(int scenario) {
    WarCompareBarModel m = {"Astra", "Boreal", "Attacker", "Defender", 180, 0, 0, 0,
        120, 0, 0, 0, RGB(86,150,208), RGB(172,116,214),
        RGB(76,64,128), RGB(206,92,82), RGB(146,104,170), RGB(92,128,190)};
    if (scenario == 1) {
        m.left_vassal = 35; m.left_mercenary = 18; m.left_alliance = 42;
        m.right_vassal = 22; m.right_mercenary = 12; m.right_alliance = 30;
    } else if (scenario == 2) {
        m.left_regular = 420; m.left_vassal = 80; m.left_mercenary = 40;
        m.left_alliance = 110; m.right_regular = 95; m.right_vassal = 12;
    } else if (scenario == 3) {
        m.left_regular = 75; m.right_regular = 460; m.right_vassal = 70;
        m.right_mercenary = 45; m.right_alliance = 120;
    } else if (scenario == 4) {
        m.left_name = "Western Azure Confederation Expeditionary Army";
        m.right_name = "Northern River Mountain Imperial Defense Coalition";
        m.left_regular = 235; m.right_regular = 210;
        m.left_mercenary = 22; m.right_alliance = 44;
    } else if (scenario == 5) {
        m.left_regular = 260; m.left_alliance = 90; m.right_regular = 180;
    } else if (scenario == 6) {
        m.left_regular = 170; m.right_regular = 280; m.right_alliance = 85;
    }
    return m;
}

int panel_war_compare_bar_probe_totals(int scenario, int *left_regular,
                                       int *left_alliance, int *right_regular,
                                       int *right_alliance, int *left_total,
                                       int *right_total) {
    WarCompareBarModel m = sample_model(scenario);
    if (left_regular) *left_regular = positive(m.left_regular);
    if (left_alliance) *left_alliance = positive(m.left_alliance);
    if (right_regular) *right_regular = positive(m.right_regular);
    if (right_alliance) *right_alliance = positive(m.right_alliance);
    if (left_total) *left_total = total_left(&m);
    if (right_total) *right_total = total_right(&m);
    return 1;
}

int panel_war_compare_bar_probe_render(const char *path, int scenario) {
    const int w = 820, h = 210;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    FILE *file;
    BITMAPFILEHEADER fh;
    UiCursor cursor = ui_cursor(16, 16, w - 32, h - 16);
    WarCompareBarModel m = sample_model(scenario);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    fill_rect(hdc, (RECT){0, 0, w, h}, RGB(28, 34, 38));
    panel_war_compare_bar_draw(hdc, &cursor, &m);
    file = fopen(path, "wb");
    if (!file) {
        SelectObject(hdc, old_bitmap);
        DeleteObject(bitmap);
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&fh, sizeof(fh), 1, file);
    fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return 1;
}
