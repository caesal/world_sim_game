#include "core/worldgen_failure_notice.h"

#include "core/game_notifications.h"

#include <stdio.h>

int worldgen_failure_notice_format(WorldGenFailureReason reason,
                                   char *out_en, size_t out_en_size,
                                   char *out_zh, size_t out_zh_size) {
    if (!out_en || !out_en_size || !out_zh || !out_zh_size ||
        reason == WORLDGEN_FAILURE_NONE) return 0;
    snprintf(out_en, out_en_size,
             "World generation failed: %s. The previous map was retained.",
             worldgen_failure_reason_text_en(reason));
    snprintf(out_zh, out_zh_size,
             "世界生成失败：%s。已保留上一张地图。",
             worldgen_failure_reason_text_zh(reason));
    return 1;
}

int worldgen_failure_notice_publish(const WorldGenAttemptDiagnostics *attempt) {
    char text_en[GAME_NOTIFICATION_TEXT];
    char text_zh[GAME_NOTIFICATION_TEXT];
    if (!attempt || attempt->success ||
        !worldgen_failure_notice_format(attempt->last_failure_reason,
                                        text_en, sizeof(text_en),
                                        text_zh, sizeof(text_zh))) return 0;
    game_notifications_push(text_en, text_zh);
    return 1;
}
