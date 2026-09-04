#include "game/game_presentation_world_announcement_probe.h"

#include "core/game_notifications.h"
#include "core/render_snapshot.h"
#include "core/world_announcement_store.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "render/render_transient_ui.h"
#include "render/top_world_announcement.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"
#include "ui/world_announcement_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game_presentation_static_physical_artifacts.h"

static int write_bmp(const char *path, const BITMAPINFO *info, const void *bits, int w, int h) {
    BITMAPFILEHEADER header = {0};
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    return 1;
}

static void stream_set(WorldAnnouncementStreamEntry *entry,
                       const WorldAnnouncementEvent *event) {
    entry->event_id = event->event_id;
    entry->event_type = event->event_type;
    entry->priority = event->priority;
    entry->year = event->year;
    entry->month = event->month;
}

static void fill_patterned_map(HDC hdc, RECT client) {
    RECT viewport = get_map_viewport_rect(client);
    int x;
    fill_rect(hdc, client, RGB(18, 24, 28));
    fill_rect(hdc, viewport, RGB(72, 108, 132));
    for (x = viewport.left; x < viewport.right; x += 48) {
        COLORREF color = ((x - viewport.left) / 48) & 1 ?
                         RGB(126, 158, 112) : RGB(92, 126, 154);
        fill_rect(hdc, (RECT){x, viewport.top, min(x + 24, viewport.right),
                              viewport.bottom}, color);
    }
}

static int render_event(const WorldAnnouncementEvent *event, const char *path,
                        int language, int width, int collapsed, int requested_page,
                        int action_toast, TopWorldAnnouncementProbeInfo *out_info) {
    const int height = 220;
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    int old_language = ui_language, old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int i, ok;
    if (!snapshot || !hdc) { free(snapshot); if (hdc) DeleteDC(hdc); ReleaseDC(NULL, screen); return 0; }
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) { free(snapshot); DeleteDC(hdc); ReleaseDC(NULL, screen); return 0; }
    old = SelectObject(hdc, bitmap);
    ui_language = language;
    side_panel_collapsed = collapsed;
    side_panel_w = 380;
    world_announcement_store_clear();
    world_announcement_store_append(event);
    snapshot->world_announcement_count = 1;
    snapshot->world_announcement_total_entries = world_announcement_store_total_entries();
    stream_set(&snapshot->world_announcements[0], event);
    world_announcement_queue_reset();
    render_context_begin(snapshot);
    fill_patterned_map(hdc, client);
    draw_top_bar(hdc, client);
    render_transient_ui_draw_full(hdc, client, 0);
    for (i = 0; i < requested_page; i++) world_announcement_queue_next_page();
    fill_patterned_map(hdc, client);
    draw_top_bar(hdc, client);
    if (action_toast) {
        game_notifications_push("Action command completed.", "操作命令已完成。");
    }
    render_transient_ui_draw_full(hdc, client, 0);
    if (out_info) *out_info = top_world_announcement_probe_info();
    ok = write_bmp(path, &info, bits, width, height);
    render_context_end();
    world_announcement_queue_reset();
    ui_language = old_language;
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    SelectObject(hdc, old);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    free(snapshot);
    return ok;
}

static void snapshot_add(RenderSnapshot *snapshot, int index,
                         const WorldAnnouncementEvent *source, int id, int priority) {
    WorldAnnouncementEvent event = *source;
    event.event_id = id;
    event.priority = priority;
    world_announcement_store_append(&event);
    stream_set(&snapshot->world_announcements[index], &event);
}

static int queue_case(FILE *summary, const WorldAnnouncementProbeBundle *bundle) {
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 1366, TOP_BAR_H);
    HBITMAP old = SelectObject(hdc, bitmap);
    RECT client = {0, 0, 1366, 768};
    int pages, page_resume, preempt, dedupe, priority, order, pause;
    int before_progress;
    int durations;
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    if (!snapshot) return 0;
    side_panel_collapsed = 0;
    side_panel_w = 380;
    world_announcement_store_clear();
    world_announcement_queue_reset();
    snapshot->world_announcement_count = 1;
    snapshot_add(snapshot, 0, &bundle->union_long, 101, WORLD_ANNOUNCEMENT_NORMAL);
    snapshot->world_announcement_total_entries = world_announcement_store_total_entries();
    render_context_begin(snapshot);
    draw_top_world_announcement(hdc, client);
    pages = world_announcement_queue_page_count();
    world_announcement_queue_next_page();
    snapshot->world_announcement_count = 2;
    snapshot_add(snapshot, 1, &bundle->age, 102, WORLD_ANNOUNCEMENT_CRITICAL);
    snapshot->world_announcement_total_entries = world_announcement_store_total_entries();
    world_announcement_queue_consume(snapshot);
    preempt = world_announcement_queue_current()->event_id == 102;
    world_announcement_queue_dismiss();
    page_resume = world_announcement_queue_current()->event_id == 101 &&
                  world_announcement_queue_current_page() == 1;
    render_context_end();
    world_announcement_queue_reset();
    world_announcement_store_clear();
    snapshot->world_announcement_count = 4;
    snapshot_add(snapshot, 0, &bundle->alliance_war_started, 201, WORLD_ANNOUNCEMENT_NORMAL);
    snapshot_add(snapshot, 1, &bundle->alliance_war_truce, 202, WORLD_ANNOUNCEMENT_NORMAL);
    snapshot_add(snapshot, 2, &bundle->plague_started, 203, WORLD_ANNOUNCEMENT_MAJOR);
    snapshot_add(snapshot, 3, &bundle->military_upgrade, 204, WORLD_ANNOUNCEMENT_CRITICAL);
    snapshot->world_announcement_total_entries = world_announcement_store_total_entries();
    world_announcement_queue_consume(snapshot);
    dedupe = world_announcement_queue_pending_count() == 4;
    world_announcement_queue_consume(snapshot);
    dedupe &= world_announcement_queue_pending_count() == 4;
    priority = world_announcement_queue_current()->event_id == 204;
    world_announcement_queue_dismiss();
    priority &= world_announcement_queue_current()->event_id == 203;
    world_announcement_queue_dismiss();
    order = world_announcement_queue_current()->event_id == 201;
    world_announcement_queue_dismiss();
    order &= world_announcement_queue_current()->event_id == 202;
    before_progress = world_announcement_queue_progress_permille();
    world_announcement_queue_set_hovered(1);
    world_announcement_queue_tick(GetTickCount() + 200);
    pause = world_announcement_queue_progress_permille() == before_progress;
    durations = world_announcement_queue_duration_ms(3) == 7000 &&
                world_announcement_queue_duration_ms(2) == 5000 &&
                world_announcement_queue_duration_ms(1) == 3500;
    fprintf(summary,
        "case=world_announcement_queue ok=%d priority=%d order=%d preempt_resume=%d dedupe=%d page_resume=%d pages=%d hover_pause=%d durations=%d\n",
        priority && order && preempt && dedupe && page_resume && pause && pages == 2 && durations,
        priority, order, preempt && page_resume, dedupe, page_resume, pages, pause, durations);
    world_announcement_queue_reset();
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    SelectObject(hdc, old); DeleteObject(bitmap); DeleteDC(hdc); ReleaseDC(NULL, screen); free(snapshot);
    return priority && order && preempt && dedupe && page_resume && pause && pages == 2 && durations;
}

static int artifact_cases(FILE *summary, const WorldAnnouncementProbeBundle *bundle) {
    TopWorldAnnouncementProbeInfo collapse, union1, union2, rich;
    int ok = 1;
    ok &= render_event(&bundle->age, static_physical_probe_artifact_path("announcement_age_first_zh.bmp"), UI_LANG_ZH, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->age, static_physical_probe_artifact_path("announcement_age_first_en.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->collapse, static_physical_probe_artifact_path("announcement_collapse_five_successors.bmp"), UI_LANG_EN, 1920, 0, 0, 0, &collapse);
    ok &= render_event(&bundle->union_four, static_physical_probe_artifact_path("announcement_union_four_members.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->union_long, static_physical_probe_artifact_path("announcement_union_long_page_1.bmp"), UI_LANG_EN, 1366, 0, 0, 0, &union1);
    ok &= render_event(&bundle->union_long, static_physical_probe_artifact_path("announcement_union_long_page_2.bmp"), UI_LANG_EN, 1366, 0, 1, 0, &union2);
    ok &= render_event(&bundle->vassal_independence, static_physical_probe_artifact_path("announcement_vassal_independence.bmp"), UI_LANG_ZH, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->military_upgrade, static_physical_probe_artifact_path("announcement_military_upgrade.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->military_downgrade, static_physical_probe_artifact_path("announcement_military_downgrade.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->plague_started, static_physical_probe_artifact_path("announcement_plague_started.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->plague_ended, static_physical_probe_artifact_path("announcement_plague_ended.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->plague_started, static_physical_probe_artifact_path("announcement_plague_started_zh.bmp"), UI_LANG_ZH, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->plague_ended, static_physical_probe_artifact_path("announcement_plague_ended_zh.bmp"), UI_LANG_ZH, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->alliance_war_started, static_physical_probe_artifact_path("announcement_alliance_war_start.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->alliance_war_victory, static_physical_probe_artifact_path("announcement_alliance_war_victory.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->alliance_war_truce, static_physical_probe_artifact_path("announcement_alliance_war_negotiated_truce.bmp"), UI_LANG_EN, 1920, 0, 0, 0, NULL);
    ok &= render_event(&bundle->union_four, static_physical_probe_artifact_path("announcement_side_panel_expanded.bmp"), UI_LANG_EN, 1920, 0, 0, 0, &rich);
    ok &= render_event(&bundle->union_four, static_physical_probe_artifact_path("announcement_side_panel_collapsed.bmp"), UI_LANG_EN, 1366, 1, 0, 0, NULL);
    ok &= render_event(&bundle->age, static_physical_probe_artifact_path("announcement_action_toast_coexistence.bmp"), UI_LANG_EN, 1920, 0, 0, 1, NULL);
    fprintf(summary,
        "case=world_announcement_collapse ok=%d successors=%d all_visible=%d identity_colors=%d stable_snapshots=%d\n",
        ok && collapse.related_visible == 5 && bundle->collapse_stable,
        bundle->collapse.related_count, collapse.related_visible == 5,
        collapse.country_spans >= 6, bundle->collapse_stable);
    fprintf(summary,
        "case=world_announcement_union ok=%d absorbed=%d all_visible=%d pages=%d alliance_color=%d country_colors=%d duplicate=%d\n",
        ok && union1.page_count == 2 && union2.page == 1 &&
        union1.related_visible + union2.related_visible == 12 && bundle->union_stable &&
        bundle->union_duplicate_count == 0,
        bundle->union_long.related_count,
        union1.related_visible + union2.related_visible == 12, union1.page_count,
        union1.first_alliance_color == bundle->union_long.alliance_a.color,
        union1.country_spans > 0 && union2.country_spans > 0, bundle->union_duplicate_count);
    fprintf(summary,
        "case=world_announcement_rich_text ok=%d country_spans=%d alliance_spans=%d neutral_spans=%d dark_color_readable=%d\n",
        rich.country_spans > 0 && rich.alliance_spans > 0 && rich.neutral_spans > 0 &&
        rich.dark_identity_outlines > 0,
        rich.country_spans > 0, rich.alliance_spans > 0, rich.neutral_spans > 0,
        rich.dark_identity_outlines > 0);
    return ok && collapse.related_visible == 5 && union1.page_count == 2 && union2.page == 1 &&
           union1.related_visible + union2.related_visible == 12 &&
           rich.country_spans > 0 && rich.alliance_spans > 0 && rich.neutral_spans > 0 &&
           rich.dark_identity_outlines > 0;
}

int game_presentation_world_announcement_ui_probe(
    FILE *summary, const WorldAnnouncementProbeBundle *bundle) {
    int ok = queue_case(summary, bundle);
    ok &= artifact_cases(summary, bundle);
    return ok;
}
