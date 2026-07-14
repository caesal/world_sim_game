#include "ui/ui_plague_probability.h"

#include "core/constants.h"
#include "core/render_snapshot.h"
#include "game/game_plague_probability_request.h"
#include "sim/plague_probability.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_pressed_state.h"

#include <string.h>

enum {
    PROBABILITY_TITLE_HEIGHT = 20,
    PROBABILITY_CELL_HEIGHT = 38,
    PROBABILITY_ROW_GAP = 4,
    PROBABILITY_COLUMN_GAP = 8,
    PROBABILITY_LABEL_HEIGHT = 17,
    PROBABILITY_TRACK_TOP = 23,
    PROBABILITY_TRACK_HEIGHT = 8,
    PROBABILITY_ACTION_GAP = 6,
    PROBABILITY_ACTION_HEIGHT = 26,
    PROBABILITY_STATUS_GAP = 4,
    PROBABILITY_STATUS_HEIGHT = 32,
    PROBABILITY_BOTTOM_GAP = 8
};

typedef struct {
    PlagueProbabilityDistribution draft;
    PlagueProbabilityDistribution source;
    int initialized;
    int dirty;
    int needs_reopen_sync;
    int dragging_bucket;
    int pressed_hit;
    unsigned int revision;
} UiPlagueProbabilityState;

static UiPlagueProbabilityState state = {
    {0, 0, 0, 0}, {0, 0, 0, 0}, 0, 0, 1, -1,
    UI_PLAGUE_PANEL_HIT_NONE, 1u
};

static int point_in(RECT rect, int x, int y) {
    return x >= rect.left && x < rect.right &&
           y >= rect.top && y < rect.bottom;
}

static void note_change(void) {
    state.revision++;
    if (state.revision == 0u) state.revision = 1u;
}

static PlagueProbabilityDistribution desired_from_view(
    const PlagueStateView *view) {
    PlagueProbabilityDistribution desired;
    plague_probability_defaults(&desired);
    if (!view || !plague_probability_validate(
            &view->effective_probabilities)) return desired;
    desired = view->effective_probabilities;
    if (view->pending_probabilities_valid &&
        plague_probability_validate(&view->pending_probabilities)) {
        desired = view->pending_probabilities;
    }
    return desired;
}

void ui_plague_probability_layout_build(RECT client, int panel_width, int top,
                                        UiPlagueProbabilityLayout *layout) {
    int left;
    int right;
    int width;
    int column_width;
    int row_top;
    int action_top;
    int reset_width;
    int apply_width;
    int total_right;
    int i;

    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    panel_width = max(1, min(panel_width, client.right - client.left));
    left = client.right - panel_width + FORM_X_PAD;
    right = max(left + 1, client.right - FORM_X_PAD);
    width = right - left;
    column_width = max(1, (width - PROBABILITY_COLUMN_GAP) / 2);
    layout->title = (RECT){left, top, right, top + PROBABILITY_TITLE_HEIGHT};
    row_top = layout->title.bottom;
    for (i = 0; i < PLAGUE_PROBABILITY_COUNT; i++) {
        int column = i % 2;
        int row = i / 2;
        int x = left + column * (column_width + PROBABILITY_COLUMN_GAP);
        int cell_right = column == 1 ? right : x + column_width;
        int y = row_top + row * (PROBABILITY_CELL_HEIGHT +
                                 PROBABILITY_ROW_GAP);
        UiPlagueProbabilityControlLayout *control = &layout->controls[i];
        control->label = (RECT){x, y, cell_right - 40,
                                y + PROBABILITY_LABEL_HEIGHT};
        control->value = (RECT){max(x, cell_right - 40), y, cell_right,
                                y + PROBABILITY_LABEL_HEIGHT};
        control->track = (RECT){x + 7, y + PROBABILITY_TRACK_TOP,
                                cell_right - 7,
                                y + PROBABILITY_TRACK_TOP +
                                    PROBABILITY_TRACK_HEIGHT};
        control->hit = (RECT){x, y + PROBABILITY_LABEL_HEIGHT,
                              cell_right,
                              y + PROBABILITY_CELL_HEIGHT};
    }
    action_top = row_top + 2 * PROBABILITY_CELL_HEIGHT +
                 PROBABILITY_ROW_GAP + PROBABILITY_ACTION_GAP;
    reset_width = min(128, max(94, width * 2 / 5));
    apply_width = min(72, max(60, width / 5));
    total_right = max(left + 1, right - reset_width - apply_width -
                                   2 * PROBABILITY_COLUMN_GAP);
    layout->total = (RECT){left, action_top, total_right,
                           action_top + PROBABILITY_ACTION_HEIGHT};
    layout->apply_button = (RECT){total_right + PROBABILITY_COLUMN_GAP,
        action_top,
        total_right + PROBABILITY_COLUMN_GAP + apply_width,
        action_top + PROBABILITY_ACTION_HEIGHT};
    layout->reset_button = (RECT){layout->apply_button.right +
        PROBABILITY_COLUMN_GAP, action_top, right,
        action_top + PROBABILITY_ACTION_HEIGHT};
    layout->status = (RECT){left,
        layout->total.bottom + PROBABILITY_STATUS_GAP, right,
        layout->total.bottom + PROBABILITY_STATUS_GAP +
            PROBABILITY_STATUS_HEIGHT};
    layout->bottom = layout->status.bottom + PROBABILITY_BOTTOM_GAP;
    layout->bounds = (RECT){left, top, right, layout->bottom};
}

void ui_plague_probability_sync(const PlagueStateView *view) {
    PlagueProbabilityDistribution desired = desired_from_view(view);
    int source_changed = !state.initialized ||
        !plague_probability_equal(&state.source, &desired);

    if (!state.initialized || state.needs_reopen_sync) {
        state.source = desired;
        state.draft = desired;
        state.initialized = 1;
        state.dirty = 0;
        state.needs_reopen_sync = 0;
        note_change();
        return;
    }
    if (source_changed) {
        state.source = desired;
        if (!state.dirty) state.draft = desired;
        state.dirty = !plague_probability_equal(&state.draft,
                                                 &state.source);
        note_change();
    }
}

static void sync_current_snapshot(void) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    ui_plague_probability_sync(snapshot ? &snapshot->plague_state : NULL);
    render_snapshot_release(snapshot);
}

void ui_plague_probability_get_draft(PlagueProbabilityDistribution *out) {
    if (!out) return;
    if (!state.initialized) {
        plague_probability_defaults(out);
        return;
    }
    *out = state.draft;
}

int ui_plague_probability_adjust_draft(PlagueProbabilityBucket bucket,
                                       int requested_value) {
    PlagueProbabilityDistribution adjusted;
    if (!state.initialized) {
        plague_probability_defaults(&state.source);
        state.draft = state.source;
        state.initialized = 1;
        state.needs_reopen_sync = 0;
    }
    if (!plague_probability_adjust_linked(&state.draft, bucket,
            requested_value, &adjusted) ||
        plague_probability_equal(&state.draft, &adjusted)) return 0;
    state.draft = adjusted;
    state.dirty = !plague_probability_equal(&state.draft, &state.source);
    note_change();
    return 1;
}

int ui_plague_probability_reset_draft(void) {
    PlagueProbabilityDistribution defaults;
    plague_probability_defaults(&defaults);
    if (!state.initialized) {
        state.source = defaults;
        state.draft = defaults;
        state.initialized = 1;
        state.needs_reopen_sync = 0;
        return 0;
    }
    if (plague_probability_equal(&state.draft, &defaults)) return 0;
    state.draft = defaults;
    state.dirty = !plague_probability_equal(&state.draft, &state.source);
    note_change();
    return 1;
}

int ui_plague_probability_apply_enabled(void) {
    return state.initialized && state.dirty;
}

int ui_plague_probability_reset_enabled(void) {
    PlagueProbabilityDistribution defaults;
    plague_probability_defaults(&defaults);
    return state.initialized &&
        !plague_probability_equal(&state.draft, &defaults);
}

int ui_plague_probability_pressed(int hit) {
    return state.pressed_hit == hit ||
        ui_pressed_control_is_active(UI_PRESSED_PLAGUE_PROBABILITY, hit);
}

unsigned int ui_plague_probability_cache_revision(void) {
    return state.revision;
}

int ui_plague_probability_hit_test(const UiPlagueProbabilityLayout *layout,
                                   int x, int y) {
    int i;
    if (!layout) return UI_PLAGUE_PANEL_HIT_NONE;
    for (i = 0; i < PLAGUE_PROBABILITY_COUNT; i++) {
        if (point_in(layout->controls[i].hit, x, y)) {
            return UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE + i;
        }
    }
    if (point_in(layout->apply_button, x, y)) {
        return UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY;
    }
    if (point_in(layout->reset_button, x, y)) {
        return UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET;
    }
    return UI_PLAGUE_PANEL_HIT_NONE;
}

static int value_for_x(RECT track, int x) {
    int travel = max(1, track.right - track.left - 1);
    x = max(track.left, min(x, track.right - 1));
    return ((x - track.left) * 100 + travel / 2) / travel;
}

static int update_drag(const UiPlagueProbabilityLayout *layout, int x) {
    int bucket = state.dragging_bucket;
    int requested;
    if (!layout || bucket < 0 || bucket >= PLAGUE_PROBABILITY_COUNT) return 0;
    requested = value_for_x(layout->controls[bucket].track, x);
    return ui_plague_probability_adjust_draft(
        (PlagueProbabilityBucket)bucket, requested);
}

int ui_plague_probability_mouse_down(HWND hwnd,
    const UiPlagueProbabilityLayout *layout, int x, int y) {
    int hit;
    hit = ui_plague_probability_hit_test(layout, x, y);
    if (hit == UI_PLAGUE_PANEL_HIT_NONE) return 0;
    sync_current_snapshot();
    if (hit >= UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE &&
        hit < UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE +
              PLAGUE_PROBABILITY_COUNT) {
        state.dragging_bucket = hit - UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE;
        state.pressed_hit = hit;
        ui_pressed_control_set(hwnd, UI_PRESSED_PLAGUE_PROBABILITY, hit);
        if (hwnd) SetCapture(hwnd);
        update_drag(layout, x);
        note_change();
        if (hwnd) ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (hit == UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY) {
        if (!ui_plague_probability_apply_enabled()) return 1;
    } else if (hit == UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET) {
        if (!ui_plague_probability_reset_enabled()) return 1;
    } else return 0;
    state.pressed_hit = hit;
    ui_pressed_control_set(hwnd, UI_PRESSED_PLAGUE_PROBABILITY, hit);
    if (hwnd) SetCapture(hwnd);
    note_change();
    if (hwnd) ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_plague_probability_mouse_move(HWND hwnd,
    const UiPlagueProbabilityLayout *layout, int x, int y) {
    (void)y;
    if (state.dragging_bucket < 0) return 0;
    if (update_drag(layout, x) && hwnd) ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_plague_probability_mouse_up(HWND hwnd,
    const UiPlagueProbabilityLayout *layout, int x, int y) {
    int old_hit = state.pressed_hit;
    int release_hit;
    int was_dragging = state.dragging_bucket >= 0;
    int action = 0;
    if (old_hit == UI_PLAGUE_PANEL_HIT_NONE && !was_dragging) return 0;
    release_hit = ui_plague_probability_hit_test(layout, x, y);
    state.dragging_bucket = -1;
    state.pressed_hit = UI_PLAGUE_PANEL_HIT_NONE;
    ui_pressed_control_clear(hwnd);
    if (hwnd) ReleaseCapture();
    if (!was_dragging && release_hit == old_hit) {
        if (old_hit == UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY &&
            ui_plague_probability_apply_enabled()) {
            action = game_request_apply_plague_probabilities(&state.draft);
            if (action) {
                state.initialized = 0;
                state.dirty = 0;
                sync_current_snapshot();
            } else {
                MessageBeep(MB_ICONWARNING);
            }
        } else if (old_hit == UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET &&
                   ui_plague_probability_reset_enabled()) {
            ui_plague_probability_reset_draft();
            action = 1;
        }
    }
    note_change();
    if (hwnd) ui_invalidate_side_panel(hwnd);
    return 1;
}

void ui_plague_probability_panel_closed(void) {
    state.needs_reopen_sync = 1;
    state.dragging_bucket = -1;
    state.pressed_hit = UI_PLAGUE_PANEL_HIT_NONE;
    ui_pressed_control_clear(NULL);
    note_change();
}

void ui_plague_probability_reset_presentation_state(void) {
    memset(&state, 0, sizeof(state));
    state.needs_reopen_sync = 1;
    state.dragging_bucket = -1;
    state.pressed_hit = UI_PLAGUE_PANEL_HIT_NONE;
    ui_pressed_control_clear(NULL);
    state.revision = 1u;
}
