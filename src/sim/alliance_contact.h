#ifndef WORLD_SIM_ALLIANCE_CONTACT_H
#define WORLD_SIM_ALLIANCE_CONTACT_H

typedef enum {
    ALLIANCE_DIP_CONTACT_NONE = 0,
    ALLIANCE_DIP_CONTACT_DIRECT,
    ALLIANCE_DIP_CONTACT_ALLIANCE,
    ALLIANCE_DIP_CONTACT_VASSAL_PROXY
} AllianceDiplomaticContactSource;

AllianceDiplomaticContactSource alliance_diplomatic_contact_source(int civ_id,
                                                                   int alliance_id,
                                                                   int member_civ_id);
int alliance_diplomatic_contact_between(int civ_a, int civ_b);

#endif
