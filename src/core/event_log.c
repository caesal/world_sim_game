#include "core/game_state.h"

#include "sim/collapse.h"
#include "sim/simulation.h"
#include "sim/technology.h"

#include <stdio.h>
#include <string.h>
static const char *event_type_label(EventLogType type, int language) {
    int zh = language != 0;
    switch (type) {
        case EVENT_TYPE_EXPANSION_CLAIMED: return zh ? "扩张" : "Expansion";
        case EVENT_TYPE_WAR_STARTED: return zh ? "战争开始" : "War started";
        case EVENT_TYPE_WAR_FRONT_SEVERED: return zh ? "战线断绝" : "Front severed";
        case EVENT_TYPE_BATTLE_RESOLVED: return zh ? "战斗" : "Battle";
        case EVENT_TYPE_TRUCE_SIGNED: return zh ? "停战" : "Truce";
        case EVENT_TYPE_COLLAPSE_SUCCEEDED: return zh ? "崩溃成功" : "Collapse succeeded";
        case EVENT_TYPE_COLLAPSE_FAILED: return zh ? "崩溃失败" : "Collapse failed";
        case EVENT_TYPE_PLAGUE_STARTED: return zh ? "瘟疫开始" : "Plague started";
        case EVENT_TYPE_PLAGUE_SPREAD: return zh ? "瘟疫传播" : "Plague spread";
        case EVENT_TYPE_PLAGUE_ENDED: return zh ? "瘟疫结束" : "Plague ended";
        case EVENT_TYPE_VASSAL_CREATED: return zh ? "附庸建立" : "Vassal created";
        case EVENT_TYPE_VASSAL_RELEASED: return zh ? "附庸独立" : "Vassal released";
        case EVENT_TYPE_VASSAL_TRANSFERRED: return zh ? "附庸转移" : "Vassal transferred";
        case EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE: return zh ? "和平独立" : "Peaceful independence";
        case EVENT_TYPE_VASSAL_INDEPENDENCE_WAR: return zh ? "独立战争" : "Independence war";
        case EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE: return zh ? "附庸解放" : "Vassal freed";
        case EVENT_TYPE_VASSAL_SELF_COLLAPSE_RELEASED: return zh ? "分裂主权" : "Successor sovereignty";
        case EVENT_TYPE_VASSAL_ANNEXED: return zh ? "附庸吞并" : "Vassal annexed";
        case EVENT_TYPE_PERFORMANCE_THROTTLED: return zh ? "性能受限" : "Performance throttled";
        case EVENT_TYPE_PERFORMANCE_SLOW_CALL: return zh ? "慢调用" : "Slow call";
        case EVENT_TYPE_SCHEDULER_YIELD: return zh ? "调度让步" : "Scheduler yield";
        case EVENT_TYPE_WORLD_GENERATION_NOTICE: return zh ? "世界生成" : "World generation";
        case EVENT_TYPE_DISORDER_CHANGED: return zh ? "混乱影响" : "Disorder effect";
        case EVENT_TYPE_DEBUG_NOTICE: return zh ? "系统提示" : "System notice";
        case EVENT_TYPE_DEEP_SEA_ROUTE_CREATED: return zh ? "深海航道" : "Deep sea route";
        case EVENT_TYPE_DEEP_SEA_ROUTE_FAILED: return zh ? "深海航道失败" : "Deep sea route failed";
        case EVENT_TYPE_CIVIL_UNREST_TRIGGERED: return zh ? "内乱触发" : "Civil unrest";
        case EVENT_TYPE_ENCLAVE_JOINED: return zh ? "飞地投靠" : "Enclave joined";
        case EVENT_TYPE_ENCLAVE_INDEPENDENT: return zh ? "飞地独立" : "Enclave independent";
        case EVENT_TYPE_ENCLAVE_FAILED: return zh ? "飞地处理失败" : "Enclave failed";
        case EVENT_TYPE_CIV_CREATED: return zh ? "国家建立" : "Country created";
        case EVENT_TYPE_DIPLOMACY_PEACE: return zh ? "外交和平" : "Diplomatic peace";
        case EVENT_TYPE_DIPLOMACY_TENSE: return zh ? "外交紧张" : "Diplomatic tension";
        case EVENT_TYPE_DIPLOMACY_ALLIANCE: return zh ? "外交同盟" : "Diplomatic alliance";
        case EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED: return zh ? "同盟结束" : "Alliance ended";
        case EVENT_TYPE_WAR_FORCED_ALLIANCE_EXIT: return zh ? "战败退盟" : "Forced alliance exit";
        case EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION: return zh ? "联合完成" : "Union completed";
        case EVENT_TYPE_TREASURY_INDEMNITY: return zh ? "战争赔款" : "War indemnity";
        case EVENT_TYPE_STABILITY_PROJECT: return zh ? "国库维稳" : "Treasury stability";
        case EVENT_TYPE_MERCENARIES_HIRED: return zh ? "雇佣兵" : "Mercenaries";
        case EVENT_TYPE_ALLIANCE_CREATED: return zh ? "联盟建立" : "Alliance created";
        case EVENT_TYPE_ALLIANCE_DISSOLVED: return zh ? "联盟解散" : "Alliance dissolved";
        case EVENT_TYPE_ALLIANCE_MEMBER_JOINED: return zh ? "成员加入" : "Alliance member joined";
        case EVENT_TYPE_ALLIANCE_MEMBER_REMOVED: return zh ? "成员离开" : "Alliance member removed";
        case EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED: return zh ? "军事联盟升级" : "Military Alliance upgrade";
        case EVENT_TYPE_ALLIANCE_MILITARY_DOWNGRADED: return zh ? "军事联盟降级" : "Military Alliance downgrade";
        case EVENT_TYPE_ALLIANCE_WAR_STARTED: return zh ? "军事联盟战争" : "Military Alliance war";
        case EVENT_TYPE_ALLIANCE_WAR_ENDED: return zh ? "军事联盟战果" : "Military Alliance result";
        case EVENT_TYPE_WORLD_TECH_AGE_FIRST: return zh ? "世界科技里程碑" : "World technology milestone";
        case EVENT_TYPE_WORLD_DEEP_SEA_FIRST: return zh ? "世界远洋里程碑" : "World deep-sea milestone";
        default: return zh ? "事件" : "Event";
    }
}
static const char *event_civ_name(int civ_id, const EventCivSnapshot *snapshot, int language) {
    if (snapshot && snapshot->uid > 0) return language ? snapshot->name_zh : snapshot->name_en;
    if (civ_id < 0 || civ_id >= civ_count) return language ? "未知国家" : "Unknown country";
    return civilization_display_name_for_language(civ_id, language);
}
void event_log_alliance_snapshot_name(const EventLogEntry *entry, int language, char *out, size_t out_size) {
    const char *raw = entry ? entry->raw_message : "";
    const char *sep = raw ? strchr(raw, '\t') : NULL;
    if (!out || out_size == 0) return;
    if (sep && language) snprintf(out, out_size, "%s", sep + 1);
    else if (sep) snprintf(out, out_size, "%.*s", (int)(sep - raw), raw);
    else snprintf(out, out_size, "%s", raw && raw[0] ? raw : (language ? "未知同盟" : "Unknown alliance"));
}
static void localized_raw_fallback(const EventLogEntry *entry, int language, char *out, size_t out_size) {
    const char *raw = entry->raw_message;
    if (!raw || !raw[0]) {
        snprintf(out, out_size, "%s", event_type_label(entry->type, language));
        return;
    }
    if (strstr(raw, "VASSAL_RELEASE_FAILED_NO_OVERLORD")) {
        snprintf(out, out_size, "%s", language ? "附庸操作失败：找不到有效宗主。" :
                 "Vassal action failed: no valid overlord.");
        return;
    }
    if (!language) {
        snprintf(out, out_size, "%s", raw);
        return;
    }
    if (strstr(raw, "Coast contour rebuild")) {
        snprintf(out, out_size, "渲染提示：发现海陆边界，但没有生成海岸线轮廓。");
    } else if (strstr(raw, "duplicate or low-contrast neighbor")) {
        snprintf(out, out_size, "调试提示：已调整国家颜色，避免与邻国重复或对比过低。");
    } else if (strstr(raw, "low-confidence fallback color")) {
        snprintf(out, out_size, "调试提示：国家颜色选择使用了备用颜色。");
    } else if (strstr(raw, "Country color check found")) {
        snprintf(out, out_size, "调试提示：发现%d个国家颜色重复或对比过低的问题。", entry->param_a);
    } else if (strstr(raw, "Repaired") && strstr(raw, "country color")) {
        snprintf(out, out_size, "调试提示：已修复%d个国家颜色的可读性。", entry->param_a);
    } else if (strstr(raw, "Collapse blocked: invalid country")) {
        snprintf(out, out_size, "崩溃被阻止：国家无效。");
    } else {
        snprintf(out, out_size, "系统事件：%s", raw);
    }
}
static void localized_collapse_reason(const char *raw, int language, char *out, size_t out_size) {
    if (!raw || !raw[0]) {
        snprintf(out, out_size, "%s", language ? "结构条件不足。" : "structural blocker.");
    } else if (!language) {
        snprintf(out, out_size, "%s", raw);
    } else if (strstr(raw, "no reusable or free country slots") || strstr(raw, "Country limit reached")) {
        snprintf(out, out_size, "没有可复用或空闲的国家槽位。");
    } else if (strstr(raw, "No splittable non-capital region")) {
        snprintf(out, out_size, "没有可分裂的非首都区域。");
    } else if (strstr(raw, "Only capital/core region remains")) {
        snprintf(out, out_size, "只剩首都核心区域。");
    } else if (strstr(raw, "No valid capital region")) {
        snprintf(out, out_size, "没有有效首都区域。");
    } else if (strstr(raw, "City limit reached")) {
        snprintf(out, out_size, "城市数量达到上限。");
    } else if (strstr(raw, "Country is invalid") || strstr(raw, "invalid country")) {
        snprintf(out, out_size, "国家无效或已经灭亡。");
    } else {
        snprintf(out, out_size, "结构条件不足。");
    }
}
static void event_log_message(const EventLogEntry *entry, int language, char *out, size_t out_size) {
    int zh = language != 0;
    const char *civ = event_civ_name(entry->civ_id, &entry->civ_snapshot, language);
    const char *target = event_civ_name(entry->target_id, &entry->target_snapshot, language);
    const char *third = event_civ_name(entry->param_a, &entry->param_a_snapshot, language);

    if (!out || out_size == 0 || !entry) return;
    switch (entry->type) {
        case EVENT_TYPE_EXPANSION_CLAIMED:
            if (entry->civ_uid <= 0) break;
            snprintf(out, out_size, zh ? "%s占领了相邻区域。" : "%s claimed a neighboring region.", civ);
            return;
        case EVENT_TYPE_WAR_STARTED:
            if (entry->civ_uid <= 0 || entry->target_uid <= 0) break;
            snprintf(out, out_size, zh ? "%s与%s开战。" : "%s went to war with %s.", civ, target);
            return;
        case EVENT_TYPE_WAR_FRONT_SEVERED:
            snprintf(out, out_size, zh ? "%s与%s因所有战线断绝而结束战争。" :
                     "War between %s and %s ended because all active fronts were severed.", civ, target);
            return;
        case EVENT_TYPE_TRUCE_SIGNED:
            snprintf(out, out_size, zh ? "%s与%s签署停战。" : "%s signed a truce with %s.", civ, target);
            return;
        case EVENT_TYPE_VASSAL_CREATED:
            snprintf(out, out_size, zh ? "%s成为%s的附庸。" : "%s became vassal of %s.", civ, target);
            return;
        case EVENT_TYPE_VASSAL_TRANSFERRED:
            snprintf(out, out_size, zh ? "%s从%s转为%s的附庸。" : "%s transferred from %s to %s.", civ, target, third);
            return;
        case EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE:
            snprintf(out, out_size, zh ? "%s从%s和平独立。" : "%s peacefully became independent from %s.", civ, target);
            return;
        case EVENT_TYPE_VASSAL_INDEPENDENCE_WAR:
            snprintf(out, out_size, zh ? "%s向%s发起独立战争。" : "%s declared an independence war against %s.", civ, target);
            return;
        case EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE:
            snprintf(out, out_size, zh ? "因主国%s崩溃，%s恢复独立。" : "%s became independent because overlord %s collapsed.",
                     target, civ);
            return;
        case EVENT_TYPE_VASSAL_SELF_COLLAPSE_RELEASED:
            snprintf(out, out_size, zh ? "%s崩溃，其分裂国家恢复完全主权。" :
                                        "%s collapsed; successor states became sovereign.", civ);
            return;
        case EVENT_TYPE_VASSAL_ANNEXED:
            snprintf(out, out_size, zh ? "%s吞并附庸%s。" : "%s annexed vassal %s.", target, civ);
            return;
        case EVENT_TYPE_COLLAPSE_SUCCEEDED:
            if (entry->civ_uid <= 0) break;
            if (entry->param_a == -(int)COLLAPSE_SINGLE_ANNEX_OVERLORD) {
                snprintf(out, out_size, zh ? "%s政权崩溃，被宗主国%s接管。" :
                                            "%s collapsed and was absorbed by its overlord %s.", civ, target);
                return;
            }
            if (entry->param_a == -(int)COLLAPSE_SINGLE_ANNEX_WAR || entry->param_a == -(int)COLLAPSE_SINGLE_ANNEX_NEIGHBOR) {
                snprintf(out, out_size, zh ? "%s政权崩溃，被邻国%s吞并。" :
                                            "%s collapsed and was annexed by %s.", civ, target);
                return;
            }
            if (entry->param_a == -(int)COLLAPSE_SINGLE_UNCLAIMED) {
                snprintf(out, out_size, zh ? "%s政权崩溃，因孤立而消失。" :
                                            "%s collapsed in isolation and disappeared.", civ);
                return;
            }
            snprintf(out, out_size, zh ? "%s崩溃并形成%d个继承国家。" :
                                        "%s collapsed and formed %d successor states.", civ, entry->param_a);
            return;
        case EVENT_TYPE_COLLAPSE_FAILED:
            if (entry->civ_uid <= 0) break;
            {
                char reason[160];
                localized_collapse_reason(entry->raw_message, language, reason, sizeof(reason));
                snprintf(out, out_size, zh ? "%s崩溃失败：%s" : "%s collapse failed: %s", civ, reason);
            }
            return;
        case EVENT_TYPE_CIVIL_UNREST_TRIGGERED:
            snprintf(out, out_size, zh ? "%s触发内乱，混乱度升至100。" :
                                        "%s triggered civil unrest and disorder reached 100.", civ);
            return;
        case EVENT_TYPE_PLAGUE_STARTED:
            if (entry->civ_uid <= 0) break;
            if (entry->city_id >= 0 && entry->city_id < city_count) {
                if (zh) snprintf(out, out_size, "%s的%s爆发瘟疫。", civ, cities[entry->city_id].name);
                else snprintf(out, out_size, "Plague started in %s of %s.", cities[entry->city_id].name, civ);
            } else {
                snprintf(out, out_size, zh ? "%s爆发瘟疫。" : "Plague started in %s.", civ);
            }
            return;
        case EVENT_TYPE_PLAGUE_ENDED:
            snprintf(out, out_size, "%s", zh ? "全球活跃瘟疫已经结束。" :
                                                "The global active plague ended.");
            return;
        case EVENT_TYPE_DEEP_SEA_ROUTE_CREATED:
            if (entry->civ_uid <= 0) break;
            snprintf(out, out_size, zh ? "%s建立了深海航道。" : "%s established a deep sea route.", civ);
            return;
        case EVENT_TYPE_DEEP_SEA_ROUTE_FAILED:
            if (entry->civ_uid <= 0) break;
            snprintf(out, out_size, zh ? "%s深海航行失败。" : "%s failed a deep sea voyage.", civ);
            return;
        case EVENT_TYPE_ENCLAVE_JOINED:
            snprintf(out, out_size, zh ? "%s的飞地断联%d年，投靠%s。" :
                     "An enclave of %s lost capital contact for %d years and joined %s.",
                     civ, entry->param_a / 12, target);
            return;
        case EVENT_TYPE_ENCLAVE_INDEPENDENT:
            snprintf(out, out_size, zh ? "%s的飞地断联%d年，无可接触强国，宣布独立为%s。" :
                     "An enclave of %s lost capital contact for %d years and declared independence as %s.",
                     target, entry->param_a / 12, civ);
            return;
        case EVENT_TYPE_ENCLAVE_FAILED:
            snprintf(out, out_size, zh ? "%s飞地脱离失败：%s" : "%s enclave separation failed: %s", civ,
                     entry->raw_message[0] ? entry->raw_message : (zh ? "结构条件不足。" : "structural blocker."));
            return;
        case EVENT_TYPE_CIV_CREATED:
            if (entry->civ_uid <= 0) break;
            snprintf(out, out_size, zh ? "%s建立。" : "%s was founded.", civ);
            return;
        case EVENT_TYPE_DIPLOMACY_PEACE:
            snprintf(out, out_size, zh ? "%s与%s进入和平外交。" : "%s and %s entered peaceful relations.", civ, target);
            return;
        case EVENT_TYPE_DIPLOMACY_TENSE:
            snprintf(out, out_size, zh ? "%s与%s关系转为紧张。" : "%s and %s relations became tense.", civ, target);
            return;
        case EVENT_TYPE_DIPLOMACY_ALLIANCE:
            snprintf(out, out_size, zh ? "%s与%s结为同盟。" : "%s and %s formed an alliance.", civ, target);
            return;
        case EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED:
            snprintf(out, out_size, zh ? "%s与%s结束同盟。" : "%s and %s ended their alliance.", civ, target);
            return;
        case EVENT_TYPE_WAR_FORCED_ALLIANCE_EXIT: {
            char alliance[EVENT_LOG_LEN]; event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size, zh ? "%s在与%s的战争中失利，被迫退出%s。" :
                     "%s lost the war against %s and was forced out of %s.", civ, target, alliance);
            return;
        }
        case EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION: {
            char alliance[EVENT_LOG_LEN]; event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size, zh ? "%s吞并其他成员国，完成%s联合。" :
                     "%s absorbed the other members and completed union in %s.", civ, alliance);
            return;
        }
        case EVENT_TYPE_ALLIANCE_CREATED: {
            char alliance[EVENT_LOG_LEN];
            event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size, zh ? "%s与%s建立了%s。" : "%s and %s founded %s.",
                     civ, target, alliance);
            return;
        }
        case EVENT_TYPE_ALLIANCE_DISSOLVED: {
            char alliance[EVENT_LOG_LEN];
            event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size, zh ? "%s解散。" : "%s dissolved.", alliance);
            return;
        }
        case EVENT_TYPE_ALLIANCE_MEMBER_JOINED: {
            char alliance[EVENT_LOG_LEN];
            event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size, zh ? "%s加入%s。" : "%s joined %s.", target, alliance);
            return;
        }
        case EVENT_TYPE_ALLIANCE_MEMBER_REMOVED: {
            char alliance[EVENT_LOG_LEN];
            event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size, zh ? "%s离开%s。" : "%s left %s.", civ, alliance);
            return;
        }
        case EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED:
        case EVENT_TYPE_ALLIANCE_MILITARY_DOWNGRADED: {
            char alliance[EVENT_LOG_LEN];
            int upgraded = entry->type == EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED;
            event_log_alliance_snapshot_name(entry, language, alliance, sizeof(alliance));
            snprintf(out, out_size,
                     upgraded ? (zh ? "%s升级为军事联盟。" : "%s upgraded to a Military Alliance.") :
                                (zh ? "%s降级为防御联盟。" : "%s downgraded to a Defensive Alliance."),
                     alliance);
            return;
        }
        case EVENT_TYPE_ALLIANCE_WAR_STARTED:
            snprintf(out, out_size, zh ? "%s与%s之间爆发军事联盟战争。" :
                                         "A Military Alliance war began between %s and %s.",
                     civ, target);
            return;
        case EVENT_TYPE_ALLIANCE_WAR_ENDED:
            snprintf(out, out_size, zh ? "%s与%s之间的军事联盟战争结束。" :
                                         "The Military Alliance war between %s and %s ended.",
                     civ, target);
            return;
        case EVENT_TYPE_WORLD_TECH_AGE_FIRST:
            snprintf(out, out_size, zh ? "%s成为首个进入%s（%d）的国家。" :
                                         "%s became the first country to enter %s (%d).",
                     civ, technology_stage_name(entry->param_a, language), entry->param_a);
            return;
        case EVENT_TYPE_WORLD_DEEP_SEA_FIRST:
            snprintf(out, out_size, zh ? "%s成为首个解锁远洋航行的国家。" :
                                         "%s became the first country to unlock deep-sea navigation.", civ);
            return;
        case EVENT_TYPE_TREASURY_INDEMNITY:
            snprintf(out, out_size,
                     zh ? "%s向%s支付赔款，抵消%d个割地；实际割让%d个，花费%d。" :
                          "%s paid war indemnity to %s, offsetting %d cessions; %d provinces transferred, cost %d.",
                     civ, target, entry->param_a, entry->param_b, max(0, entry->region_id));
            return;
        case EVENT_TYPE_STABILITY_PROJECT:
            snprintf(out, out_size,
                     zh ? "%s动用国库维稳，花费%d，临时降低有效混乱%d点。" :
                          "%s funded a stability project, spending %d for a temporary %d-point effective disorder offset.",
                     civ, entry->param_a, entry->param_b);
            return;
        case EVENT_TYPE_MERCENARIES_HIRED:
            snprintf(out, out_size,
                     zh ? "%s在对%s的战争中雇佣%d名临时士兵，花费%d。" :
                          "%s hired temporary soldiers against %s: %d soldiers, cost %d.",
                     civ, target, entry->param_a, entry->param_b);
            return;
        case EVENT_TYPE_DISORDER_CHANGED:
            snprintf(out, out_size, zh ? "%s混乱影响调整为%d%%，当前混乱%d。" :
                     "%s soft disorder effect is now %d%% at disorder %d.", civ, entry->param_a, entry->param_b);
            return;
        case EVENT_TYPE_PERFORMANCE_SLOW_CALL:
            snprintf(out, out_size, zh ? "慢调用：%s，耗时%d ms，扫描 tiles %d。" :
                     "Slow call: %s took %d ms, scanned %d tiles.",
                     entry->raw_message[0] ? entry->raw_message : "unknown", entry->param_a, entry->param_b);
            return;
        case EVENT_TYPE_SCHEDULER_YIELD:
            snprintf(out, out_size, zh ? "模拟步骤超预算：%d ms，预算 %d ms，重复 %d 次。" :
                     "Simulation step over budget: %d ms, budget %d ms, repeated %d times.",
                     entry->param_a, entry->param_b, max(1, entry->region_id));
            return;
        case EVENT_TYPE_WORLD_GENERATION_NOTICE:
            if (entry->region_id == 1) {
                snprintf(out, out_size, zh ? "初始文明请求%d个，超过容量，已限制为%d个。" :
                         "Initial civilization request %d exceeded capacity and was capped at %d.",
                         entry->param_a, entry->param_b);
                return;
            }
            if (entry->region_id == 2) {
                snprintf(out, out_size, zh ? "已放置%d/%d个请求文明；其余地点不可用。" :
                         "Placed %d/%d requested civilizations; remaining sites were not viable.",
                         entry->param_a, entry->param_b);
                return;
            }
            localized_raw_fallback(entry, language, out, out_size);
            return;
        case EVENT_TYPE_DEBUG_NOTICE:
        case EVENT_TYPE_PERFORMANCE_THROTTLED:
        default:
            localized_raw_fallback(entry, language, out, out_size);
            return;
    }
    localized_raw_fallback(entry, language, out, out_size);
}
void event_log_format_entry_data(const EventLogEntry *entry, int language, char *out, size_t out_size) {
    char message[EVENT_LOG_LEN * 2];
    if (!out || out_size == 0 || !entry) return;
    event_log_message(entry, language, message, sizeof(message));
    if (language != 0) {
        snprintf(out, out_size, "[%d年%d月] %s\n%s", entry->year, entry->month,
                 event_type_label(entry->type, language), message);
    } else {
        snprintf(out, out_size, "[Year %d Month %d] %s\n%s", entry->year, entry->month,
                 event_type_label(entry->type, language), message);
    }
    if (entry->repeat_count > 1) {
        char repeat[24];
        snprintf(repeat, sizeof(repeat), " x%d", entry->repeat_count);
        strncat(out, repeat, out_size - strlen(out) - 1);
    }
}
void event_log_format_entry(int index, int language, char *out, size_t out_size) {
    EventLogEntry entry;
    if (!out || out_size == 0) return;
    if (!event_log_get_entry(index, &entry)) {
        out[0] = '\0';
        return;
    }
    event_log_format_entry_data(&entry, language, out, out_size);
}
const char *event_log_get(int index) {
    static char formatted[EVENT_LOG_LEN * 3];
    char *newline;
    event_log_format_entry(index, ui_language, formatted, sizeof(formatted));
    newline = strchr(formatted, '\n');
    if (newline) *newline = ' ';
    return formatted;
}
