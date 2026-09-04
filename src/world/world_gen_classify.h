#ifndef WORLD_SIM_WORLD_GEN_CLASSIFY_H
#define WORLD_SIM_WORLD_GEN_CLASSIFY_H

#include "core/world_types.h"
#include "world/world_gen_context.h"

int world_gen_desert_base(void);
int world_gen_desert_bias_span(void);
int world_gen_semi_arid_width(void);
int world_gen_oasis_transition_margin(void);
int world_gen_classify_validation_set_aridity(
    int desert_base, int bias_span, int semi_arid_width,
    int oasis_transition_margin);
void world_gen_classify_validation_reset_aridity(void);
int world_gen_classify_validation_aridity_active(void);
int world_gen_classify_validation_set_desert_bias_span(int bias_span);
void world_gen_classify_validation_reset_desert_bias_span(void);
int world_gen_classify_validation_desert_bias_span_active(void);
int world_gen_desert_moisture_limit(int bias_desert);
int world_gen_semi_arid_moisture_limit(int bias_desert);
int world_gen_oasis_moisture_limit(int drought);
int world_gen_oasis_transition_moisture_limit(int bias_desert, int drought);
int world_gen_oasis_transition_moisture_limit_for_margin(
    int bias_desert, int drought, int oasis_transition_margin);
int world_gen_classify_oasis_predicate_for_margin(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_transition_margin);
int world_gen_classify_visible_oasis_for_margin(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_transition_margin);
int world_gen_classify_response_oasis_predicate_for_pair(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_drop, int transition_margin);
int world_gen_classify_response_oasis_pair_independent_eligible(
    const WorldGenContext *context, int index, Climate climate);
int world_gen_classify_response_oasis_visible_eligible(
    const WorldGenContext *context, int index, Climate climate);
int world_gen_classify_response_oasis_moisture_in_window(
    int drought, int moisture, int oasis_limit, int oasis_transition_limit);
int world_gen_classify_response_visible_oasis_for_pair(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_drop, int transition_margin);
Geography world_gen_classify_underlying_land(const WorldGenContext *context,
                                              int index, int x, int y,
                                              Climate climate);
int world_gen_classify_macro_climate(WorldGenContext *context);
int world_gen_classify_final(WorldGenContext *context);

#endif
