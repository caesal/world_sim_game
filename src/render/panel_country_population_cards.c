#include "render/panel_country_population_cards.h"

#include "render/render_panel_internal.h"
#include "sim/population_military.h"
#include "ui/ui_clay_widgets.h"

#include <stdio.h>

static void format_split(char *out, size_t out_size, int male, int female) {
    char male_text[24];
    char female_text[24];
    format_metric_value(male, male_text, sizeof(male_text));
    format_metric_value(female, female_text, sizeof(female_text));
    snprintf(out, out_size, "%s %s | %s %s",
             tr("M", "男"), male_text, tr("F", "女"), female_text);
}

static int worker_male(PopulationSummary summary) {
    return summary.cohorts[POP_AGE_18_24].male +
           summary.cohorts[POP_AGE_25_39].male +
           summary.cohorts[POP_AGE_40_54].male +
           summary.cohorts[POP_AGE_55_64].male;
}

static int worker_female(PopulationSummary summary) {
    return summary.cohorts[POP_AGE_18_24].female +
           summary.cohorts[POP_AGE_25_39].female +
           summary.cohorts[POP_AGE_40_54].female +
           summary.cohorts[POP_AGE_55_64].female;
}

static int recruit_male(PopulationSummary summary) {
    return summary.cohorts[POP_AGE_25_39].male +
           summary.cohorts[POP_AGE_40_54].male +
           summary.cohorts[POP_AGE_55_64].male;
}

static int recruit_female(PopulationSummary summary) {
    return summary.cohorts[POP_AGE_25_39].female +
           summary.cohorts[POP_AGE_40_54].female +
           summary.cohorts[POP_AGE_55_64].female;
}

void draw_population_structure_cards(HDC hdc, UiCursor *cursor,
                                     PopulationSummary summary,
                                     int current_soldiers) {
    int gap = 6;
    int w = (cursor->width - gap * 2) / 3;
    int h = 34;
    int army_male = 0;
    int army_female = 0;
    char split[64];
    RECT area = ui_take_rect(cursor, h * 2 + gap + 4);
    RECT r = {area.left, area.top, area.left + w, area.top + h};

    ui_clay_draw_metric_chip_int(hdc, r, ICON_POPULATION,
                                 metric_label("Children", "儿童"),
                                 summary.children, RGB(118, 143, 95));
    r.left += w + gap; r.right += w + gap;
    ui_clay_draw_metric_chip_int(hdc, r, ICON_MIGRATION,
                                 metric_label("Fertile", "育龄"),
                                 summary.fertile, RGB(164, 102, 141));
    r.left += w + gap; r.right = area.right;
    ui_clay_draw_metric_chip_int(hdc, r, ICON_HABITABILITY,
                                 metric_label("Elder", "老人"),
                                 summary.elder, RGB(188, 154, 88));

    r = (RECT){area.left, area.top + h + gap, area.left + w, area.top + h * 2 + gap};
    format_split(split, sizeof(split), worker_male(summary), worker_female(summary));
    ui_clay_draw_metric_chip_text(hdc, r, ICON_PRODUCTION,
                                  metric_label("Workers", "劳力"),
                                  split, RGB(83, 123, 166));
    r.left += w + gap; r.right += w + gap;
    format_split(split, sizeof(split), recruit_male(summary), recruit_female(summary));
    ui_clay_draw_metric_chip_text(hdc, r, ICON_MILITARY,
                                  metric_label("Recruitable", "可征召"),
                                  split, RGB(158, 74, 62));
    r.left += w + gap; r.right = area.right;
    population_military_split_current_soldiers(summary, current_soldiers,
                                               &army_male, &army_female);
    format_split(split, sizeof(split), army_male, army_female);
    ui_clay_draw_metric_chip_text(hdc, r, ICON_MILITARY,
                                  metric_label("Army", "军队"),
                                  split, RGB(204, 172, 112));
}
