#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_RESULT_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_RESULT_H

#include "render/render_common.h"

const char *panel_country_diplomacy_result_text(int local_identity,
                                                int winner_identity,
                                                int loser_identity,
                                                int result);
COLORREF panel_country_diplomacy_result_color(int local_identity,
                                              int winner_identity,
                                              int loser_identity,
                                              int result);

#endif
