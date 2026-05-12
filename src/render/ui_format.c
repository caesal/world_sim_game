#include "render/ui_format.h"

#include "render/render_common.h"

#include <stdio.h>

void ui_format_months(char *buffer, size_t size, int months, UiMonthZeroMode zero_mode) {
    const char *zero = zero_mode == UI_MONTH_ZERO_DONE ? tr("Done", "已完成") : tr("Now", "现在");
    int years;
    int rem;

    if (!buffer || size == 0) return;
    if (months <= 0) {
        snprintf(buffer, size, "%s", zero);
        return;
    }
    if (months < 12) {
        snprintf(buffer, size, tr("%dm", "%d个月"), months);
        return;
    }
    years = months / 12;
    rem = months % 12;
    if (rem == 0) {
        snprintf(buffer, size, tr("%dy", "%d年"), years);
    } else {
        snprintf(buffer, size, tr("%dy %dm", "%d年%d个月"), years, rem);
    }
}
