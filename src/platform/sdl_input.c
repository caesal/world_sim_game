#include "platform/sdl_input.h"

#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "game/game.h"
#include "game/game_loop.h"
#include "game/game_worldgen.h"
#include "platform/sdl_country_input.h"
#include "platform/sdl_worldgen_form.h"
#include "ui/color_picker.h"
#include "ui/ui_l10n.h"
#include "ui/ui_layout.h"
#include "ui/pause_menu.h"
#include "ui/ui_worldgen_layout.h"
#include "world/world_gen.h"

static RECT client_rect(SDL_Window *window) {
    int w = WINDOW_W;
    int h = WINDOW_H;
    RECT client;
    if (window) SDL_GetWindowSize(window, &w, &h);
    client.left = 0;
    client.top = 0;
    client.right = w;
    client.bottom = h;
    return client;
}

static int inside(RECT rect, int x, int y) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

static void set_display_mode_index(int index, SdlMapRenderer *map) {
    if (index < 0 || index >= MAP_DISPLAY_MODE_COUNT) return;
    display_mode = MAP_DISPLAY_MODES[index];
    sdl_render_invalidate_map(map);
}

static void toggle_run(void) {
    if (!world_generated) return;
    auto_run = !auto_run;
    game_loop_reset();
}

static void generate_world(SdlMapRenderer *map) {
    sdl_worldgen_form_deactivate();
    sdl_worldgen_form_apply_initial_count();
    game_request_new_world_with_callbacks(NULL, NULL);
    sdl_render_invalidate_map(map);
}

static void sync_text_input(SDL_Window *window) {
    RECT client;
    WorldgenLayout layout;
    RECT active;
    SDL_Rect area;
    int cursor;

    if (!window) return;
    if (!sdl_worldgen_form_active()) {
        SDL_SetTextInputArea(window, NULL, 0);
        SDL_StopTextInput(window);
        return;
    }
    client = client_rect(window);
    worldgen_layout_build(client, side_panel_w,
                          worldgen_layout_clamp_scroll(client, side_panel_w, worldgen_scroll_offset), &layout);
    SDL_StartTextInput(window);
    if (!sdl_worldgen_form_active_rect(&layout, &active)) {
        SDL_SetTextInputArea(window, NULL, 0);
        return;
    }
    area.x = active.left;
    area.y = active.top;
    area.w = active.right - active.left;
    area.h = active.bottom - active.top;
    cursor = area.w > 8 ? area.w - 8 : 0;
    SDL_SetTextInputArea(window, &area, cursor);
}

static void update_slider_value(RECT client, int slider_id, int mouse_x) {
    WorldgenLayout layout;
    WorldgenSliderLayout *slider;
    int value;

    worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
    if (slider_id < 0 || slider_id >= UI_SLIDER_COUNT) return;
    slider = &layout.sliders[slider_id];
    value = (mouse_x - slider->track.left) * 100 / max(1, slider->track.right - slider->track.left);
    value = clamp(value, 0, 100);
    switch (slider_id) {
        case WORLD_SLIDER_OCEAN: ocean_slider = value; break;
        case WORLD_SLIDER_CONTINENT: continent_slider = value; break;
        case WORLD_SLIDER_RELIEF: relief_slider = value; break;
        case WORLD_SLIDER_MOISTURE: moisture_slider = value; break;
        case WORLD_SLIDER_DROUGHT: drought_slider = value; break;
        case WORLD_SLIDER_VEGETATION: vegetation_slider = value; break;
        case WORLD_SLIDER_BIAS_FOREST: bias_forest_slider = value; break;
        case WORLD_SLIDER_BIAS_DESERT: bias_desert_slider = value; break;
        case WORLD_SLIDER_BIAS_MOUNTAIN: bias_mountain_slider = value; break;
        case WORLD_SLIDER_BIAS_WETLAND: bias_wetland_slider = value; break;
        case UI_SLIDER_REGION_SIZE: region_size_slider = value; break;
        default: break;
    }
}

static int random_range(int min_value, int max_value) {
    return min_value + rnd(max_value - min_value + 1);
}

static void randomize_physical_world(void) {
    ocean_slider = random_range(25, 75);
    continent_slider = random_range(10, 85);
    relief_slider = random_range(20, 85);
    moisture_slider = random_range(15, 85);
    drought_slider = random_range(15, 85);
    vegetation_slider = random_range(15, 85);
}

static void randomize_terrain_world(void) {
    bias_forest_slider = random_range(20, 80);
    bias_desert_slider = random_range(20, 80);
    bias_mountain_slider = random_range(20, 80);
    bias_wetland_slider = random_range(20, 80);
    region_size_slider = random_range(15, 90);
}

static void randomize_civ_setup(void) {
    sdl_worldgen_form_randomize();
}

static int world_slider_hit(RECT client, int x, int y) {
    WorldgenLayout layout;
    int i;
    worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
    for (i = WORLD_SLIDER_OCEAN; i <= UI_SLIDER_REGION_SIZE; i++) {
        if (worldgen_rect_visible(layout.viewport, layout.sliders[i].hit) &&
            inside(layout.sliders[i].hit, x, y)) return i;
    }
    return -1;
}

static int debug_subtab_hit(RECT client, int x, int y) {
    int panel_x = client.right - side_panel_w + 18;
    int width = side_panel_w - 36;
    int tab_y = TOP_BAR_H + 94;
    int gap = 6;
    int tab_w = (width - gap) / DEBUG_SUBTAB_COUNT;
    int i;
    for (i = 0; i < DEBUG_SUBTAB_COUNT; i++) {
        RECT tab = {panel_x + i * (tab_w + gap), tab_y,
                    i == DEBUG_SUBTAB_COUNT - 1 ? panel_x + width : panel_x + i * (tab_w + gap) + tab_w,
                    tab_y + 28};
        if (inside(tab, x, y)) return i;
    }
    return -1;
}

static int debug_filter_hit(RECT client, int x, int y) {
    int panel_x = client.right - side_panel_w + 18;
    int width = side_panel_w - 36;
    int filter_y = TOP_BAR_H + 174;
    int gap = 4;
    int filter_w = (width - (DEBUG_EVENT_FILTER_COUNT - 1) * gap) / DEBUG_EVENT_FILTER_COUNT;
    int i;
    for (i = 0; i < DEBUG_EVENT_FILTER_COUNT; i++) {
        RECT button = {panel_x + i * (filter_w + gap), filter_y,
                       i == DEBUG_EVENT_FILTER_COUNT - 1 ? panel_x + width :
                       panel_x + i * (filter_w + gap) + filter_w, filter_y + 24};
        if (inside(button, x, y)) return i;
    }
    return -1;
}

static void select_tile(RECT client, int mouse_x, int mouse_y) {
    const RenderSnapshot *snapshot;
    const SnapshotTile *tile;
    MapLayout layout;
    int x;
    int y;
    int owner = -1;

    if (!world_generated) return;
    layout = get_map_layout(client);
    if (mouse_x < layout.map_x || mouse_y < layout.map_y ||
        mouse_x >= layout.map_x + layout.draw_w || mouse_y >= layout.map_y + layout.draw_h) return;
    x = (mouse_x - layout.map_x) * MAP_W / max(1, layout.draw_w);
    y = (mouse_y - layout.map_y) * MAP_H / max(1, layout.draw_h);
    snapshot = render_snapshot_acquire();
    tile = render_snapshot_tile_at(snapshot, x, y);
    if (tile) owner = tile->owner;
    selected_x = x;
    selected_y = y;
    if (snapshot && owner >= 0 && owner < snapshot->civ_count && snapshot->civs[owner].alive) {
        selected_civ = owner;
        country_detail_subtab = COUNTRY_DETAIL_OVERVIEW;
    } else {
        selected_civ = -1;
    }
    render_snapshot_release(snapshot);
}

static void handle_left_down(SDL_Window *window, SdlMapRenderer *map, int x, int y, int *quit) {
    RECT client = client_rect(window);
    RECT legend = get_map_legend_toggle_rect(client);
    int i;
    int slider;

    if (pause_menu_open) {
        int hit = pause_menu_hit_test(client, x, y);
        if (hit == PAUSE_MENU_RESUME_GAME) {
            pause_menu_open = 0;
        } else if (hit == PAUSE_MENU_VERSION_LOG) {
            pause_menu_show_version_log(NULL);
        } else if (hit == PAUSE_MENU_EXIT_GAME && quit) {
            *quit = 1;
        }
        return;
    }
    if (color_picker_active()) {
        color_picker_mouse_down(NULL, client, x, y);
        return;
    }
    if (!IsRectEmpty(&legend) && inside(legend, x, y)) {
        map_legend_collapsed = !map_legend_collapsed;
        return;
    }
    if (inside(get_language_button_rect(client), x, y)) {
        ui_language = ui_language == UI_LANG_EN ? UI_LANG_ZH : UI_LANG_EN;
        return;
    }
    if (inside(get_reset_view_button_rect(client), x, y)) {
        ui_map_view_reset();
        ui_map_view_clamp(client);
        return;
    }
    if (inside(get_play_button_rect(client), x, y)) {
        toggle_run();
        return;
    }
    for (i = 0; i < SPEED_COUNT; i++) {
        if (inside(get_speed_button_rect(client, i), x, y)) {
            speed_index = i;
            return;
        }
    }
    if (side_panel_handle_hit_test(client, x, y)) {
        ui_toggle_side_panel(client);
        return;
    }
    if (!side_panel_collapsed) {
        for (i = 0; i < PANEL_TAB_COUNT; i++) {
            if (inside(get_panel_tab_rect(client, i), x, y)) {
                panel_tab = i;
                sdl_worldgen_form_deactivate();
                sync_text_input(window);
                return;
            }
        }
    }
    if (!side_panel_collapsed && panel_tab == PANEL_WORLD) {
        WorldgenLayout layout;
        worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
        if (sdl_worldgen_form_click(&layout, x, y)) {
            sync_text_input(window);
            return;
        }
        sync_text_input(window);
        for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
            if (inside(get_mode_button_rect(client, i), x, y)) {
                set_display_mode_index(i, map);
                return;
            }
        }
        for (i = 0; i < MAP_SIZE_COUNT; i++) {
            if (inside(layout.map_size_buttons[i], x, y)) {
                pending_map_size = i;
                return;
            }
        }
        if (worldgen_rect_visible(layout.viewport, layout.physical_random_button) &&
            inside(layout.physical_random_button, x, y)) {
            randomize_physical_world();
            return;
        }
        if (worldgen_rect_visible(layout.viewport, layout.terrain_random_button) &&
            inside(layout.terrain_random_button, x, y)) {
            randomize_terrain_world();
            return;
        }
        if (worldgen_rect_visible(layout.viewport, layout.civ_random_button) &&
            inside(layout.civ_random_button, x, y)) {
            randomize_civ_setup();
            return;
        }
        if (worldgen_rect_visible(layout.viewport, layout.generate_row) &&
            inside(layout.generate_row, x, y)) {
            generate_world(map);
            sync_text_input(window);
            return;
        }
        if (inside(layout.civ_color_preview, x, y)) {
            color_picker_open_setup(selected_civ_color);
            return;
        }
        slider = world_slider_hit(client, x, y);
        if (slider >= 0) {
            dragging_slider = slider;
            update_slider_value(client, slider, x);
            return;
        }
    }
    if (!side_panel_collapsed && panel_tab == PANEL_DEBUG && x >= client.right - side_panel_w) {
        int hit = debug_subtab_hit(client, x, y);
        if (hit >= 0) {
            debug_subtab = hit;
            return;
        }
        if (debug_subtab == DEBUG_SUBTAB_MAP_LOG) {
            int filter = debug_filter_hit(client, x, y);
            for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
                if (inside(get_mode_button_rect(client, i), x, y)) {
                    set_display_mode_index(i, map);
                    return;
                }
            }
            if (filter >= 0) {
                debug_event_filter = filter;
                debug_event_log_scroll_offset = 0;
                return;
            }
        }
    }
    if (sdl_country_panel_click(client, x, y)) {
        sdl_worldgen_form_deactivate();
        sync_text_input(window);
        return;
    }
    if (x >= client.right - side_panel_w - 4 && x <= client.right - side_panel_w + 4) {
        dragging_panel = 1;
        return;
    }
    select_tile(client, x, y);
    sdl_worldgen_form_deactivate();
    sync_text_input(window);
}

static void handle_right_down(SDL_Window *window, int x, int y) {
    RECT client = client_rect(window);
    if (pause_menu_open || x >= client.right - side_panel_w ||
        y < TOP_BAR_H || y > client.bottom - BOTTOM_BAR_H) return;
    dragging_map = 1;
    map_interaction_preview = 1;
    last_mouse_x = x;
    last_mouse_y = y;
}

static void handle_motion(SDL_Window *window, SdlMapRenderer *map, int x, int y) {
    RECT client = client_rect(window);
    hover_x = x;
    hover_y = y;
    if (color_picker_active()) {
        color_picker_mouse_move(NULL, client, x, y);
        return;
    }
    if (dragging_map) {
        map_offset_x += x - last_mouse_x;
        map_offset_y += y - last_mouse_y;
        map_view_auto_centered = 0;
        last_mouse_x = x;
        last_mouse_y = y;
        ui_map_view_clamp(client);
        sdl_render_invalidate_map(map);
    } else if (dragging_panel) {
        side_panel_w = clamp(client.right - x, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        side_panel_expanded_w = side_panel_w;
        ui_map_view_clamp(client);
    } else if (dragging_slider >= 0) {
        update_slider_value(client, dragging_slider, x);
    }
}

static void handle_wheel(SDL_Window *window, SdlMapRenderer *map, float wheel_y, int mouse_x, int mouse_y) {
    RECT client = client_rect(window);
    MapLayout before;
    int old_zoom = map_zoom_percent;
    int steps = wheel_y > 0.0f ? 1 : -1;
    int tile_x_scaled;
    int tile_y_scaled;

    if (mouse_x >= client.right - side_panel_w) {
        if (panel_tab == PANEL_WORLD) {
            worldgen_scroll_offset = worldgen_layout_clamp_scroll(
                client, side_panel_w, worldgen_scroll_offset - steps * 72);
        }
        return;
    }
    if (mouse_y < TOP_BAR_H || mouse_y > client.bottom - BOTTOM_BAR_H) return;
    before = get_map_layout(client);
    tile_x_scaled = (mouse_x - before.map_x) * 100000 / max(1, before.draw_w);
    tile_y_scaled = (mouse_y - before.map_y) * 100000 / max(1, before.draw_h);
    map_zoom_percent = clamp(map_zoom_percent + steps * 5, 25, 700);
    if (map_zoom_percent == old_zoom) return;
    {
        MapLayout after = get_map_layout(client);
        map_offset_x += mouse_x - (after.map_x + tile_x_scaled * after.draw_w / 100000);
        map_offset_y += mouse_y - (after.map_y + tile_y_scaled * after.draw_h / 100000);
        map_view_auto_centered = 0;
        ui_map_view_clamp(client);
    }
    map_interaction_preview = 1;
    sdl_render_invalidate_map(map);
}

static void handle_key(SDL_Keycode key, int *quit, SdlMapRenderer *map) {
    if (sdl_worldgen_form_active()) {
        if (key == SDLK_ESCAPE) {
            sdl_worldgen_form_deactivate();
            return;
        }
        if (!pause_menu_open && sdl_worldgen_form_key(key)) return;
    }
    if (key == SDLK_Q) {
        *quit = 1;
    } else if (key == SDLK_ESCAPE) {
        if (color_picker_active()) {
            color_picker_close();
            return;
        }
        sdl_worldgen_form_deactivate();
        pause_menu_open = !pause_menu_open;
        auto_run = 0;
    } else if (!pause_menu_open && key == SDLK_SPACE) {
        toggle_run();
    } else if (!pause_menu_open && (key == SDLK_F5 || key == SDLK_R)) {
        generate_world(map);
    } else if (!pause_menu_open && key == SDLK_L) {
        ui_language = ui_language == UI_LANG_ZH ? UI_LANG_EN : UI_LANG_ZH;
    } else if (!pause_menu_open && key >= SDLK_1 && key <= SDLK_5) {
        set_display_mode_index((int)(key - SDLK_1), map);
    }
}

void sdl_input_handle_event(const SDL_Event *event, SDL_Window *window,
                            SdlMapRenderer *map, int *quit) {
    if (!event || !quit) return;
    if (event->type == SDL_EVENT_QUIT) {
        *quit = 1;
    } else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        RECT client = client_rect(window);
        ui_side_panel_apply_state(client);
        worldgen_scroll_offset = worldgen_layout_clamp_scroll(client, side_panel_w, worldgen_scroll_offset);
        sync_text_input(window);
    } else if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        handle_key(event->key.key, quit, map);
        sync_text_input(window);
    } else if (event->type == SDL_EVENT_TEXT_INPUT) {
        if (sdl_worldgen_form_text_input(event->text.text)) sync_text_input(window);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        handle_left_down(window, map, (int)event->button.x, (int)event->button.y, quit);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT) {
        handle_right_down(window, (int)event->button.x, (int)event->button.y);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (color_picker_active()) color_picker_mouse_up(NULL);
        dragging_map = 0;
        dragging_panel = 0;
        dragging_slider = -1;
        map_interaction_preview = 0;
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        handle_motion(window, map, (int)event->motion.x, (int)event->motion.y);
        sync_text_input(window);
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        float mx;
        float my;
        SDL_GetMouseState(&mx, &my);
        handle_wheel(window, map, event->wheel.y, (int)mx, (int)my);
        sync_text_input(window);
    }
}
