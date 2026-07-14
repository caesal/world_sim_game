#include "sim/plague_diagnostics.h"

#include "data/plague_names.h"
#include "sim/plague_state.h"

#include <string.h>

PlagueDiagnosticsSnapshot plague_diagnostics_state;

void plague_diagnostics_reset(void) {
    memset(&plague_diagnostics_state, 0, sizeof(plague_diagnostics_state));
}

static void copy_active(int absolute_month, const PlagueEpisodeState *episode) {
    PlagueActiveDiagnostics *out = &plague_diagnostics_state.active;
    memset(out, 0, sizeof(*out));
    if (!episode || !episode->active) return;
    out->active = 1;
    out->episode_id = episode->episode_id;
    out->size = episode->size;
    out->severity = episode->severity;
    out->start_month = episode->start_month;
    out->age_months = absolute_month - episode->start_month;
    if (out->age_months < 0) out->age_months = 0;
    out->spores_initial = episode->spores_initial;
    out->spores_remaining = episode->spores_remaining;
    out->active_cities = episode->active_city_count;
    out->infected_cities = episode->ever_infected_city_count;
    out->maximum_generation = episode->maximum_generation_reached;
    out->affected_countries = episode->ever_affected_country_count;
    out->total_deaths = episode->total_deaths;
    plague_names_format(episode->name_id, episode->name_cycle, 0,
                        out->name_en, sizeof(out->name_en));
    plague_names_format(episode->name_id, episode->name_cycle, 1,
                        out->name_zh, sizeof(out->name_zh));
}

static void copy_history(int index, const PlagueEpisodeHistory *history) {
    PlagueHistoryDiagnostics *out = &plague_diagnostics_state.history[index];
    memset(out, 0, sizeof(*out));
    out->episode_id = history->episode_id;
    out->size = history->size;
    out->severity = history->severity;
    out->start_month = history->start_month;
    out->end_month = history->end_month;
    out->duration_months = history->duration_months;
    out->spores_initial = history->spores_initial;
    out->spores_used = history->spores_used;
    out->infected_cities = history->infected_city_count;
    out->affected_countries = history->affected_country_count;
    out->total_deaths = history->total_deaths;
    plague_names_format(history->name_id, history->name_cycle, 0,
                        out->name_en, sizeof(out->name_en));
    plague_names_format(history->name_id, history->name_cycle, 1,
                        out->name_zh, sizeof(out->name_zh));
}

void plague_diagnostics_refresh(int absolute_month) {
    const PlagueModelState *model = plague_state_get();
    int i;
    copy_active(absolute_month, &model->episode);
    plague_diagnostics_state.history_count = model->history_count;
    if (plague_diagnostics_state.history_count > PLAGUE_RECENT_HISTORY_CAP) {
        plague_diagnostics_state.history_count = PLAGUE_RECENT_HISTORY_CAP;
    }
    for (i = 0; i < plague_diagnostics_state.history_count; i++) {
        PlagueEpisodeHistory history;
        if (plague_state_recent_history(i, &history)) copy_history(i, &history);
    }
    for (; i < PLAGUE_RECENT_HISTORY_CAP; i++) {
        memset(&plague_diagnostics_state.history[i], 0,
               sizeof(plague_diagnostics_state.history[i]));
    }
}
