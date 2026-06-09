#include "render_panel_internal.h"

#include "render/snapshot_ui.h"
#include "sim/population.h"

static const char *age_band_label(int band) {
    static const char *labels[POP_COHORT_COUNT] = {
        "0-4", "5-17", "18-24", "25-39", "40-54", "55-64", "65-74", "75+"
    };

    return labels[band];
}

static const int cohort_display_width_years[POP_COHORT_COUNT] = {
    5, 13, 7, 15, 15, 10, 10, 12
};

static int cohort_density_value(int value, int band) {
    if (band < 0 || band >= POP_COHORT_COUNT) return value;
    return value / max(1, cohort_display_width_years[band]);
}

static int max_population_side(PopulationSummary summary) {
    int max_value = 1;
    int i;

    for (i = 0; i < POP_COHORT_COUNT; i++) {
        int male = cohort_density_value(summary.cohorts[i].male, i);
        int female = cohort_density_value(summary.cohorts[i].female, i);
        if (male > max_value) max_value = male;
        if (female > max_value) max_value = female;
    }
    return max_value;
}

static void draw_population_bar(HDC hdc, RECT rect, int value, int max_value, int left_side, COLORREF color) {
    RECT bar = rect;
    int width = (rect.right - rect.left) * value / max_value;

    fill_rect(hdc, rect, RGB(34, 42, 50));
    if (left_side) bar.left = rect.right - width;
    else bar.right = rect.left + width;
    fill_rect(hdc, bar, color);
}

int draw_population_pyramid_summary_labeled(HDC hdc, RECT client, int x, int y, int width,
                                            PopulationSummary summary, HFONT body_font,
                                            const char *pressure_label) {
    int center = x + width / 2;
    int bar_w = (width - 116) / 2;
    int row_h = 15;
    int max_value = max_population_side(summary);
    int i;
    char text[160];

    (void)client;
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, y, tr("Age Structure", "年龄结构"), RGB(205, 214, 222));
    y += 20;
    draw_text_line(hdc, x, y, tr("Age-normalized density", "按年龄段年均人数"), RGB(148, 160, 172));
    y += 16;
    draw_text_line(hdc, center - bar_w / 2 - 24, y, tr("Male", "男"), RGB(178, 190, 202));
    draw_text_line(hdc, center + 40, y, tr("Female", "女"), RGB(178, 190, 202));
    y += 18;

    for (i = POP_COHORT_COUNT - 1; i >= 0; i--) {
        RECT male = {center - bar_w - 38, y + 2, center - 38, y + row_h - 1};
        RECT female = {center + 38, y + 2, center + bar_w + 38, y + row_h - 1};
        RECT label = {center - 34, y, center + 34, y + row_h};

        draw_population_bar(hdc, male, cohort_density_value(summary.cohorts[i].male, i),
                            max_value, 1, RGB(83, 123, 166));
        draw_population_bar(hdc, female, cohort_density_value(summary.cohorts[i].female, i),
                            max_value, 0, RGB(164, 102, 141));
        draw_center_text(hdc, label, age_band_label(i), RGB(218, 224, 230));
        y += row_h + 2;
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

int draw_population_pyramid_summary(HDC hdc, RECT client, int x, int y, int width,
                                    PopulationSummary summary, HFONT body_font) {
    return draw_population_pyramid_summary_labeled(hdc, client, x, y, width, summary, body_font,
                                                   tr("Pressure", "压力"));
}

int draw_population_pyramid(HDC hdc, RECT client, int x, int y, int width, int civ_id, HFONT body_font) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    PopulationSummary empty = {0};
    return draw_population_pyramid_summary(hdc, client, x, y, width,
                                           civ ? civ->population_summary : empty, body_font);
}
