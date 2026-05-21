#ifndef WORLD_SIM_UI_L10N_H
#define WORLD_SIM_UI_L10N_H

#include "core/game_state.h"
#include "ui/ui_types.h"

static inline const char *ui_l10n(const char *en, const char *zh) {
    return ui_language == UI_LANG_ZH ? zh : en;
}

#endif
