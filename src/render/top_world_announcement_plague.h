#ifndef WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_PLAGUE_H
#define WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_PLAGUE_H

#include "core/world_announcement_types.h"
#include "render/icons.h"

#define ANNOUNCEMENT_SPAN_MAX 48

typedef struct {
    char text[384];
    COLORREF color;
    int identity_kind;
} AnnouncementSpan;

typedef struct {
    AnnouncementSpan spans[ANNOUNCEMENT_SPAN_MAX];
    int count;
} AnnouncementLine;

typedef struct {
    void (*append_span)(AnnouncementLine *line, const char *text,
                        COLORREF color, int identity_kind);
    void (*append_neutral)(AnnouncementLine *line, const char *en, const char *zh);
    void (*append_civ)(AnnouncementLine *line,
                       const WorldAnnouncementCivIdentity *identity);
} TopWorldAnnouncementPlagueOps;

int top_world_announcement_plague_style(int type, int language,
                                        const char **title, IconId *icon,
                                        COLORREF *accent);
int top_world_announcement_plague_append_body(
    AnnouncementLine *line, const WorldAnnouncementEvent *event, int language,
    const TopWorldAnnouncementPlagueOps *ops);

#endif
