#ifndef WORLD_SIM_WORLD_GEN_ARIDITY_RESPONSE_H
#define WORLD_SIM_WORLD_GEN_ARIDITY_RESPONSE_H

typedef struct {
    int combined_arid_limit;
    int semi_arid_band;
    int desert_limit;
    int semi_arid_limit;
    int oasis_limit;
    int oasis_transition_limit;
} WorldGenAridityResponseLimits;

int world_gen_aridity_response_calculate(
    int world_moisture, int drought, int bias_desert,
    int moisture_compression_span, int oasis_drop, int transition_margin,
    WorldGenAridityResponseLimits *limits);
int world_gen_aridity_response_calculate_expanded(
    int world_moisture, int drought, int bias_desert, int arid_base,
    int desert_bias_span, int moisture_compression_span, int oasis_drop,
    int transition_margin, WorldGenAridityResponseLimits *limits);
int world_gen_aridity_response_calculate_diminishing(
    int world_moisture, int drought, int bias_desert, int arid_base,
    int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin,
    WorldGenAridityResponseLimits *limits);
int world_gen_aridity_response_validation_enable(
    int moisture_compression_span, int oasis_drop, int transition_margin);
int world_gen_aridity_response_validation_enable_expanded(
    int arid_base, int desert_bias_span, int moisture_compression_span,
    int oasis_drop, int transition_margin);
int world_gen_aridity_response_validation_enable_diminishing(
    int arid_base, int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin);
void world_gen_aridity_response_validation_reset(void);
int world_gen_aridity_response_validation_active(void);
int world_gen_aridity_response_validation_matches(
    int moisture_compression_span, int oasis_drop, int transition_margin);
int world_gen_aridity_response_validation_matches_expanded(
    int arid_base, int desert_bias_span, int moisture_compression_span,
    int oasis_drop, int transition_margin);
int world_gen_aridity_response_validation_matches_diminishing(
    int arid_base, int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin);
int world_gen_aridity_response_arid_base(void);
int world_gen_aridity_response_desert_bias_span(void);
int world_gen_aridity_response_drought_classification_span(void);
int world_gen_aridity_response_moisture_compression_span(void);
int world_gen_aridity_response_oasis_drop(void);
int world_gen_aridity_response_transition_margin(void);
int world_gen_aridity_response_current_limits(
    int world_moisture, int drought, int bias_desert,
    WorldGenAridityResponseLimits *limits);

#endif
