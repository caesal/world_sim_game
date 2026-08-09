#ifndef WORLD_SIM_GAME_DECISION_CACHE_PROBE_FIXTURE_H
#define WORLD_SIM_GAME_DECISION_CACHE_PROBE_FIXTURE_H

#include <stdint.h>

void decision_cache_probe_setup_fixture(int alive_count, int uid_base);
int decision_cache_probe_setup_generated_fixture(int alive_count, int uid_base);
int decision_cache_probe_worldgen_first_publication_ok(void);
void decision_cache_probe_advance_month(void);
int decision_cache_probe_alive_count(void);
uint64_t decision_cache_probe_published_hash(void);
int decision_cache_probe_verify_publication(int expected_alive, uint64_t expected_revision,
                                            int *missing, int *uid_mismatch,
                                            int *fallback_zero);

#endif
