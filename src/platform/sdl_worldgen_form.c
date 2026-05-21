#include "platform/sdl_worldgen_form.h"

#include "core/game_state.h"
#include "data/country_names.h"
#include "game/game.h"
#include "platform/sdl_ui_common.h"
#include "ui/ui_l10n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SDL_FORM_NONE = -1,
    SDL_FORM_INITIAL = 0,
    SDL_FORM_NAME,
    SDL_FORM_SYMBOL,
    SDL_FORM_METRIC_BASE
} SdlWorldgenField;

static int active_field = SDL_FORM_NONE;
static int replace_on_next_text = 0;
static int synced_civ_id = -2;
static char initial_text[8];
static char name_text[NAME_LEN];
static char symbol_text[8];
static char metric_text[WORLDGEN_CORE_METRIC_COUNT][4];

static int field_is_metric(int field) {
    return field >= SDL_FORM_METRIC_BASE &&
           field < SDL_FORM_METRIC_BASE + WORLDGEN_CORE_METRIC_COUNT;
}

static void write_int_text(char *buffer, int buffer_size, int value) {
    snprintf(buffer, (size_t)buffer_size, "%d", value);
}

static void set_defaults(void) {
    int i;
    write_int_text(initial_text, sizeof(initial_text), initial_civ_count);
    snprintf(name_text, sizeof(name_text), "%s", "New Realm");
    snprintf(symbol_text, sizeof(symbol_text), "%s", "N");
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) write_int_text(metric_text[i], sizeof(metric_text[i]), 5);
}

void sdl_worldgen_form_init(void) {
    active_field = SDL_FORM_NONE;
    replace_on_next_text = 0;
    synced_civ_id = -2;
    set_defaults();
}

static char *field_buffer(int field, int *max_bytes, int *numeric_only) {
    if (max_bytes) *max_bytes = 0;
    if (numeric_only) *numeric_only = 0;
    if (field == SDL_FORM_INITIAL) {
        if (max_bytes) *max_bytes = (int)sizeof(initial_text);
        if (numeric_only) *numeric_only = 1;
        return initial_text;
    }
    if (field == SDL_FORM_NAME) {
        if (max_bytes) *max_bytes = (int)sizeof(name_text);
        return name_text;
    }
    if (field == SDL_FORM_SYMBOL) {
        if (max_bytes) *max_bytes = 2;
        return symbol_text;
    }
    if (field_is_metric(field)) {
        if (max_bytes) *max_bytes = (int)sizeof(metric_text[0]);
        if (numeric_only) *numeric_only = 1;
        return metric_text[field - SDL_FORM_METRIC_BASE];
    }
    return NULL;
}

static int read_int_field(char *buffer, int fallback, int min_value, int max_value) {
    char *end = NULL;
    long value = strtol(buffer, &end, 10);
    int normalized = end == buffer ? fallback : (int)value;
    return clamp(normalized, min_value, max_value);
}

void sdl_worldgen_form_apply_initial_count(void) {
    initial_civ_count = read_int_field(initial_text, initial_civ_count, 0, MAX_CIVS);
    write_int_text(initial_text, sizeof(initial_text), initial_civ_count);
}

static int metric_value(int index, int fallback) {
    int value = read_int_field(metric_text[index], fallback, 0, 10);
    write_int_text(metric_text[index], sizeof(metric_text[index]), value);
    return value;
}

static void normalize_active_field(void) {
    if (active_field == SDL_FORM_INITIAL) sdl_worldgen_form_apply_initial_count();
    else if (field_is_metric(active_field)) {
        int idx = active_field - SDL_FORM_METRIC_BASE;
        (void)metric_value(idx, 5);
    }
}

static void activate_field(int field) {
    active_field = field;
    replace_on_next_text = 1;
}

static void normalize_all(void) {
    int i;
    sdl_worldgen_form_apply_initial_count();
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) (void)metric_value(i, 5);
    if (!name_text[0]) snprintf(name_text, sizeof(name_text), "%s", "New Realm");
    if (!symbol_text[0]) snprintf(symbol_text, sizeof(symbol_text), "%c", (char)('A' + civ_count % 26));
}

static int selected_edit_civ_id(void) {
    if (selected_civ >= 0 && selected_civ < civ_count) return selected_civ;
    return -1;
}

static int add_or_apply(int apply) {
    int civ_id;
    normalize_all();
    if (apply) {
        civ_id = selected_edit_civ_id();
        if (civ_id < 0) return 0;
        if (!game_request_edit_selected_civilization(name_text, symbol_text[0],
                metric_value(WORLDGEN_METRIC_MILITARY, 5),
                metric_value(WORLDGEN_METRIC_LOGISTICS, 5),
                metric_value(WORLDGEN_METRIC_GOVERNANCE, 5),
                metric_value(WORLDGEN_METRIC_COHESION, 5),
                metric_value(WORLDGEN_METRIC_PRODUCTION, 5),
                metric_value(WORLDGEN_METRIC_COMMERCE, 5),
                metric_value(WORLDGEN_METRIC_INNOVATION, 5))) return 0;
        if (selected_civ_color_index >= 0) {
            game_request_set_civilization_color_exact(selected_civ, selected_civ_color);
        }
        synced_civ_id = -2;
        return 1;
    }
    civ_id = game_request_add_civilization_from_selection(name_text, symbol_text[0],
            metric_value(WORLDGEN_METRIC_MILITARY, 5),
            metric_value(WORLDGEN_METRIC_LOGISTICS, 5),
            metric_value(WORLDGEN_METRIC_GOVERNANCE, 5),
            metric_value(WORLDGEN_METRIC_COHESION, 5),
            metric_value(WORLDGEN_METRIC_PRODUCTION, 5),
            metric_value(WORLDGEN_METRIC_COMMERCE, 5),
            metric_value(WORLDGEN_METRIC_INNOVATION, 5));
    if (civ_id >= 0 && selected_civ_color_index >= 0) {
        game_request_set_civilization_color_exact(civ_id, selected_civ_color);
    }
    synced_civ_id = -2;
    return civ_id >= 0;
}

void sdl_worldgen_form_sync_selected(const RenderSnapshot *snapshot) {
    const SnapshotCiv *civ;
    if (active_field != SDL_FORM_NONE) return;
    if (!snapshot || selected_civ < 0 || selected_civ >= snapshot->civ_count ||
        !snapshot->civs[selected_civ].alive) {
        if (synced_civ_id != -1) {
            synced_civ_id = -1;
            set_defaults();
        }
        return;
    }
    if (synced_civ_id == selected_civ) return;
    civ = &snapshot->civs[selected_civ];
    snprintf(name_text, sizeof(name_text), "%s", ui_language == UI_LANG_ZH ? civ->name_zh : civ->name_en);
    snprintf(symbol_text, sizeof(symbol_text), "%c", civ->symbol ? civ->symbol : 'N');
    write_int_text(metric_text[WORLDGEN_METRIC_MILITARY], sizeof(metric_text[0]), civ->military);
    write_int_text(metric_text[WORLDGEN_METRIC_LOGISTICS], sizeof(metric_text[0]), civ->logistics);
    write_int_text(metric_text[WORLDGEN_METRIC_GOVERNANCE], sizeof(metric_text[0]), civ->governance);
    write_int_text(metric_text[WORLDGEN_METRIC_COHESION], sizeof(metric_text[0]), civ->cohesion);
    write_int_text(metric_text[WORLDGEN_METRIC_PRODUCTION], sizeof(metric_text[0]), civ->production);
    write_int_text(metric_text[WORLDGEN_METRIC_COMMERCE], sizeof(metric_text[0]), civ->commerce);
    write_int_text(metric_text[WORLDGEN_METRIC_INNOVATION], sizeof(metric_text[0]), civ->innovation);
    selected_civ_color = civ->color;
    selected_civ_color_index = -1;
    synced_civ_id = selected_civ;
}

static int alive_symbol_used(char symbol) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int used = 0;
    int i;
    if (snapshot) {
        for (i = 0; i < snapshot->civ_count; i++) {
            if (snapshot->civs[i].alive && snapshot->civs[i].symbol == symbol) {
                used = 1;
                break;
            }
        }
    }
    render_snapshot_release(snapshot);
    return used;
}

static char random_unused_symbol(void) {
    int start = rnd(26);
    int i;
    for (i = 0; i < 26; i++) {
        char symbol = (char)('A' + (start + i) % 26);
        if (!alive_symbol_used(symbol)) return symbol;
    }
    return (char)('A' + rnd(26));
}

static int random_metric_value(void) {
    int roll = rnd(100);
    if (roll < 75) return 3 + rnd(6);
    if (roll < 88) return rnd(3);
    return 9 + rnd(2);
}

void sdl_worldgen_form_randomize(void) {
    int used[COUNTRY_NAME_COUNT];
    int name_id = -1;
    int i;
    const RenderSnapshot *snapshot;

    memset(used, 0, sizeof(used));
    snapshot = render_snapshot_acquire();
    if (snapshot) {
        for (i = 0; i < snapshot->civ_count; i++) {
            int id = snapshot->civs[i].name_id;
            if (id >= 0 && id < COUNTRY_NAME_COUNT) used[id] = 1;
        }
    }
    render_snapshot_release(snapshot);
    for (i = 0; i < COUNTRY_NAME_COUNT; i++) {
        int candidate = rnd(COUNTRY_NAME_COUNT);
        if (!used[candidate]) {
            name_id = candidate;
            break;
        }
    }
    if (name_id < 0) name_id = rnd(COUNTRY_NAME_COUNT);

    snprintf(name_text, sizeof(name_text), "%s", country_name_localized(name_id, ui_language));
    snprintf(symbol_text, sizeof(symbol_text), "%c", random_unused_symbol());
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        write_int_text(metric_text[i], sizeof(metric_text[i]), random_metric_value());
    }
    selected_civ_color = game_preview_civilization_color_auto_avoid(-1, selected_civ_color);
    selected_civ_color_index = 0;
    synced_civ_id = -2;
}

static int visible_inside(const WorldgenLayout *layout, RECT rect, int x, int y) {
    return worldgen_rect_visible(layout->viewport, rect) &&
           x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

static int field_hit(const WorldgenLayout *layout, int x, int y) {
    int i;
    if (visible_inside(layout, layout->initial_input_frame, x, y)) return SDL_FORM_INITIAL;
    if (visible_inside(layout, layout->name_input_frame, x, y)) return SDL_FORM_NAME;
    if (visible_inside(layout, layout->symbol_input_frame, x, y)) return SDL_FORM_SYMBOL;
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        if (visible_inside(layout, layout->metric_input_frame[i], x, y)) {
            return SDL_FORM_METRIC_BASE + i;
        }
    }
    return SDL_FORM_NONE;
}

int sdl_worldgen_form_click(const WorldgenLayout *layout, int x, int y) {
    int hit = field_hit(layout, x, y);
    if (hit != SDL_FORM_NONE) {
        normalize_active_field();
        activate_field(hit);
        return 1;
    }
    normalize_active_field();
    active_field = SDL_FORM_NONE;
    replace_on_next_text = 0;
    if (visible_inside(layout, layout->add_button, x, y)) {
        (void)add_or_apply(0);
        return 1;
    }
    if (visible_inside(layout, layout->apply_button, x, y)) {
        (void)add_or_apply(1);
        return 1;
    }
    return 0;
}

int sdl_worldgen_form_active(void) {
    return active_field != SDL_FORM_NONE;
}

void sdl_worldgen_form_deactivate(void) {
    normalize_active_field();
    active_field = SDL_FORM_NONE;
    replace_on_next_text = 0;
}

static void remove_last_utf8(char *buffer) {
    size_t len = strlen(buffer);
    if (len == 0) return;
    len--;
    while (len > 0 && ((unsigned char)buffer[len] & 0xc0) == 0x80) len--;
    buffer[len] = '\0';
}

static int copy_symbol_text(char *buffer, const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    if (!buffer || !value) return 0;
    while (*p && *p < 0x20) p++;
    if (!*p || *p >= 0x80) return 0;
    buffer[0] = (char)*p;
    buffer[1] = '\0';
    return 1;
}

int sdl_worldgen_form_text_input(const char *value) {
    char *buffer;
    int max_bytes;
    int numeric_only;
    size_t current;
    size_t add;
    const unsigned char *p;

    if (active_field == SDL_FORM_NONE || !value) return 0;
    buffer = field_buffer(active_field, &max_bytes, &numeric_only);
    if (!buffer || max_bytes <= 1) return 1;
    if (active_field == SDL_FORM_SYMBOL) {
        if (copy_symbol_text(buffer, value)) replace_on_next_text = 0;
        return 1;
    }
    if (numeric_only) {
        for (p = (const unsigned char *)value; *p; p++) {
            if (*p < '0' || *p > '9') return 1;
        }
    }
    current = replace_on_next_text ? 0 : strlen(buffer);
    add = strlen(value);
    if (current + add >= (size_t)max_bytes) return 1;
    if (replace_on_next_text) {
        buffer[0] = '\0';
        replace_on_next_text = 0;
    }
    memcpy(buffer + current, value, add + 1);
    if (active_field == SDL_FORM_INITIAL) sdl_worldgen_form_apply_initial_count();
    return 1;
}

int sdl_worldgen_form_key(SDL_Keycode key) {
    char *buffer;
    int max_bytes;
    int numeric_only;

    if (active_field == SDL_FORM_NONE) return 0;
    buffer = field_buffer(active_field, &max_bytes, &numeric_only);
    (void)max_bytes;
    (void)numeric_only;
    if (key == SDLK_BACKSPACE && buffer) {
        remove_last_utf8(buffer);
        if (active_field == SDL_FORM_INITIAL) sdl_worldgen_form_apply_initial_count();
        replace_on_next_text = 0;
        return 1;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        sdl_worldgen_form_deactivate();
        return 1;
    }
    if (key == SDLK_TAB) {
        normalize_active_field();
        active_field++;
        if (active_field >= SDL_FORM_METRIC_BASE + WORLDGEN_CORE_METRIC_COUNT) active_field = SDL_FORM_INITIAL;
        replace_on_next_text = 1;
        return 1;
    }
    return 1;
}

int sdl_worldgen_form_active_rect(const WorldgenLayout *layout, RECT *out) {
    if (!layout || !out || active_field == SDL_FORM_NONE) return 0;
    if (active_field == SDL_FORM_INITIAL) *out = layout->initial_input_frame;
    else if (active_field == SDL_FORM_NAME) *out = layout->name_input_frame;
    else if (active_field == SDL_FORM_SYMBOL) *out = layout->symbol_input_frame;
    else if (field_is_metric(active_field)) *out = layout->metric_input_frame[active_field - SDL_FORM_METRIC_BASE];
    else return 0;
    return !IsRectEmpty(out);
}

static void draw_input(SDL_Renderer *renderer, SdlText *text, RECT frame,
                       const WorldgenLayout *layout, int field, const char *value) {
    SDL_Color border = active_field == field ? sdl_ui_color(225, 205, 132) : sdl_ui_color(86, 104, 113);
    if (IsRectEmpty(&frame) || !worldgen_rect_visible(layout->viewport, frame)) return;
    sdl_ui_fill(renderer, frame, sdl_ui_color(32, 39, 43));
    sdl_ui_stroke(renderer, frame, border);
    sdl_ui_text_at(renderer, text, text->small, frame.left + 6, frame.top + 7,
                   sdl_ui_color(235, 241, 244), value);
}

void sdl_worldgen_form_draw_overlay(SDL_Renderer *renderer, SdlText *text,
                                    const WorldgenLayout *layout) {
    int i;
    if (!renderer || !text || !layout) return;
    draw_input(renderer, text, layout->initial_input_frame, layout, SDL_FORM_INITIAL, initial_text);
    draw_input(renderer, text, layout->name_input_frame, layout, SDL_FORM_NAME, name_text);
    draw_input(renderer, text, layout->symbol_input_frame, layout, SDL_FORM_SYMBOL, symbol_text);
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        draw_input(renderer, text, layout->metric_input_frame[i], layout, SDL_FORM_METRIC_BASE + i, metric_text[i]);
    }
    if (worldgen_rect_visible(layout->viewport, layout->add_button)) {
        sdl_ui_fill(renderer, layout->add_button, sdl_ui_color(58, 78, 68));
        sdl_ui_text_center(renderer, text, text->small, layout->add_button, sdl_ui_color(245, 250, 248),
                           ui_l10n("Add Civilization", "添加文明"));
    }
    if (worldgen_rect_visible(layout->viewport, layout->apply_button)) {
        sdl_ui_fill(renderer, layout->apply_button, sdl_ui_color(58, 68, 78));
        sdl_ui_text_center(renderer, text, text->small, layout->apply_button, sdl_ui_color(245, 250, 248),
                           ui_l10n("Apply Selected", "应用到所选"));
    }
}

void sdl_worldgen_form_draw(SDL_Renderer *renderer, SdlText *text,
                            const WorldgenLayout *layout) {
    static const char *metric_en[WORLDGEN_CORE_METRIC_COUNT] = {
        "Military (0-10)", "Logistics (0-10)", "Governance (0-10)", "Cohesion (0-10)",
        "Production (0-10)", "Commerce (0-10)", "Innovation (0-10)"
    };
    static const char *metric_zh[WORLDGEN_CORE_METRIC_COUNT] = {
        "军备力 (0-10)", "后勤 (0-10)", "治理 (0-10)", "凝聚 (0-10)",
        "生产 (0-10)", "贸易 (0-10)", "技术 (0-10)"
    };
    int i;

    draw_input(renderer, text, layout->initial_input_frame, layout, SDL_FORM_INITIAL, initial_text);
    sdl_ui_text_at(renderer, text, text->small, layout->initial_help.left, layout->initial_help.top,
                   sdl_ui_color(168, 180, 188),
                   ui_l10n("Used when Generate World auto-places civilizations.",
                           "生成世界并自动放置文明时使用。"));
    if (worldgen_rect_visible(layout->viewport, layout->civ_hint)) {
        sdl_ui_text_at(renderer, text, text->small, layout->civ_hint.left, layout->civ_hint.top,
                       sdl_ui_color(190, 202, 210),
                       ui_l10n("Select land or a natural region, then add or apply a civilization.",
                               "选择陆地或自然区域后，添加或应用文明。"));
    }
    if (worldgen_rect_visible(layout->viewport, layout->name_label)) {
        sdl_ui_text_at(renderer, text, text->small, layout->name_label.left, layout->name_label.top,
                       sdl_ui_color(205, 214, 222), ui_l10n("Name", "名称"));
        sdl_ui_text_at(renderer, text, text->small, layout->symbol_label.left, layout->symbol_label.top,
                       sdl_ui_color(205, 214, 222), ui_l10n("Symbol", "符号"));
    }
    draw_input(renderer, text, layout->name_input_frame, layout, SDL_FORM_NAME, name_text);
    draw_input(renderer, text, layout->symbol_input_frame, layout, SDL_FORM_SYMBOL, symbol_text);
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        if (!worldgen_rect_visible(layout->viewport, layout->metric_input_frame[i])) continue;
        sdl_ui_text_at(renderer, text, text->small, layout->metric_label[i].left, layout->metric_label[i].top,
                       sdl_ui_color(205, 214, 222), ui_l10n(metric_en[i], metric_zh[i]));
        draw_input(renderer, text, layout->metric_input_frame[i], layout, SDL_FORM_METRIC_BASE + i, metric_text[i]);
    }
    if (worldgen_rect_visible(layout->viewport, layout->add_button)) {
        sdl_ui_fill(renderer, layout->add_button, sdl_ui_color(58, 78, 68));
        sdl_ui_text_center(renderer, text, text->small, layout->add_button, sdl_ui_color(245, 250, 248),
                           ui_l10n("Add Civilization", "添加文明"));
        sdl_ui_fill(renderer, layout->apply_button, sdl_ui_color(58, 68, 78));
        sdl_ui_text_center(renderer, text, text->small, layout->apply_button, sdl_ui_color(245, 250, 248),
                           ui_l10n("Apply Selected", "应用到所选"));
    }
}
