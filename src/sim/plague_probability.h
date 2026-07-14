#ifndef WORLD_SIM_PLAGUE_PROBABILITY_H
#define WORLD_SIM_PLAGUE_PROBABILITY_H

#include "sim/plague_types.h"

void plague_probability_defaults(PlagueProbabilityDistribution *out);
int plague_probability_validate(const PlagueProbabilityDistribution *probabilities);
int plague_probability_equal(const PlagueProbabilityDistribution *a,
                             const PlagueProbabilityDistribution *b);
PlagueSize plague_probability_size_from_roll(
    const PlagueProbabilityDistribution *probabilities, int roll_0_to_99);
int plague_probability_adjust_linked(
    const PlagueProbabilityDistribution *current, PlagueProbabilityBucket bucket,
    int requested_value, PlagueProbabilityDistribution *out);

void plague_probability_model_reset(PlagueModelState *model);
int plague_probability_model_validate(const PlagueModelState *model);
int plague_probability_model_apply(
    PlagueModelState *model, const PlagueProbabilityDistribution *probabilities);
int plague_probability_model_promote_pending(PlagueModelState *model);
void plague_probability_model_copy_view(const PlagueModelState *model,
                                        PlagueStateView *view);

#endif
