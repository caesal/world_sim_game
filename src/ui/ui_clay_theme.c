#include "ui/ui_clay_theme.h"

static COLORREF clay_mix(COLORREF a, COLORREF b, int b_percent) {
    int a_percent = 100 - b_percent;
    return RGB((GetRValue(a) * a_percent + GetRValue(b) * b_percent) / 100,
               (GetGValue(a) * a_percent + GetGValue(b) * b_percent) / 100,
               (GetBValue(a) * a_percent + GetBValue(b) * b_percent) / 100);
}

static UiClayStyle clay_base_style(UiClaySurface surface) {
    UiClayStyle style = {
        RGB(50, 59, 61), RGB(88, 101, 104), RGB(24, 29, 31),
        RGB(104, 118, 119), RGB(244, 244, 236), RGB(196, 204, 202),
        18, 12, 8, 4
    };

    switch (surface) {
        case UI_CLAY_SURFACE_SHELL:
            style.fill = RGB(42, 49, 52);
            style.border = RGB(78, 90, 94);
            style.radius = 16;
            style.padding_x = 10;
            style.padding_y = 8;
            break;
        case UI_CLAY_SURFACE_PANEL:
            style.fill = RGB(49, 58, 60);
            style.border = RGB(92, 106, 108);
            style.radius = 22;
            style.padding_x = 14;
            style.padding_y = 12;
            break;
        case UI_CLAY_SURFACE_CARD:
            style.fill = RGB(58, 67, 68);
            style.border = RGB(104, 117, 117);
            style.radius = 18;
            break;
        case UI_CLAY_SURFACE_PILL:
            style.fill = RGB(72, 82, 84);
            style.border = RGB(112, 126, 128);
            style.radius = 28;
            style.padding_x = 14;
            style.padding_y = 6;
            break;
        case UI_CLAY_SURFACE_TAB:
            style.fill = RGB(58, 67, 69);
            style.border = RGB(101, 115, 116);
            style.radius = 14;
            style.padding_x = 12;
            style.padding_y = 6;
            style.shadow_offset = 3;
            break;
        default:
            break;
    }
    return style;
}

UiClayStyle ui_clay_style(UiClaySurface surface, UiClayState state) {
    UiClayStyle style = clay_base_style(surface);

    switch (state) {
        case UI_CLAY_STATE_HOVER:
            style.fill = clay_mix(style.fill, style.highlight, 18);
            style.border = clay_mix(style.border, RGB(154, 174, 174), 22);
            break;
        case UI_CLAY_STATE_PRESSED:
            style.fill = clay_mix(style.fill, style.shadow, 24);
            style.shadow_offset = max(1, style.shadow_offset - 2);
            break;
        case UI_CLAY_STATE_SELECTED:
            style.fill = RGB(112, 105, 138);
            style.border = RGB(174, 164, 202);
            style.highlight = RGB(148, 139, 176);
            style.shadow = RGB(39, 36, 50);
            style.text = RGB(255, 248, 226);
            break;
        case UI_CLAY_STATE_DISABLED:
            style.fill = clay_mix(style.fill, RGB(34, 38, 40), 45);
            style.border = clay_mix(style.border, RGB(48, 54, 56), 55);
            style.highlight = clay_mix(style.highlight, style.fill, 70);
            style.text = RGB(132, 142, 142);
            style.text_muted = RGB(106, 116, 116);
            style.shadow_offset = 2;
            break;
        case UI_CLAY_STATE_NORMAL:
        default:
            break;
    }
    return style;
}

COLORREF ui_clay_text_color(UiClayState state) {
    return ui_clay_style(UI_CLAY_SURFACE_PANEL, state).text;
}

COLORREF ui_clay_muted_text_color(void) {
    return clay_base_style(UI_CLAY_SURFACE_PANEL).text_muted;
}
