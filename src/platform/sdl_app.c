#include "platform/sdl_app.h"

#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "game/game_loop.h"
#include "game/game_worldgen.h"
#include "platform/sdl_input.h"
#include "platform/sdl_render.h"
#include "platform/sdl_world.h"
#include "platform/sdl_worldgen_form.h"
#include "sim/simulation_worker.h"
#include "ui/ui_layout.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

static void set_bundle_resource_cwd(void) {
    const char *base = SDL_GetBasePath();
    char resources[1024];

    if (!base) return;
    if (strstr(base, ".app/Contents/MacOS/")) {
        snprintf(resources, sizeof(resources), "%s../Resources", base);
        chdir(resources);
    }
}

int run_sdl_game(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SdlText text;
    SdlMapRenderer map;
    char error[256];
    int quit = 0;
    int running = 0;
    int months_per_tick = speed_index + 1;

    memset(&text, 0, sizeof(text));
    sdl_render_init(&map);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    set_bundle_resource_cwd();
    if (!sdl_text_init(&text, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        SDL_Quit();
        return 1;
    }
    window = SDL_CreateWindow("World Sim Game", WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        sdl_text_shutdown(&text);
        SDL_Quit();
        return 1;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        sdl_text_shutdown(&text);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    game_start_blank_world();
    sdl_worldgen_form_init();
    {
        RECT client = {0, 0, WINDOW_W, WINDOW_H};
        ui_side_panel_apply_state(client);
    }
    sdl_render_invalidate_map(&map);
    while (!quit) {
        SDL_Event event;
        int window_w;
        int window_h;
        int redraw;

        while (SDL_PollEvent(&event)) {
            sdl_input_handle_event(&event, window, &map, &quit);
        }

        running = auto_run;
        months_per_tick = speed_index + 1;
        redraw = game_loop_tick_frame();
        if (redraw != GAME_REDRAW_NONE) sdl_render_invalidate_map(&map);
        SDL_GetWindowSize(window, &window_w, &window_h);
        if (!sdl_render_draw(&map, renderer, &text, window_w, window_h,
                             display_mode, running, months_per_tick)) {
            fprintf(stderr, "SDL render failed: %s\n", SDL_GetError());
            break;
        }
        SDL_Delay(16);
    }

    sdl_render_shutdown(&map);
    simulation_worker_shutdown();
    render_snapshot_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    sdl_text_shutdown(&text);
    SDL_Quit();
    return 0;
}

int run_sdl_render_smoke(const char *path) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surface = NULL;
    SdlText text;
    SdlMapRenderer map;
    char error[256];
    int ok = 0;
    const char *out_path = path && path[0] ? path : "world_sim_render_smoke.bmp";

    memset(&text, 0, sizeof(text));
    sdl_render_init(&map);
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    if (!sdl_text_init(&text, error, sizeof(error))) goto done;
    window = SDL_CreateWindow("World Sim Render Smoke", 1280, 820, SDL_WINDOW_HIDDEN);
    if (!window) goto done;
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) goto done;
    sdl_world_generate_default(25, MAP_SIZE_SMALL);
    {
        RECT client = {0, 0, 1280, 820};
        ui_side_panel_apply_state(client);
    }
    if (!sdl_render_draw_backbuffer(&map, renderer, &text, 1280, 820,
                                    display_mode, auto_run, speed_index + 1)) {
        goto done;
    }
    surface = SDL_RenderReadPixels(renderer, NULL);
    if (!surface) goto done;
    ok = SDL_SaveBMP(surface, out_path) ? 1 : 0;
done:
    if (surface) SDL_DestroySurface(surface);
    sdl_render_shutdown(&map);
    simulation_worker_shutdown();
    render_snapshot_shutdown();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    sdl_text_shutdown(&text);
    SDL_Quit();
    return ok ? 0 : 1;
}
