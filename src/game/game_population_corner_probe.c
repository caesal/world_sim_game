#include "game/game_population_corner_probe.h"

#include "core/game_types.h"
#include "sim/civilization_slots.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/population_aging.h"
#include "sim/population_diagnostics.h"
#include "sim/population_display_cohorts.h"
#include "sim/population_military.h"
#include "sim/simulation_month.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_internal.h"

#include <string.h>

static int cohort_total(PopulationCohort cohort) {
    return cohort.male + cohort.female;
}

static int display_range_total(const PopulationDisplayCohorts *display,
                               int first, int last) {
    int total = 0;
    int age;
    for (age = first; display && age <= last && age < POP_DISPLAY_AGE_COUNT; age++) {
        total += display->male[age] + display->female[age];
    }
    return total;
}

static int male_25_64(void) {
    return cities[0].population_cohorts[POP_AGE_25_39].male +
           cities[0].population_cohorts[POP_AGE_40_54].male +
           cities[0].population_cohorts[POP_AGE_55_64].male;
}

static int female_25_64(void) {
    return cities[0].population_cohorts[POP_AGE_25_39].female +
           cities[0].population_cohorts[POP_AGE_40_54].female +
           cities[0].population_cohorts[POP_AGE_55_64].female;
}

static int male_18_64(void) {
    return cities[0].population_cohorts[POP_AGE_18_24].male +
           cities[0].population_cohorts[POP_AGE_25_39].male +
           cities[0].population_cohorts[POP_AGE_40_54].male +
           cities[0].population_cohorts[POP_AGE_55_64].male;
}

static int female_18_64(void) {
    return cities[0].population_cohorts[POP_AGE_18_24].female +
           cities[0].population_cohorts[POP_AGE_25_39].female +
           cities[0].population_cohorts[POP_AGE_40_54].female +
           cities[0].population_cohorts[POP_AGE_55_64].female;
}

static void reset_one_city(void) {
    memset(civs, 0, sizeof(civs));
    memset(cities, 0, sizeof(cities));
    civ_count = 1;
    city_count = 1;
    civilization_reset_slot_state(0);
    civs[0].alive = 1;
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].population_ready = 1;
    population_age_reset_all();
}

static void sync_fixture(void) {
    population_sync_all();
}

static void write_army_probe(FILE *file) {
    int base_a, current_a, base_b, base_with_18_24, base_mil0, base_mil10;
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_25_39].male = 300000;
    cities[0].population_cohorts[POP_AGE_40_54].male = 250000;
    cities[0].population_cohorts[POP_AGE_55_64].male = 100000;
    sync_fixture();
    base_a = population_military_base_soldiers_for_civ(0);
    current_a = population_military_current_soldiers_for_civ(0, 5000);
    fprintf(file, "army_male_only base=%d expected=16250 current_after_5000=%d expected=11250\n",
            base_a, current_a);

    reset_one_city();
    cities[0].population_cohorts[POP_AGE_25_39].male = 1000;
    cities[0].population_cohorts[POP_AGE_25_39].female = 1000;
    sync_fixture();
    base_b = population_military_base_soldiers_for_civ(0);
    cities[0].population_cohorts[POP_AGE_18_24].male = 1000000;
    sync_fixture();
    base_with_18_24 = population_military_base_soldiers_for_civ(0);
    civs[0].military = 0;
    base_mil0 = population_military_base_soldiers_for_civ(0);
    civs[0].military = 10;
    base_mil10 = population_military_base_soldiers_for_civ(0);
    fprintf(file, "army_mixed base=%d expected=35 base_with_18_24=%d expected=35 "
                  "mil0=%d mil10=%d unchanged=%d\n",
            base_b, base_with_18_24, base_mil0, base_mil10,
            base_mil0 == base_mil10);

    reset_one_city();
    cities[0].population_cohorts[POP_AGE_25_39].male = 10;
    sync_fixture();
    fprintf(file, "army_tiny_no_minimum base=%d expected=0\n",
            population_military_base_soldiers_for_civ(0));
}

static void write_casualty_probe(FILE *file) {
    int before_male, before_female, after_male, after_female, removed;
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_25_39].male = 10000;
    cities[0].population_cohorts[POP_AGE_25_39].female = 10000;
    sync_fixture();
    before_male = male_25_64();
    before_female = female_25_64();
    removed = population_apply_casualties(0, 3500);
    after_male = male_25_64();
    after_female = female_25_64();
    fprintf(file, "casualty_weighted removed=%d male_loss=%d female_loss=%d "
                  "expected_ratio=2500:1000 no_negative=%d\n",
            removed, before_male - after_male, before_female - after_female,
            after_male >= 0 && after_female >= 0);
}

static void write_population_card_probe(FILE *file) {
    PopulationSummary summary;
    int army_male = 0;
    int army_female = 0;
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_18_24].male = 10000;
    cities[0].population_cohorts[POP_AGE_18_24].female = 9000;
    cities[0].population_cohorts[POP_AGE_25_39].male = 300000;
    cities[0].population_cohorts[POP_AGE_25_39].female = 200000;
    cities[0].population_cohorts[POP_AGE_40_54].male = 250000;
    cities[0].population_cohorts[POP_AGE_40_54].female = 180000;
    cities[0].population_cohorts[POP_AGE_55_64].male = 100000;
    cities[0].population_cohorts[POP_AGE_55_64].female = 80000;
    sync_fixture();
    summary = population_country_summary(0);
    population_military_split_current_soldiers(summary, 11250, &army_male, &army_female);
    fprintf(file, "population_cards workers_m=%d workers_f=%d recruit_m=%d recruit_f=%d "
                  "army_m=%d army_f=%d army_total=%d expected_total=11250\n",
            male_18_64(), female_18_64(), male_25_64(), female_25_64(),
            army_male, army_female, army_male + army_female);
    population_military_split_current_soldiers(summary, 0, &army_male, &army_female);
    fprintf(file, "population_cards_army_zero recruit_m=%d recruit_f=%d "
                  "army_m=%d army_f=%d army_total=%d current_soldiers=0\n",
            male_25_64(), female_25_64(), army_male, army_female,
            army_male + army_female);
}

static void write_display_band_calibration_probe(FILE *file) {
    PopulationDisplayCohorts display;
    int real_elder, real_75_plus, display_elder, display_75_plus;
    int display_5_17, shape_preserved;
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_5_17].male = 30;
    cities[0].population_cohorts[POP_AGE_5_17].female = 30;
    cities[0].population_cohorts[POP_AGE_65_74].male = 12;
    cities[0].population_cohorts[POP_AGE_65_74].female = 8;
    cities[0].population_cohorts[POP_AGE_75_PLUS].male = 1;
    cities[0].population_cohorts[POP_AGE_75_PLUS].female = 1;
    sync_fixture();
    memset(&display, 0, sizeof(display));
    display.male[5] = 90;
    display.male[17] = 10;
    display.male[75] = 120;
    display.female[100] = 180;
    population_display_replace_city_for_validation(0, &display);
    population_display_calibrate_city(0);
    population_display_city_cached(0, &display);
    real_elder = cohort_total(cities[0].population_cohorts[POP_AGE_65_74]) +
                 cohort_total(cities[0].population_cohorts[POP_AGE_75_PLUS]);
    real_75_plus = cohort_total(cities[0].population_cohorts[POP_AGE_75_PLUS]);
    display_elder = display_range_total(&display, 65, POP_DISPLAY_MAX_AGE);
    display_75_plus = display_range_total(&display, 75, POP_DISPLAY_MAX_AGE);
    display_5_17 = display_range_total(&display, 5, 17);
    shape_preserved = display.male[5] > display.male[17];
    fprintf(file, "display_band_calibration real_elder=%d display65plus=%d "
                  "real75plus=%d display75plus=%d display5_17=%d "
                  "shape_preserved=%d pass=%d\n",
            real_elder, display_elder, real_75_plus, display_75_plus,
            display_5_17, shape_preserved,
            real_elder == display_elder && real_75_plus == display_75_plus &&
            display_5_17 == 60 && shape_preserved);
}

static void write_support_separation_probe(FILE *file) {
    int own_before, callable, own_with_temp;
    reset_one_city();
    civ_count = 2;
    civilization_reset_slot_state(1);
    civs[1].alive = 1;
    city_count = 2;
    cities[1].alive = 1;
    cities[1].owner = 1;
    cities[1].population_ready = 1;
    cities[0].population_cohorts[POP_AGE_25_39].male = 10000;
    cities[1].population_cohorts[POP_AGE_25_39].male = 10000;
    sync_fixture();
    own_before = war_current_soldiers_for_civ(0);
    vassal_make(0, 1, 80);
    callable = vassal_total_callable_soldiers(0);
    memset(active_wars, 0, sizeof(active_wars));
    active_wars[0].active = 1;
    active_wars[0].attacker = 0;
    active_wars[0].defender = 1;
    active_wars[0].temporary_soldiers_a = 10000;
    own_with_temp = war_current_soldiers_for_civ(0);
    fprintf(file, "support_separate own=%d callable=%d own_with_merc_temp=%d "
                  "vassal_separate=%d mercenary_separate=%d\n",
            own_before, callable, own_with_temp, callable > 0,
            own_with_temp == own_before);
    memset(active_wars, 0, sizeof(active_wars));
}

static void write_fractional_birth_probe(FILE *file) {
    PopulationSummary summary;
    PopulationDiagnostics diag;
    TerrainStats stats;
    memset(&summary, 0, sizeof(summary));
    memset(&stats, 0, sizeof(stats));
    summary.cohorts[POP_AGE_18_24].male = 16;
    summary.cohorts[POP_AGE_18_24].female = 16;
    summary.cohorts[POP_AGE_25_39].male = 25;
    summary.cohorts[POP_AGE_25_39].female = 25;
    summary.cohorts[POP_AGE_40_54].male = 16;
    summary.cohorts[POP_AGE_40_54].female = 16;
    summary.total = 315;
    summary.carrying_capacity = 1000;
    stats.food = 4;
    stats.water = 4;
    stats.habitability = 4;
    diag = population_diagnostics_for_summary(-1, summary, stats);
    fprintf(file, "tiny_births total=315 births_x100=%d births_int=%d "
                  "natural_x100=%d net_x100=%d\n",
            diag.estimated_monthly_births_x100, diag.estimated_monthly_births,
            diag.estimated_natural_age_deaths_x100,
            diag.estimated_net_monthly_change_x100);
}

static void write_tiny_aging_probe(FILE *file) {
    int years;
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_5_17].male = 1;
    sync_fixture();
    for (years = 0; years < 13; years++) population_age_city_one_year(0);
    fprintf(file, "tiny_aging_after_13y age5_17=%d age18_24=%d moved=%d\n",
            cohort_total(cities[0].population_cohorts[POP_AGE_5_17]),
            cohort_total(cities[0].population_cohorts[POP_AGE_18_24]),
            cohort_total(cities[0].population_cohorts[POP_AGE_18_24]) > 0);
}

static void write_aging_reset_probe(FILE *file) {
    int years;
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_5_17].male = 1;
    sync_fixture();
    for (years = 0; years < 12; years++) population_age_city_one_year(0);
    reset_one_city();
    cities[0].population_cohorts[POP_AGE_5_17].male = 1;
    sync_fixture();
    population_age_city_one_year(0);
    fprintf(file, "aging_reset_no_leak age5_17=%d age18_24=%d no_leak=%d\n",
            cohort_total(cities[0].population_cohorts[POP_AGE_5_17]),
            cohort_total(cities[0].population_cohorts[POP_AGE_18_24]),
            cohort_total(cities[0].population_cohorts[POP_AGE_5_17]) == 1 &&
            cohort_total(cities[0].population_cohorts[POP_AGE_18_24]) == 0);
}

static void write_single_civ_probe(FILE *file) {
    fprintf(file, "single_civ_auto_run_stop living0=%d living1=%d living2=%d\n",
            simulation_month_should_stop_auto_run_for_living(0),
            simulation_month_should_stop_auto_run_for_living(1),
            simulation_month_should_stop_auto_run_for_living(2));
}

static void write_technology_timing_probe(FILE *file) {
    fprintf(file, "tech_years baseline_innov5=%d expected=120\n",
            technology_required_years_for_values(5, 30, 60, 0));
    fprintf(file, "tech_years innov6=%d expected=115 innov4=%d expected=125\n",
            technology_required_years_for_values(6, 30, 60, 0),
            technology_required_years_for_values(4, 30, 60, 0));
    fprintf(file, "tech_years resources_good=%d expected=110 poor=%d expected=130\n",
            technology_required_years_for_values(5, 36, 60, 0),
            technology_required_years_for_values(5, 23, 60, 0));
    fprintf(file, "tech_years pressure_low=%d expected=115 p81=%d expected=128 p116=%d expected=135\n",
            technology_required_years_for_values(5, 30, 49, 0),
            technology_required_years_for_values(5, 30, 81, 0),
            technology_required_years_for_values(5, 30, 116, 0));
    fprintf(file, "tech_years clamp_fast=%d expected=80 clamp_slow=%d expected=150 late_stage=%d expected=110\n",
            technology_required_years_for_values(20, 36, 49, 0),
            technology_required_years_for_values(-10, 23, 116, 0),
            technology_required_years_for_values(5, 30, 60, 8));
    fprintf(file, "tech_disorder_percent d0=%d d100=%d remainder_path=kept\n",
            disorder_technology_percent(0), disorder_technology_percent(100));
}

void write_population_corner_probes(FILE *file) {
    write_army_probe(file);
    write_casualty_probe(file);
    write_population_card_probe(file);
    write_display_band_calibration_probe(file);
    write_support_separation_probe(file);
    write_fractional_birth_probe(file);
    write_tiny_aging_probe(file);
    write_aging_reset_probe(file);
    write_single_civ_probe(file);
    write_technology_timing_probe(file);
}
