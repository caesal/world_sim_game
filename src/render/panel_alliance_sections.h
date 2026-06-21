#ifndef WORLD_SIM_PANEL_ALLIANCE_SECTIONS_H
#define WORLD_SIM_PANEL_ALLIANCE_SECTIONS_H

#include "render/panel_alliance_detail.h"

int alliance_sections_members_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
int alliance_sections_votes_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
int alliance_sections_history_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
void alliance_sections_draw_members(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                    const AlliancePanelRow *row);
void alliance_sections_draw_votes(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                  const AlliancePanelRow *row);
void alliance_sections_draw_history(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                    const AlliancePanelRow *row);

#endif
