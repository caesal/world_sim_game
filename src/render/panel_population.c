#include "render_panel_internal.h"

#include "render/snapshot_ui.h"

static const int display_row_first[] = {
    75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0
};
static const int display_row_last[] = {
    100, 74, 69, 64, 59, 54, 49, 44, 39, 34, 29, 24, 19, 14, 9, 4
};

#define DISPLAY_ROW_COUNT ((int)(sizeof(display_row_first) / sizeof(display_row_first[0])))

static const char *display_row_label(int row) {
    static const char *labels[] = {
        "75+", "70-74", "65-69", "60-64", "55-59", "50-54",
        "45-49", "40-44", "35-39", "30-34", "25-29", "20-24",
        "15-19", "10-14", "5-9", "0-4"
    };
    return labels[row];
}

static int display_range_value(const int *ages, int row) {
    int first = display_row_first[row];
    int last = display_row_last[row];
    int total = 0;
    int age;
    for (age = first; age <= last && age < POP_DISPLAY_AGE_COUNT; age++) {
        total += ages[age];
    }
    return total;
}

static int display_density_value(const int *ages, int row) {
    int width = row == 0 ? 12 : 5;
    return (display_range_value(ages, row) + width / 2) / width;
}

static int max_population_side(const PopulationDisplayCohorts *display) {
    int max_value = 1;
    int row;
    for (row = 0; row < DISPLAY_ROW_COUNT; row++) {
        int male = display_density_value(display->male, row);
        int female = display_density_value(display->female, row);
        if (male > max_value) max_value = male;
        if (female > max_value) max_value = female;
    }
    return max_value;
}

static void draw_population_bar(HDC hdc, RECT rect, int value, int max_value,
                                int left_side, COLORREF color) {
    RECT bar = rect;
    int width = (rect.right - rect.left) * value / max(1, max_value);

    fill_rect(hdc, rect, RGB(34, 42, 50));
    if (left_side) bar.left = rect.right - width;
    else bar.right = rect.left + width;
    fill_rect(hdc, bar, color);
}

int draw_population_display_pyramid_summary_labeled(HDC hdc, RECT client, int x, int y,
                                                    int width,
                                                    const PopulationDisplayCohorts *display,
                                                    PopulationSummary summary,
                                                    HFONT body_font,
                                                    const char *pressure_label) {
    PopulationDisplayCohorts fallback;
    const PopulationDisplayCohorts *source = display;
    int center = x + width / 2;
    int bar_w = (width - 116) / 2;
    int row_h = 10;
    int bar_h = 7;
    int max_value;
    int row;
    char text[160];

    (void)client;
    if (!source || population_display_total(source) <= 0) {
        population_display_uniform_from_summary(&fallback, summary);
        source = &fallback;
    }
    max_value = max_population_side(source);
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, y, tr("Age Structure", "年龄结构"), RGB(205, 214, 222));
    y += 20;
    draw_text_line(hdc, x, y, tr("Age bands", "年龄段"), RGB(148, 160, 172));
    y += 16;
    draw_text_line(hdc, center - bar_w / 2 - 24, y, tr("Male", "男"), RGB(178, 190, 202));
    draw_text_line(hdc, center + 40, y, tr("Female", "女"), RGB(178, 190, 202));
    y += 18;

    for (row = 0; row < DISPLAY_ROW_COUNT; row++) {
        RECT male = {center - bar_w - 38, y + 2, center - 38, y + 2 + bar_h};
        RECT female = {center + 38, y + 2, center + bar_w + 38, y + 2 + bar_h};
        RECT label = {center - 34, y, center + 34, y + row_h};

        draw_population_bar(hdc, male, display_density_value(source->male, row),
                            max_value, 1, RGB(83, 123, 166));
        draw_population_bar(hdc, female, display_density_value(source->female, row),
                            max_value, 0, RGB(164, 102, 141));
        draw_center_text(hdc, label, display_row_label(row), RGB(218, 224, 230));
        y += row_h;
    }
    y += 6;
    snprintf(text, sizeof(text), "%s %d  %s %d  %s %d",
             tr("Children", "儿童"), summary.children,
             tr("Working", "劳力"), summary.working,
             tr("Elder", "老人"), summary.elder);
    draw_text_line(hdc, x, y, text, RGB(178, 190, 202));
    y += 18;
    snprintf(text, sizeof(text), "%s %d  %s %d  %s %d%%",
             tr("Fertile", "育龄"), summary.fertile,
             tr("Recruitable", "征召"), summary.recruitable,
             pressure_label ? pressure_label : tr("Pressure", "压力"), summary.pressure);
    draw_text_line(hdc, x, y, text, RGB(178, 190, 202));
    return y + 24;
}

int draw_population_pyramid_summary_labeled(HDC hdc, RECT client, int x, int y, int width,
                                            PopulationSummary summary, HFONT body_font,
                                            const char *pressure_label) {
    return draw_population_display_pyramid_summary_labeled(hdc, client, x, y, width,
                                                           NULL, summary, body_font,
                                                           pressure_label);
}

int draw_population_pyramid_summary(HDC hdc, RECT client, int x, int y, int width,
                                    PopulationSummary summary, HFONT body_font) {
    return draw_population_pyramid_summary_labeled(hdc, client, x, y, width, summary,
                                                   body_font, tr("Pressure", "压力"));
}

int draw_population_pyramid(HDC hdc, RECT client, int x, int y, int width,
                            int civ_id, HFONT body_font) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    PopulationSummary empty = {0};
    return draw_population_display_pyramid_summary_labeled(
        hdc, client, x, y, width, civ ? &civ->population_display : NULL,
        civ ? civ->population_summary : empty, body_font, tr("Pressure", "压力"));
}
