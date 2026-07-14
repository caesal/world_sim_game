#include "data/plague_names.h"

#include "sim/plague_types.h"

#include <stdio.h>
#include <string.h>

static const PlagueName names[PLAGUE_NAME_COUNT] = {
    {"黑铃疫", "Black Bell Plague"},
    {"灰烛热", "Gray Candle Fever"},
    {"白喉潮", "White Throat Tide"},
    {"赤斑瘟", "Redblotch Pestilence"},
    {"霜肺症", "Frostlung Sickness"},
    {"铁咳疫", "Iron Cough Plague"},
    {"铜皮热", "Copperhide Fever"},
    {"蓝疹瘟", "Blue Rash Pestilence"},
    {"夜汗病", "Night Sweat Malady"},
    {"寒骨疫", "Coldbone Plague"},
    {"乌花瘟", "Blackbloom Pestilence"},
    {"银痂症", "Silver Scab Sickness"},
    {"雾眼病", "Mist-Eye Malady"},
    {"潮骨热", "Tidebone Fever"},
    {"烛泪疫", "Candletear Plague"},
    {"灰鸦瘟", "Greyraven Pestilence"},
    {"瘀星病", "Bruised Star Malady"},
    {"白盐热", "Whitesalt Fever"},
    {"裂唇疫", "Splitlip Plague"},
    {"腐钟瘟", "Rotbell Pestilence"},
    {"黑潮热", "Blacktide Fever"},
    {"金疮疫", "Goldwound Plague"},
    {"绿雾病", "Greenmist Malady"},
    {"枯脉症", "Withervein Sickness"},
    {"月痕瘟", "Moonmark Pestilence"},
    {"砂肺疫", "Sandlung Plague"},
    {"苍斑热", "Pale Blotch Fever"},
    {"断息病", "Broken Breath Malady"},
    {"骨灯疫", "Bonelantern Plague"},
    {"血烬瘟", "Bloodash Pestilence"},
    {"白烬病", "Whiteash Malady"},
    {"苦井热", "Bitterwell Fever"},
    {"黑盐疫", "Blacksalt Plague"},
    {"寒针症", "Coldneedle Sickness"},
    {"焦舌瘟", "Charred Tongue Pestilence"},
    {"银泪热", "Silvertear Fever"},
    {"瘦月疫", "Thinmoon Plague"},
    {"红砂病", "Redsand Malady"},
    {"腐叶瘟", "Rotleaf Pestilence"},
    {"蓝唇症", "Bluelip Sickness"},
    {"夜骨热", "Nightbone Fever"},
    {"雾疮疫", "Mistboil Plague"},
    {"白爪瘟", "Whiteclaw Pestilence"},
    {"赤潮症", "Redtide Sickness"},
    {"锈肺病", "Rustlung Malady"},
    {"黑羽热", "Blackfeather Fever"},
    {"暮斑疫", "Duskblotch Plague"},
    {"寒鳞瘟", "Coldscale Pestilence"},
    {"灰喉症", "Graythroat Sickness"},
    {"盐泪病", "Salttear Malady"},
    {"枯花疫", "Witherbloom Plague"},
    {"银雾瘟", "Silvermist Pestilence"},
    {"裂骨热", "Crackbone Fever"},
    {"乌血症", "Blackblood Sickness"},
    {"霜疮疫", "Frostboil Plague"},
    {"火痂瘟", "Firescab Pestilence"},
    {"青汗病", "Green Sweat Malady"},
    {"白蚀热", "Whiteblight Fever"},
    {"暗潮疫", "Darktide Plague"},
    {"赤眼瘟", "Redeye Pestilence"},
    {"灰鳞症", "Grayscale Sickness"},
    {"铁斑病", "Ironspot Malady"},
    {"月咳疫", "Mooncough Plague"},
    {"寒烬热", "Coldash Fever"},
    {"腐银症", "Tarnished Silver Sickness"},
    {"黑露瘟", "Blackdew Pestilence"},
    {"白脉病", "Whitevein Malady"},
    {"苍灯疫", "Palelantern Plague"},
    {"血潮热", "Bloodtide Fever"},
    {"锈舌症", "Rusttongue Sickness"},
    {"夜瘀病", "Nightbruise Malady"},
    {"枯泉疫", "Withered Spring Plague"},
    {"银喉瘟", "Silverthroat Pestilence"},
    {"红烛热", "Redcandle Fever"},
    {"灰面症", "Grayface Sickness"},
    {"黑星疫", "Blackstar Plague"},
    {"苦风病", "Bitterwind Malady"},
    {"寒雾瘟", "Coldmist Pestilence"},
    {"白骨热", "Whitebone Fever"},
    {"青痕症", "Greenmark Sickness"},
    {"血铃疫", "Bloodbell Plague"},
    {"裂肺瘟", "Splitlung Pestilence"},
    {"银斑病", "Silverspot Malady"},
    {"乌潮热", "Raventide Fever"},
    {"霜泪症", "Frosttear Sickness"},
    {"赤叶疫", "Redleaf Plague"},
    {"黑痂瘟", "Blackscab Pestilence"},
    {"苍咳病", "Palecough Malady"},
    {"盐骨热", "Saltbone Fever"},
    {"月盐疫", "Moonsalt Plague"},
    {"灰井瘟", "Graywell Pestilence"},
    {"铁泪症", "Irontear Sickness"},
    {"白烛病", "Whitecandle Malady"},
    {"暗羽热", "Darkfeather Fever"},
    {"腐潮疫", "Rottide Plague"},
    {"蓝骨瘟", "Bluebone Pestilence"},
    {"寒星病", "Coldstar Malady"},
    {"红雾症", "Redmist Sickness"},
    {"黑脉热", "Blackvein Fever"},
    {"终钟疫", "Lastbell Plague"}
};

typedef struct {
    int value;
    const char *text;
} RomanPart;

static int roman_cycle(int cycle, char *out, size_t out_size) {
    static const RomanPart parts[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
        {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
        {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}
    };
    size_t used = 0;
    size_t i;
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (cycle <= 1) return 1;
    if (cycle > 3999) return snprintf(out, out_size, "%d", cycle) > 0;
    for (i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
        while (cycle >= parts[i].value) {
            size_t length = strlen(parts[i].text);
            if (used + length + 1 > out_size) return 0;
            memcpy(out + used, parts[i].text, length);
            used += length;
            out[used] = '\0';
            cycle -= parts[i].value;
        }
    }
    return 1;
}

int plague_names_count(void) {
    return PLAGUE_NAME_COUNT;
}

const PlagueName *plague_names_get(int name_id) {
    if (name_id < 0 || name_id >= PLAGUE_NAME_COUNT) return NULL;
    return &names[name_id];
}

const char *plague_names_text(int name_id, int language) {
    const PlagueName *name = plague_names_get(name_id);
    if (!name) return "";
    return language == 1 ? name->zh : name->en;
}

int plague_names_format(int name_id, int cycle, int language,
                        char *out, size_t out_size) {
    const char *base = plague_names_text(name_id, language);
    char roman[32];
    int written;
    if (!out || out_size == 0 || !base[0]) return 0;
    if (!roman_cycle(cycle, roman, sizeof(roman))) return 0;
    if (cycle <= 1) written = snprintf(out, out_size, "%s", base);
    else if (language == 1) written = snprintf(out, out_size, "%s%s", base, roman);
    else written = snprintf(out, out_size, "%s %s", base, roman);
    return written >= 0 && (size_t)written < out_size;
}
