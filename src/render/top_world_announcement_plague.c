#include "render/top_world_announcement_plague.h"

#include "core/event_log_plague.h"
#include "core/game_types.h"
#include "ui/ui_types.h"

int top_world_announcement_plague_style(int type, int language,
                                        const char **title, IconId *icon,
                                        COLORREF *accent) {
    if (type != EVENT_TYPE_PLAGUE_STARTED && type != EVENT_TYPE_PLAGUE_ENDED) return 0;
    *title = language == UI_LANG_ZH ? "全球瘟疫" : "Global Plague";
    *icon = ICON_ADAPTATION;
    *accent = RGB(116, 196, 126);
    return 1;
}

int top_world_announcement_plague_append_body(
    AnnouncementLine *line, const WorldAnnouncementEvent *event, int language,
    const TopWorldAnnouncementPlagueOps *ops) {
    char payload_text[384];
    if (!line || !event || !ops) return 0;
    if (event_log_plague_format_payload(&event->plague, language,
                                        payload_text, sizeof(payload_text))) {
        ops->append_span(line, payload_text, RGB(206, 216, 184), 0);
        return 1;
    }
    if (event->event_type == EVENT_TYPE_PLAGUE_STARTED) {
        ops->append_neutral(line, "A global plague outbreak began in ", "全球瘟疫在");
        ops->append_span(line, language == UI_LANG_ZH ? event->location_name_zh :
                                                     event->location_name_en,
                         RGB(206, 216, 184), 0);
        ops->append_neutral(line, " of ", "爆发，所属国家：");
        ops->append_civ(line, &event->actor);
        ops->append_neutral(line, ".", "。");
        return 1;
    }
    if (event->event_type == EVENT_TYPE_PLAGUE_ENDED) {
        ops->append_neutral(line, "The global active plague ended.",
                           "全球活跃瘟疫已经结束。");
        return 1;
    }
    return 0;
}
