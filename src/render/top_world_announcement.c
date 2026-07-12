#include "render/top_world_announcement.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "render/icons.h"
#include "render/render_common.h"
#include "render/snapshot_ui.h"
#include "render/top_world_announcement_surface.h"
#include "render/top_world_announcement_resources.h"
#include "sim/technology.h"
#include "ui/ui_layout.h"
#include "ui/ui_world_announcement.h"
#include "ui/world_announcement_queue.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ANNOUNCEMENT_SPAN_MAX 48

typedef struct {
    char text[192];
    COLORREF color;
    int identity_kind;
} AnnouncementSpan;

typedef struct {
    AnnouncementSpan spans[ANNOUNCEMENT_SPAN_MAX];
    int count;
} AnnouncementLine;

static TopWorldAnnouncementProbeInfo probe_info;
static int last_draw_ms;
static int peak_draw_ms;
static int layout_event_id = -1;
static int layout_actor_uid;
static int layout_related_count;
static int layout_first_uid;
static int layout_last_uid;
static int layout_available;
static int layout_language;
static int layout_pages;
static int layout_starts[MAX_CIVS + 1];

static const char *civ_name(const WorldAnnouncementCivIdentity *identity) {
    if (!identity || identity->uid <= 0) return ui_language == UI_LANG_ZH ? "未知国家" : "Unknown country";
    return ui_language == UI_LANG_ZH ? identity->name_zh : identity->name_en;
}

static const char *alliance_name(const WorldAnnouncementAllianceIdentity *identity) {
    if (!identity || !identity->valid) return ui_language == UI_LANG_ZH ? "未知联盟" : "Unknown alliance";
    return ui_language == UI_LANG_ZH ? identity->name_zh : identity->name_en;
}

static COLORREF identity_color(Color32 color) { return (COLORREF)color; }

static void append_span(AnnouncementLine *line, const char *text, COLORREF color, int identity_kind) {
    AnnouncementSpan *span;
    if (!line || !text || !text[0] || line->count >= ANNOUNCEMENT_SPAN_MAX) return;
    span = &line->spans[line->count++];
    snprintf(span->text, sizeof(span->text), "%s", text);
    span->color = color;
    span->identity_kind = identity_kind;
}

static void append_neutral(AnnouncementLine *line, const char *en, const char *zh) {
    append_span(line, ui_language == UI_LANG_ZH ? zh : en, RGB(236, 239, 240), 0);
}

static void append_format(AnnouncementLine *line, COLORREF color, const char *format, ...) {
    char text[192];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    append_span(line, text, color, 0);
}

static void append_civ(AnnouncementLine *line, const WorldAnnouncementCivIdentity *identity) {
    append_span(line, civ_name(identity), identity_color(identity ? identity->color : 0), 1);
}

static void append_alliance(AnnouncementLine *line,
                            const WorldAnnouncementAllianceIdentity *identity) {
    append_span(line, alliance_name(identity), identity_color(identity ? identity->color : 0), 2);
}

static int line_width(HDC hdc, const AnnouncementLine *line) {
    int i;
    int width = 0;
    for (i = 0; line && i < line->count; i++) {
        width += top_world_announcement_text_width(hdc, line->spans[i].text);
    }
    return width;
}

static void style_for_event(int type, const char **title, IconId *icon, COLORREF *accent) {
    *title = ui_language == UI_LANG_ZH ? "世界动态" : "World Announcement";
    *icon = ICON_TERRITORY;
    *accent = RGB(226, 188, 82);
    if (type == EVENT_TYPE_WORLD_TECH_AGE_FIRST || type == EVENT_TYPE_WORLD_DEEP_SEA_FIRST) {
        *title = ui_language == UI_LANG_ZH ? "科技时代里程碑" : "Technology Age Milestone";
        *icon = ICON_INNOVATION; *accent = RGB(104, 188, 232);
    } else if (type == EVENT_TYPE_COLLAPSE_SUCCEEDED) {
        *title = ui_language == UI_LANG_ZH ? "国家崩溃" : "Country Collapse";
        *icon = ICON_DISORDER; *accent = RGB(224, 92, 82);
    } else if (type == EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION) {
        *title = ui_language == UI_LANG_ZH ? "联盟联合" : "Alliance Union";
        *icon = ICON_COHESION; *accent = RGB(176, 126, 222);
    } else if (type == EVENT_TYPE_PLAGUE_STARTED || type == EVENT_TYPE_PLAGUE_ENDED) {
        *title = ui_language == UI_LANG_ZH ? "全球瘟疫" : "Global Plague";
        *icon = ICON_ADAPTATION; *accent = RGB(116, 196, 126);
    } else if (type == EVENT_TYPE_ALLIANCE_WAR_STARTED || type == EVENT_TYPE_ALLIANCE_WAR_ENDED ||
               type == EVENT_TYPE_VASSAL_INDEPENDENCE_WAR) {
        *title = ui_language == UI_LANG_ZH ? "世界战争" : "World War";
        *icon = ICON_ATTACK; *accent = RGB(224, 94, 76);
    } else if (type == EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED ||
               type == EVENT_TYPE_ALLIANCE_MILITARY_DOWNGRADED) {
        *title = ui_language == UI_LANG_ZH ? "军事联盟变更" : "Military Alliance Change";
        *icon = ICON_COUNTRY_DEFENSE; *accent = RGB(224, 176, 80);
    } else if (type == EVENT_TYPE_VASSAL_CREATED || type == EVENT_TYPE_VASSAL_RELEASED ||
               type == EVENT_TYPE_VASSAL_TRANSFERRED || type == EVENT_TYPE_VASSAL_ANNEXED ||
               type == EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE ||
               type == EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE) {
        *title = ui_language == UI_LANG_ZH ? "附庸关系变更" : "Vassal Status Change";
        *icon = ICON_GOVERNANCE; *accent = RGB(190, 132, 218);
    } else if (type >= EVENT_TYPE_ALLIANCE_CREATED && type <= EVENT_TYPE_ALLIANCE_MEMBER_REMOVED) {
        *title = ui_language == UI_LANG_ZH ? "联盟动态" : "Alliance Update";
        *icon = ICON_COHESION; *accent = RGB(96, 164, 224);
    }
}

static void append_war_side(AnnouncementLine *line, const WorldAnnouncementEvent *event, int side) {
    const WorldAnnouncementAllianceIdentity *alliance = side == 1 ? &event->alliance_a : &event->alliance_b;
    const WorldAnnouncementCivIdentity *civ = side == 1 ? &event->actor : &event->target;
    if (alliance->valid) append_alliance(line, alliance); else append_civ(line, civ);
}

static void build_list_prefix(AnnouncementLine *line, const WorldAnnouncementEvent *event, int continued) {
    if (continued) {
        append_neutral(line,
            event->event_type == EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION ? "Absorbed members (continued): " :
                                                                       "Successors (continued): ",
            event->event_type == EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION ? "被吸收成员（续）：" : "继承国（续）：");
        return;
    }
    if (event->event_type == EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION) {
        append_alliance(line, &event->alliance_a);
        append_neutral(line, " completed union; ", "完成联合；");
        append_civ(line, &event->actor);
        append_format(line, RGB(236, 239, 240), ui_language == UI_LANG_ZH ?
            "吸收了%d个成员国：" : " absorbed %d member states: ", event->related_count);
    } else {
        append_civ(line, &event->actor);
        append_format(line, RGB(236, 239, 240), ui_language == UI_LANG_ZH ?
            "崩溃，形成了%d个继承国：" : " collapsed, forming %d successor states: ",
            event->related_count);
    }
}

static int compute_list_pages(HDC hdc, const WorldAnnouncementEvent *event,
                              int available, int starts[MAX_CIVS + 1]) {
    AnnouncementLine first_prefix = {0}, continued_prefix = {0};
    int name_width[MAX_CIVS] = {0};
    int prefix_width[2], separator_width;
    SIZE size;
    int page = 0, index = 0, i;
    int first_uid = event->related_count > 0 ? event->related[0].uid : 0;
    int last_uid = event->related_count > 0 ? event->related[event->related_count - 1].uid : 0;
    if (layout_event_id == event->event_id && layout_actor_uid == event->actor.uid &&
        layout_related_count == event->related_count && layout_first_uid == first_uid &&
        layout_last_uid == last_uid && layout_available == available &&
        layout_language == ui_language) {
        memcpy(starts, layout_starts, sizeof(layout_starts));
        return layout_pages;
    }
    build_list_prefix(&first_prefix, event, 0);
    build_list_prefix(&continued_prefix, event, 1);
    prefix_width[0] = line_width(hdc, &first_prefix);
    prefix_width[1] = line_width(hdc, &continued_prefix);
    measure_text_utf8(hdc, ui_language == UI_LANG_ZH ? "、" : ", ", &size);
    separator_width = size.cx;
    for (i = 0; i < event->related_count; i++) {
        name_width[i] = top_world_announcement_text_width(
            hdc, civ_name(&event->related[i]));
    }
    while (index < event->related_count && page < MAX_CIVS) {
        int first = index;
        int width = prefix_width[page > 0];
        starts[page++] = index;
        while (index < event->related_count) {
            int next_width = name_width[index] + (index == first ? 0 : separator_width);
            if (index > first && width + next_width > available) break;
            width += next_width;
            index++;
        }
        if (index == first) index++;
    }
    starts[page] = event->related_count;
    layout_event_id = event->event_id;
    layout_actor_uid = event->actor.uid;
    layout_related_count = event->related_count;
    layout_first_uid = first_uid;
    layout_last_uid = last_uid;
    layout_available = available;
    layout_language = ui_language;
    layout_pages = max(1, page);
    memcpy(layout_starts, starts, sizeof(layout_starts));
    return layout_pages;
}

static void build_list_body(AnnouncementLine *line, const WorldAnnouncementEvent *event,
                            int page, const int starts[MAX_CIVS + 1]) {
    int i;
    build_list_prefix(line, event, page > 0);
    for (i = starts[page]; i < starts[page + 1]; i++) {
        if (i > starts[page]) append_neutral(line, ", ", "、");
        append_civ(line, &event->related[i]);
    }
    append_neutral(line, ".", "。");
}

static void build_body(AnnouncementLine *line, const WorldAnnouncementEvent *event) {
    int type = event->event_type;
    if (type == EVENT_TYPE_WORLD_TECH_AGE_FIRST) {
        append_civ(line, &event->actor); append_neutral(line, " became the first country to enter ", "成为首个进入");
        append_span(line, technology_stage_name(event->technology_stage, ui_language), RGB(100, 202, 234), 0);
        append_format(line, RGB(236, 239, 240), ui_language == UI_LANG_ZH ? "（%d）的国家。" : " (%d).", event->technology_stage);
    } else if (type == EVENT_TYPE_WORLD_DEEP_SEA_FIRST) {
        append_civ(line, &event->actor); append_neutral(line, " became the first country to unlock deep-sea navigation.", "成为首个解锁远洋航行的国家。");
    } else if (type == EVENT_TYPE_COLLAPSE_SUCCEEDED) {
        append_civ(line, &event->actor);
        if (event->target.uid > 0) { append_neutral(line, " collapsed and was absorbed by ", "崩溃并被"); append_civ(line, &event->target); append_neutral(line, ".", "吞并。"); }
        else append_neutral(line, " collapsed and disappeared in isolation.", "崩溃并因孤立而消失。");
    } else if (type == EVENT_TYPE_ALLIANCE_CREATED) {
        append_alliance(line, &event->alliance_a); append_neutral(line, " was founded by ", "由"); append_civ(line, &event->actor); append_neutral(line, " and ", "与"); append_civ(line, &event->target); append_neutral(line, ".", "建立。");
    } else if (type == EVENT_TYPE_ALLIANCE_DISSOLVED) {
        append_alliance(line, &event->alliance_a); append_neutral(line, " dissolved.", "解散。");
    } else if (type == EVENT_TYPE_ALLIANCE_MEMBER_JOINED) {
        append_civ(line, &event->target); append_neutral(line, " joined ", "加入"); append_alliance(line, &event->alliance_a); append_neutral(line, ".", "。");
    } else if (type == EVENT_TYPE_ALLIANCE_MEMBER_REMOVED) {
        append_civ(line, &event->actor); append_neutral(line, " left ", "离开"); append_alliance(line, &event->alliance_a); append_neutral(line, ".", "。");
    } else if (type == EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED || type == EVENT_TYPE_ALLIANCE_MILITARY_DOWNGRADED) {
        append_alliance(line, &event->alliance_a); append_neutral(line,
            type == EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED ? " upgraded to a Military Alliance." : " downgraded to a Defensive Alliance.",
            type == EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED ? "升级为军事联盟。" : "降级为防御联盟。");
    } else if (type == EVENT_TYPE_VASSAL_CREATED) {
        append_civ(line, &event->actor); append_neutral(line, " became a vassal of ", "成为"); append_civ(line, &event->target); append_neutral(line, ".", "的附庸。");
    } else if (type == EVENT_TYPE_VASSAL_TRANSFERRED) {
        append_civ(line, &event->actor); append_neutral(line, " transferred from ", "从"); append_civ(line, &event->target); append_neutral(line, " to ", "转为"); if (event->related_count) append_civ(line, &event->related[0]); append_neutral(line, ".", "的附庸。");
    } else if (type == EVENT_TYPE_VASSAL_ANNEXED) {
        append_civ(line, &event->target); append_neutral(line, " annexed vassal ", "吞并附庸"); append_civ(line, &event->actor); append_neutral(line, ".", "。");
    } else if (type == EVENT_TYPE_VASSAL_INDEPENDENCE_WAR) {
        append_civ(line, &event->actor); append_neutral(line, " declared a war of independence from its overlord (", "向宗主国（"); append_civ(line, &event->target); append_neutral(line, ").", "）宣布独立战争。");
    } else if (type == EVENT_TYPE_VASSAL_RELEASED || type == EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE || type == EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE) {
        append_civ(line, &event->actor); append_neutral(line, " became independent from ", "从"); append_civ(line, &event->target); append_neutral(line, ".", "独立。");
    } else if (type == EVENT_TYPE_PLAGUE_STARTED) {
        append_neutral(line, "A global plague outbreak began in ", "全球瘟疫在"); append_span(line, ui_language == UI_LANG_ZH ? event->location_name_zh : event->location_name_en, RGB(206, 216, 184), 0); append_neutral(line, " of ", "爆发，所属国家："); append_civ(line, &event->actor); append_neutral(line, ".", "。");
    } else if (type == EVENT_TYPE_PLAGUE_ENDED) {
        append_neutral(line, "The global active plague ended.", "全球活跃瘟疫已经结束。");
    } else if (type == EVENT_TYPE_ALLIANCE_WAR_STARTED) {
        append_war_side(line, event, 1); append_neutral(line, " entered war against ", "与"); append_war_side(line, event, 2); append_neutral(line, ".", "进入战争。");
    } else if (type == EVENT_TYPE_ALLIANCE_WAR_ENDED) {
        if (event->terminal_result == WORLD_ANNOUNCEMENT_TERMINAL_VICTORY) { append_war_side(line, event, event->winner_side); append_neutral(line, " defeated ", "战胜"); append_war_side(line, event, event->winner_side == 1 ? 2 : 1); append_neutral(line, ".", "。"); }
        else if (event->terminal_result == WORLD_ANNOUNCEMENT_TERMINAL_OFFENSIVE_HALTED) { append_war_side(line, event, 1); append_neutral(line, "'s offensive against ", "对"); append_war_side(line, event, 2); append_neutral(line, " was halted.", "的攻势受阻。"); }
        else if (event->terminal_result == WORLD_ANNOUNCEMENT_TERMINAL_FRONT_SEVERED) { append_neutral(line, "The front between ", ""); append_war_side(line, event, 1); append_neutral(line, " and ", "与"); append_war_side(line, event, 2); append_neutral(line, " was severed.", "的战线断绝。"); }
        else { append_war_side(line, event, 1); append_neutral(line, " and ", "与"); append_war_side(line, event, 2); append_neutral(line, " signed a truce agreement.", "签署停战协议。"); }
    }
    if (line->count == 0) append_neutral(line, "The world order changed.", "世界格局发生变化。");
}

static void draw_span_text(HDC hdc, int x, int y, const AnnouncementSpan *span) {
    if (span->identity_kind) {
        if (GetRValue(span->color) + GetGValue(span->color) + GetBValue(span->color) < 210) {
            draw_text_line(hdc, x - 1, y, span->text, RGB(226, 230, 232));
            probe_info.dark_identity_outlines++;
        } else draw_text_line(hdc, x + 1, y + 1, span->text, RGB(8, 10, 12));
    }
    draw_text_line(hdc, x, y, span->text, span->color);
}

static void draw_line(HDC hdc, RECT rect, AnnouncementLine *line) {
    HFONT old_font;
    int widths[ANNOUNCEMENT_SPAN_MAX];
    int x = rect.left;
    int y = rect.top;
    int i;
    old_font = SelectObject(hdc, top_world_announcement_body_font());
    for (i = 0; i < line->count; i++) {
        widths[i] = top_world_announcement_text_width(hdc, line->spans[i].text);
    }
    SaveDC(hdc);
    IntersectClipRect(hdc, rect.left, rect.top, rect.right, rect.bottom);
    for (i = 0; i < line->count; i++) {
        if (x > rect.left && x + widths[i] > rect.right) {
            x = rect.left;
            y += 20;
        }
        draw_span_text(hdc, x, y, &line->spans[i]);
        x += widths[i];
    }
    RestoreDC(hdc, -1);
    SelectObject(hdc, old_font);
}

static void draw_control(HDC hdc, RECT client, WorldAnnouncementControl control,
                         int visible, const char *symbol) {
    RECT rect;
    int hot;
    HFONT old_font;
    if (!visible) return;
    rect = get_world_announcement_control_rect(client, control);
    hot = ui_world_announcement_hover_control(client, hover_x, hover_y) == (int)control;
    if (hot) fill_rect_alpha(hdc, rect, RGB(110, 128, 138), 110);
    if (control == WORLD_ANNOUNCEMENT_CONTROL_LOCATE)
        top_world_announcement_draw_cached_icon(hdc, ICON_TERRITORY, rect,
                                                RGB(222, 228, 232));
    else {
        old_font = SelectObject(hdc, top_world_announcement_metadata_font());
        draw_text_rect(hdc, rect, symbol, RGB(238, 241, 242), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        SelectObject(hdc, old_font);
    }
}

int draw_top_world_announcement(HDC hdc, RECT client) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    const WorldAnnouncementEvent *event;
    AnnouncementLine line = {0};
    RECT band = get_world_announcement_rect(client);
    RECT body;
    RECT meta;
    RECT title_rect;
    const char *title;
    const char *tooltip = NULL;
    IconId icon;
    COLORREF accent;
    char header[224];
    char state[96];
    int starts[MAX_CIVS + 1];
    int pages = 1;
    int page;
    int controls_left;
    int hover_control;
    int progress;
    int list_event;
    int surface_state;
    HDC draw_hdc;
    TopWorldAnnouncementSurfaceKey surface_key = {0};
    HFONT old_font;
    int i;
    int elapsed_ms;
    int layout_ms;
    int content_ms = 0;
    int composite_ms;
    long long render_start;
    long long phase_start;
    world_announcement_queue_consume(snapshot);
    event = world_announcement_queue_current();
    if (!event || band.right <= band.left) {
        top_world_announcement_resources_ensure();
        top_world_announcement_resources_prewarm(hdc);
        memset(&probe_info, 0, sizeof(probe_info));
        return 0;
    }
    top_world_announcement_resources_ensure();
    style_for_event(event->event_type, &title, &icon, &accent);
    preload_icon(icon);
    render_start = profiler_now_us();
    phase_start = render_start;
    list_event = event->event_type == EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION ||
                 event->event_type == EVENT_TYPE_COLLAPSE_SUCCEEDED;
    body = (RECT){band.left + 42, band.top + 39, band.right - 10, band.bottom - 6};
    old_font = SelectObject(hdc, top_world_announcement_body_font());
    if (list_event && event->related_count > 0) {
        pages = compute_list_pages(hdc, event, body.right - body.left, starts);
    }
    SelectObject(hdc, old_font);
    world_announcement_queue_set_page_count(pages);
    page = min(world_announcement_queue_current_page(), pages - 1);
    hover_control = ui_world_announcement_hover_control(client, hover_x, hover_y);
    surface_key.event_id = event->event_id;
    surface_key.event_type = event->event_type;
    surface_key.year = event->year;
    surface_key.month = event->month;
    surface_key.actor_uid = event->actor.uid;
    surface_key.target_uid = event->target.uid;
    surface_key.alliance_a_id = event->alliance_a.alliance_id;
    surface_key.alliance_b_id = event->alliance_b.alliance_id;
    surface_key.related_count = event->related_count;
    if (event->related_count > 0) {
        surface_key.related_first_uid = event->related[0].uid;
        surface_key.related_last_uid = event->related[event->related_count - 1].uid;
    }
    surface_key.detail_key = event->terminal_result | (event->winner_side << 4) |
                             (event->technology_stage << 8) | (event->war_class << 16);
    surface_key.page = page;
    surface_key.page_count = pages;
    surface_key.pending_count = world_announcement_queue_pending_count();
    surface_key.language = ui_language;
    surface_key.hover_control = hover_control;
    surface_state = top_world_announcement_surface_begin(hdc, band, &surface_key, &draw_hdc);
    layout_ms = profiler_elapsed_ms_since_us(phase_start);
    if (surface_state < 0)
        top_world_announcement_surface_fill_alpha(draw_hdc, band, RGB(24, 30, 34), 191);
    if (surface_state != 0) {
        phase_start = profiler_now_us();
        if (list_event && event->related_count > 0) build_list_body(&line, event, page, starts);
        else build_body(&line, event);
        memset(&probe_info, 0, sizeof(probe_info));
        probe_info.active = 1;
        probe_info.background_alpha = 191;
        probe_info.page = page;
        probe_info.page_count = pages;
        if (list_event && event->related_count > 0) {
        probe_info.related_first = starts[page];
        probe_info.related_visible = starts[page + 1] - starts[page];
    }
        probe_info.header_font_px = 20;
        probe_info.body_font_px = 18;
        probe_info.metadata_font_px = 16;
        probe_info.minimum_body_font_px = 18;
        probe_info.compact_shrink_enabled = 0;
        for (i = 0; i < line.count; i++) {
            if (line.spans[i].identity_kind == 1) {
                probe_info.country_spans++;
                if (!probe_info.first_country_color) probe_info.first_country_color = line.spans[i].color;
            } else if (line.spans[i].identity_kind == 2) {
                probe_info.alliance_spans++;
                if (!probe_info.first_alliance_color) probe_info.first_alliance_color = line.spans[i].color;
            } else probe_info.neutral_spans++;
        }
        if (top_world_announcement_border_brush())
            FrameRect(draw_hdc, &band, top_world_announcement_border_brush());
        fill_rect(draw_hdc, (RECT){band.left, band.bottom - 2, band.right, band.bottom}, accent);
        top_world_announcement_surface_draw_icon(draw_hdc, band, icon, accent);
    controls_left = get_world_announcement_control_rect(client,
        pages > 1 ? WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS : WORLD_ANNOUNCEMENT_CONTROL_LOCATE).left;
    meta = (RECT){max(band.left + 220, controls_left - 180), band.top + 7,
                  controls_left - 6, band.top + 35};
    title_rect = (RECT){band.left + 42, band.top + 6, meta.left - 6, band.top + 36};
    snprintf(header, sizeof(header), "%s  |  %s %d %s %d", title,
             ui_language == UI_LANG_ZH ? "年" : "Year", event->year,
             ui_language == UI_LANG_ZH ? "月" : "Month", event->month);
        old_font = SelectObject(draw_hdc, top_world_announcement_header_font());
        draw_text_rect(draw_hdc, title_rect, header, accent,
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (hover_control == WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS) tooltip = ui_language == UI_LANG_ZH ? "上一页" : "Previous page";
    else if (hover_control == WORLD_ANNOUNCEMENT_CONTROL_NEXT) tooltip = ui_language == UI_LANG_ZH ? "下一页" : "Next page";
    else if (hover_control == WORLD_ANNOUNCEMENT_CONTROL_LOCATE) tooltip = ui_language == UI_LANG_ZH ? "定位" : "Locate";
    else if (hover_control == WORLD_ANNOUNCEMENT_CONTROL_DISMISS) tooltip = ui_language == UI_LANG_ZH ? "关闭" : "Dismiss";
    if (tooltip) snprintf(state, sizeof(state), "%s", tooltip);
    else if (pages > 1) snprintf(state, sizeof(state),
        ui_language == UI_LANG_ZH ? "队列 %d  %d/%d" : "%d queued  %d/%d",
        world_announcement_queue_pending_count(), page + 1, pages);
    else snprintf(state, sizeof(state), ui_language == UI_LANG_ZH ? "队列 %d" : "%d queued",
                  world_announcement_queue_pending_count());
        SelectObject(draw_hdc, top_world_announcement_metadata_font());
        draw_text_rect(draw_hdc, meta, state, RGB(208, 216, 220),
                       DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(draw_hdc, old_font);
        draw_line(draw_hdc, body, &line);
        draw_control(draw_hdc, client, WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS, pages > 1, "<");
        draw_control(draw_hdc, client, WORLD_ANNOUNCEMENT_CONTROL_NEXT, pages > 1, ">");
        draw_control(draw_hdc, client, WORLD_ANNOUNCEMENT_CONTROL_LOCATE, 1, "");
        draw_control(draw_hdc, client, WORLD_ANNOUNCEMENT_CONTROL_DISMISS, 1, "x");
        if (surface_state > 0) top_world_announcement_surface_finish();
        content_ms = profiler_elapsed_ms_since_us(phase_start);
    }
    world_announcement_queue_mark_page_visible();
    phase_start = profiler_now_us();
    if (surface_state >= 0) top_world_announcement_surface_blit(hdc, band);
    progress = 1000 - world_announcement_queue_progress_permille();
    fill_rect(hdc, (RECT){band.left + 1, band.bottom - 4,
              band.left + 1 + (band.right - band.left - 2) * progress / 1000, band.bottom - 2}, accent);
    GdiFlush();
    composite_ms = profiler_elapsed_ms_since_us(phase_start);
    top_world_announcement_note_draw_phases(layout_ms, content_ms, composite_ms);
    elapsed_ms = profiler_elapsed_ms_since_us(render_start);
    last_draw_ms = elapsed_ms;
    if (elapsed_ms > peak_draw_ms) peak_draw_ms = elapsed_ms;
    profiler_record_render_subphase(PROFILER_RENDER_SUB_TOP_BOTTOM_BAR,
        PROFILER_SPIKE_PRESENTATION, "World announcement", elapsed_ms);
    return 1;
}

TopWorldAnnouncementProbeInfo top_world_announcement_probe_info(void) { return probe_info; }
int top_world_announcement_last_draw_ms(void) { return last_draw_ms; }
int top_world_announcement_peak_draw_ms(void) { return peak_draw_ms; }
void top_world_announcement_reset_draw_metrics(void) {
    last_draw_ms = peak_draw_ms = 0;
    top_world_announcement_reset_phase_metrics();
    top_world_announcement_surface_invalidate();
}
