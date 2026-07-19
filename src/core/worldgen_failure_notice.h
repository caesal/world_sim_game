#ifndef WORLD_SIM_WORLDGEN_FAILURE_NOTICE_H
#define WORLD_SIM_WORLDGEN_FAILURE_NOTICE_H

#include "core/worldgen_attempt.h"

#include <stddef.h>

int worldgen_failure_notice_format(WorldGenFailureReason reason,
                                   char *out_en, size_t out_en_size,
                                   char *out_zh, size_t out_zh_size);
int worldgen_failure_notice_publish(const WorldGenAttemptDiagnostics *attempt);

#endif
