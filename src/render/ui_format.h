#ifndef WORLD_SIM_UI_FORMAT_H
#define WORLD_SIM_UI_FORMAT_H

#include <stddef.h>

typedef enum {
    UI_MONTH_ZERO_NOW,
    UI_MONTH_ZERO_DONE
} UiMonthZeroMode;

void ui_format_months(char *buffer, size_t size, int months, UiMonthZeroMode zero_mode);

#endif
