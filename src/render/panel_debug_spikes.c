#include "render/panel_debug_spikes.h"

#include "core/profiler.h"
#include "render_panel_internal.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"

#include <stdio.h>

static COLORREF spike_color(int ms) {
    if (ms >= 120) return RGB(218, 92, 78);
    if (ms >= 50) return RGB(218, 178, 78);
    return ui_theme_color(UI_COLOR_TEXT_MUTED);
}

static const char *mode_name(int mode) {
    switch (mode) {
        case DISPLAY_POLITICAL: return "Country";
        case DISPLAY_ALLIANCE: return "Alliance";
        case DISPLAY_REGIONS: return "Province";
        case DISPLAY_ROUTE_POTENTIAL: return "Routes";
        case DISPLAY_GEOGRAPHY: return "Geo";
        case DISPLAY_CLIMATE: return "Climate";
        default: return "Other";
    }
}

static void spike_row(HDC hdc, UiCursor *cursor, const char *label,
                      const char *value, COLORREF color) {
    char text[256];
    RECT rect;
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(text, sizeof(text), "%s: %s", label, value);
    rect = (RECT){cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 20};
    draw_text_rect(hdc, rect, text, color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 21;
}

static void draw_spike_entry(HDC hdc, UiCursor *cursor, const char *label,
                             const ProfilerSpikeEntry *entry) {
    char text[220];
    if (!entry || entry->duration_ms <= 0) {
        spike_row(hdc, cursor, label, "none", ui_theme_color(UI_COLOR_TEXT_MUTED));
        return;
    }
    snprintf(text, sizeof(text), "%s %d ms sim %d/%02d shown %d/%02d q %d age %d",
              entry->phase, entry->duration_ms, entry->sim_year, entry->sim_month,
             entry->presented_year, entry->presented_month,
             entry->pending_presented_months, entry->age_ms);
    spike_row(hdc, cursor, label, text, spike_color(entry->duration_ms));
}

static void draw_render_phase_entry(HDC hdc, UiCursor *cursor, int index,
                                    const ProfilerRenderPhaseEntry *entry) {
    char label[32], text[220];
    snprintf(label, sizeof(label), "Render spike %d", index + 1);
    if (!entry || entry->duration_ms <= 0) {
        spike_row(hdc, cursor, label, "none", ui_theme_color(UI_COLOR_TEXT_MUTED));
        return;
    }
    snprintf(text, sizeof(text), "%s %d ms mode %s age %d",
             entry->phase, entry->duration_ms, mode_name(entry->display_mode),
             entry->age_ms);
    spike_row(hdc, cursor, label, text, spike_color(entry->duration_ms));
}

void draw_debug_spike_rows(HDC hdc, UiCursor *cursor) {
    RuntimeProfilerSnapshot perf;
    char text[220];
    int i;
    profiler_snapshot(&perf);
    ui_section(hdc, cursor, tr("Spike Trace", "Spike Trace"));
    draw_spike_entry(hdc, cursor, tr("Last spike", "Last spike"), &perf.last_spike);
    snprintf(text, sizeof(text), "auto %d / speed %d / static work %d cur %d present %d",
             perf.last_spike.auto_run_active, perf.last_spike.speed_index,
             perf.last_spike.static_needs_work,
             perf.last_spike.static_presented_current,
             perf.last_spike.static_presentable);
    spike_row(hdc, cursor, tr("Spike context", "Spike context"), text,
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    for (i = 0; i < perf.recent_spike_count; i++) {
        char label[32];
        snprintf(label, sizeof(label), "Recent spike %d", i + 1);
        draw_spike_entry(hdc, cursor, label, &perf.recent_spikes[i]);
    }
    for (i = 0; i < perf.recent_render_phase_count; i++) {
        draw_render_phase_entry(hdc, cursor, i, &perf.recent_render_phases[i]);
    }
    snprintf(text, sizeof(text), "Al %d/%d Ct %d/%d Pr %d/%d",
             perf.render_mode_avg_ms[DISPLAY_ALLIANCE],
             perf.render_mode_peak_ms[DISPLAY_ALLIANCE],
             perf.render_mode_avg_ms[DISPLAY_POLITICAL],
             perf.render_mode_peak_ms[DISPLAY_POLITICAL],
             perf.render_mode_avg_ms[DISPLAY_REGIONS],
             perf.render_mode_peak_ms[DISPLAY_REGIONS]);
    spike_row(hdc, cursor, tr("Render avg/peak A/C/P", "Render avg/peak A/C/P"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "Rt %d/%d Ge %d/%d Cl %d/%d",
             perf.render_mode_avg_ms[DISPLAY_ROUTE_POTENTIAL],
             perf.render_mode_peak_ms[DISPLAY_ROUTE_POTENTIAL],
             perf.render_mode_avg_ms[DISPLAY_GEOGRAPHY],
             perf.render_mode_peak_ms[DISPLAY_GEOGRAPHY],
             perf.render_mode_avg_ms[DISPLAY_CLIMATE],
             perf.render_mode_peak_ms[DISPLAY_CLIMATE]);
    spike_row(hdc, cursor, tr("Render avg/peak R/G/Cl", "Render avg/peak R/G/Cl"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "fill %d/%s labels %d/%d side %d",
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_FILL],
             mode_name(perf.render_subphase_peak_display[PROFILER_RENDER_SUB_FILL]),
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_COUNTRY_LABELS],
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_ALLIANCE_LABELS],
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_SIDE_PANEL]);
    spike_row(hdc, cursor, tr("Render peaks fill/labels", "Render peaks fill/labels"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "static %d border %d city %d route %d blit %d",
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_STATIC_SCENE],
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_BORDERS],
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_CITY_OVERLAY],
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_ROUTE_OVERLAY],
             perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_BACKBUFFER]);
    spike_row(hdc, cursor, tr("Render peaks static/etc", "Render peaks static/etc"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "sim %d / present %d / publish %d",
             perf.spike_category_peak_ms[PROFILER_SPIKE_SIMULATION],
             perf.spike_category_peak_ms[PROFILER_SPIKE_PRESENTATION],
             perf.spike_category_peak_ms[PROFILER_SPIKE_SNAPSHOT]);
    spike_row(hdc, cursor, tr("Peaks sim/present/snap", "Peaks sim/present/snap"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "static %d / city %d / label %d",
             perf.spike_category_peak_ms[PROFILER_SPIKE_STATIC_CACHE],
             perf.spike_category_peak_ms[PROFILER_SPIKE_CITY_OVERLAY],
             perf.spike_category_peak_ms[PROFILER_SPIKE_LABEL_CACHE]);
    spike_row(hdc, cursor, tr("Peaks static/city/label", "Peaks static/city/label"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "exp %d / route %d / dip %d / ally %d",
             perf.spike_category_peak_ms[PROFILER_SPIKE_EXPANSION],
             perf.spike_category_peak_ms[PROFILER_SPIKE_ROUTE_MARITIME],
             perf.spike_category_peak_ms[PROFILER_SPIKE_ANNUAL_DIPLOMACY],
             perf.spike_category_peak_ms[PROFILER_SPIKE_ANNUAL_ALLIANCE]);
    spike_row(hdc, cursor, tr("Peaks exp/route/year", "Peaks exp/route/year"),
              text, ui_theme_color(UI_COLOR_TEXT_MUTED));
}
