#include "game/game_presentation_plague_probability_probe.h"

#include "game/game_presentation_plague_probability_artifact_probe.h"
#include "game/game_presentation_plague_probability_contract_probe.h"

int game_presentation_plague_probability_probe(FILE *summary) {
    int contract =
        game_presentation_plague_probability_contract_probe(summary);
    int artifacts =
        game_presentation_plague_probability_artifact_probe(summary);
    int ok = contract && artifacts;
    fprintf(summary,
            "case=plague_probability_presentation ok=%d contract=%d artifacts=%d\n",
            ok, contract, artifacts);
    return ok;
}
