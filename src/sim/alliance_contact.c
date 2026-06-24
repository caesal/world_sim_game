#include "sim/alliance_contact.h"

#include "core/game_state.h"
#include "sim/alliance.h"
#include "sim/diplomacy.h"
#include "sim/vassal.h"

static int sovereign_alive(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS &&
           civs[civ_id].alive && vassal_overlord(civ_id) < 0;
}

static AllianceDiplomaticContactSource contact_with_member(int civ_id, int member_civ_id) {
    int i;
    if (diplomacy_direct_contact_kind(civ_id, member_civ_id) != DIP_CONTACT_NONE)
        return ALLIANCE_DIP_CONTACT_DIRECT;
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (!civs[i].alive || vassal_overlord(i) != member_civ_id) continue;
        if (diplomacy_direct_contact_kind(civ_id, i) != DIP_CONTACT_NONE)
            return ALLIANCE_DIP_CONTACT_VASSAL_PROXY;
    }
    return ALLIANCE_DIP_CONTACT_NONE;
}

AllianceDiplomaticContactSource alliance_diplomatic_contact_source(int civ_id,
                                                                   int alliance_id,
                                                                   int member_civ_id) {
    AllianceDiplomaticContactSource direct;
    int i, count;
    if (!sovereign_alive(civ_id) || !sovereign_alive(member_civ_id) ||
        alliance_id < 0 || !alliance_is_formal_member(alliance_id, member_civ_id))
        return ALLIANCE_DIP_CONTACT_NONE;
    direct = contact_with_member(civ_id, member_civ_id);
    if (direct == ALLIANCE_DIP_CONTACT_DIRECT) return direct;
    count = alliance_member_count(alliance_id);
    for (i = 0; i < count; i++) {
        int member = alliance_formal_member_at(alliance_id, i);
        if (member == member_civ_id || !sovereign_alive(member)) continue;
        if (contact_with_member(civ_id, member) != ALLIANCE_DIP_CONTACT_NONE)
            return ALLIANCE_DIP_CONTACT_ALLIANCE;
    }
    return direct == ALLIANCE_DIP_CONTACT_VASSAL_PROXY ?
           ALLIANCE_DIP_CONTACT_ALLIANCE : ALLIANCE_DIP_CONTACT_NONE;
}

int alliance_diplomatic_contact_between(int civ_a, int civ_b) {
    int alliance_a = alliance_for_civ(civ_a);
    int alliance_b = alliance_for_civ(civ_b);
    if (alliance_a >= 0 && alliance_b == alliance_a) return 0;
    if (alliance_b >= 0 &&
        alliance_diplomatic_contact_source(civ_a, alliance_b, civ_b) !=
        ALLIANCE_DIP_CONTACT_NONE) return 1;
    if (alliance_a >= 0 &&
        alliance_diplomatic_contact_source(civ_b, alliance_a, civ_a) !=
        ALLIANCE_DIP_CONTACT_NONE) return 1;
    return 0;
}
