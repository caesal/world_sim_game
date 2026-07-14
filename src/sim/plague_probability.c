#include "sim/plague_probability.h"

#include <string.h>

static int clamp_probability(int value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

static int probability_value(const PlagueProbabilityDistribution *probabilities,
                             PlagueProbabilityBucket bucket) {
    if (!probabilities) return 0;
    switch (bucket) {
        case PLAGUE_PROBABILITY_NO_PLAGUE: return probabilities->no_plague;
        case PLAGUE_PROBABILITY_SMALL: return probabilities->small;
        case PLAGUE_PROBABILITY_MEDIUM: return probabilities->medium;
        case PLAGUE_PROBABILITY_LARGE: return probabilities->large;
        default: return 0;
    }
}

static void set_probability_value(PlagueProbabilityDistribution *probabilities,
                                  PlagueProbabilityBucket bucket, int value) {
    if (!probabilities) return;
    switch (bucket) {
        case PLAGUE_PROBABILITY_NO_PLAGUE: probabilities->no_plague = value; break;
        case PLAGUE_PROBABILITY_SMALL: probabilities->small = value; break;
        case PLAGUE_PROBABILITY_MEDIUM: probabilities->medium = value; break;
        case PLAGUE_PROBABILITY_LARGE: probabilities->large = value; break;
        default: break;
    }
}

static void allocate_proportionally(int total, const int *weights, int count,
                                    int *allocated) {
    int remainders[3] = {0, 0, 0};
    int weight_total = 0;
    int allocated_total = 0;
    int i;
    if (!weights || !allocated || count < 1 || count > 3) return;
    for (i = 0; i < count; i++) {
        weight_total += weights[i];
        allocated[i] = 0;
    }
    if (total <= 0 || weight_total <= 0) return;
    for (i = 0; i < count; i++) {
        int product = total * weights[i];
        allocated[i] = product / weight_total;
        remainders[i] = product % weight_total;
        allocated_total += allocated[i];
    }
    while (allocated_total < total) {
        int best = 0;
        for (i = 1; i < count; i++) {
            if (remainders[i] > remainders[best]) best = i;
        }
        allocated[best]++;
        allocated_total++;
        remainders[best] = -1;
    }
}

void plague_probability_defaults(PlagueProbabilityDistribution *out) {
    if (!out) return;
    out->no_plague = 50;
    out->small = 25;
    out->medium = 15;
    out->large = 10;
}

int plague_probability_validate(const PlagueProbabilityDistribution *probabilities) {
    if (!probabilities || probabilities->no_plague < 0 ||
        probabilities->no_plague > 100 || probabilities->small < 0 ||
        probabilities->small > 100 || probabilities->medium < 0 ||
        probabilities->medium > 100 || probabilities->large < 0 ||
        probabilities->large > 100) return 0;
    return probabilities->no_plague + probabilities->small +
           probabilities->medium + probabilities->large == 100;
}

int plague_probability_equal(const PlagueProbabilityDistribution *a,
                             const PlagueProbabilityDistribution *b) {
    return a && b && a->no_plague == b->no_plague && a->small == b->small &&
           a->medium == b->medium && a->large == b->large;
}

PlagueSize plague_probability_size_from_roll(
    const PlagueProbabilityDistribution *probabilities, int roll_0_to_99) {
    int roll;
    if (!plague_probability_validate(probabilities)) return PLAGUE_SIZE_NONE;
    roll = clamp_probability(roll_0_to_99);
    if (roll == 100) roll = 99;
    if (roll < probabilities->no_plague) return PLAGUE_SIZE_NONE;
    roll -= probabilities->no_plague;
    if (roll < probabilities->small) return PLAGUE_SIZE_SMALL;
    roll -= probabilities->small;
    if (roll < probabilities->medium) return PLAGUE_SIZE_MEDIUM;
    return PLAGUE_SIZE_LARGE;
}

static int adjust_no_plague(const PlagueProbabilityDistribution *current,
                            int requested_value,
                            PlagueProbabilityDistribution *out) {
    int weights[3] = {current->small, current->medium, current->large};
    int allocated[3];
    int plague_total;
    if (weights[0] + weights[1] + weights[2] == 0) {
        weights[0] = 5;
        weights[1] = 3;
        weights[2] = 2;
    }
    requested_value = clamp_probability(requested_value);
    plague_total = 100 - requested_value;
    allocate_proportionally(plague_total, weights, 3, allocated);
    out->no_plague = requested_value;
    out->small = allocated[0];
    out->medium = allocated[1];
    out->large = allocated[2];
    return 1;
}

static int adjust_plague_size(const PlagueProbabilityDistribution *current,
                              PlagueProbabilityBucket bucket, int requested_value,
                              PlagueProbabilityDistribution *out) {
    PlagueProbabilityBucket others[2];
    int old_value = probability_value(current, bucket);
    int delta;
    int i;
    int other_count = 0;
    requested_value = clamp_probability(requested_value);
    *out = *current;
    delta = requested_value - old_value;
    set_probability_value(out, bucket, requested_value);
    if (delta <= 0) {
        out->no_plague -= delta;
        return 1;
    }
    if (delta <= current->no_plague) {
        out->no_plague = current->no_plague - delta;
        return 1;
    }
    out->no_plague = 0;
    for (i = PLAGUE_PROBABILITY_SMALL; i <= PLAGUE_PROBABILITY_LARGE; i++) {
        if (i != (int)bucket) {
            others[other_count++] = (PlagueProbabilityBucket)i;
        }
    }
    {
        int residual = delta - current->no_plague;
        int weights[2] = {
            probability_value(current, others[0]),
            probability_value(current, others[1])
        };
        int allocated[2];
        int remaining = weights[0] + weights[1] - residual;
        allocate_proportionally(remaining, weights, 2, allocated);
        set_probability_value(out, others[0], allocated[0]);
        set_probability_value(out, others[1], allocated[1]);
    }
    return 1;
}

int plague_probability_adjust_linked(
    const PlagueProbabilityDistribution *current, PlagueProbabilityBucket bucket,
    int requested_value, PlagueProbabilityDistribution *out) {
    int adjusted;
    if (!out || !plague_probability_validate(current) ||
        bucket < PLAGUE_PROBABILITY_NO_PLAGUE ||
        bucket >= PLAGUE_PROBABILITY_COUNT) return 0;
    if (bucket == PLAGUE_PROBABILITY_NO_PLAGUE) {
        adjusted = adjust_no_plague(current, requested_value, out);
    } else {
        adjusted = adjust_plague_size(current, bucket, requested_value, out);
    }
    return adjusted && plague_probability_validate(out) &&
           probability_value(out, bucket) == clamp_probability(requested_value);
}

void plague_probability_model_reset(PlagueModelState *model) {
    if (!model) return;
    plague_probability_defaults(&model->effective_probabilities);
    model->pending_probabilities = model->effective_probabilities;
    model->pending_probabilities_valid = 0;
}

int plague_probability_model_validate(const PlagueModelState *model) {
    if (!model || !plague_probability_validate(&model->effective_probabilities) ||
        !plague_probability_validate(&model->pending_probabilities) ||
        (model->pending_probabilities_valid != 0 &&
         model->pending_probabilities_valid != 1)) return 0;
    if (model->pending_probabilities_valid) return model->episode.active != 0;
    return plague_probability_equal(&model->effective_probabilities,
                                    &model->pending_probabilities);
}

int plague_probability_model_apply(
    PlagueModelState *model, const PlagueProbabilityDistribution *probabilities) {
    if (!model || !plague_probability_validate(probabilities)) return 0;
    if (!model->episode.active) {
        model->effective_probabilities = *probabilities;
        model->pending_probabilities = *probabilities;
        model->pending_probabilities_valid = 0;
    } else if (plague_probability_equal(probabilities,
                                        &model->effective_probabilities)) {
        model->pending_probabilities = model->effective_probabilities;
        model->pending_probabilities_valid = 0;
    } else {
        model->pending_probabilities = *probabilities;
        model->pending_probabilities_valid = 1;
    }
    return 1;
}

int plague_probability_model_promote_pending(PlagueModelState *model) {
    if (!model || model->episode.active || !model->pending_probabilities_valid) return 0;
    model->effective_probabilities = model->pending_probabilities;
    model->pending_probabilities_valid = 0;
    model->pending_probabilities = model->effective_probabilities;
    return 1;
}

void plague_probability_model_copy_view(const PlagueModelState *model,
                                        PlagueStateView *view) {
    if (!model || !view) return;
    view->effective_probabilities = model->effective_probabilities;
    view->pending_probabilities = model->pending_probabilities;
    view->pending_probabilities_valid = model->pending_probabilities_valid;
}
