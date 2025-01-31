#include "util.h"
#include "finders.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>



uint64_t *loadSavedSeeds(const char *fnam, uint64_t *scnt)
{
    FILE *fp = fopen(fnam, "r");

    uint64_t seed, i;
    uint64_t *baseSeeds;

    if (fp == NULL)
        return NULL;

    *scnt = 0;

    while (!feof(fp))
    {
        if (fscanf(fp, "%" PRId64, (int64_t*)&seed) == 1) (*scnt)++;
        else while (!feof(fp) && fgetc(fp) != '\n');
    }

    if (*scnt == 0)
        return NULL;

    baseSeeds = (uint64_t*) calloc(*scnt, sizeof(*baseSeeds));

    rewind(fp);

    for (i = 0; i < *scnt && !feof(fp);)
    {
        if (fscanf(fp, "%" PRId64, (int64_t*)&baseSeeds[i]) == 1) i++;
        else while (!feof(fp) && fgetc(fp) != '\n');
    }

    fclose(fp);

    return baseSeeds;
}


const char* mc2str(int mc)
{
    switch (mc)
    {
    case MC_B1_7:   return "Beta 1.7"; break;
    case MC_B1_8:   return "Beta 1.8"; break;
    case MC_1_0:    return "1.0"; break;
    case MC_1_1:    return "1.1"; break;
    case MC_1_2:    return "1.2"; break;
    case MC_1_3:    return "1.3"; break;
    case MC_1_4:    return "1.4"; break;
    case MC_1_5:    return "1.5"; break;
    case MC_1_6:    return "1.6"; break;
    case MC_1_7:    return "1.7"; break;
    case MC_1_8:    return "1.8"; break;
    case MC_1_9:    return "1.9"; break;
    case MC_1_10:   return "1.10"; break;
    case MC_1_11:   return "1.11"; break;
    case MC_1_12:   return "1.12"; break;
    case MC_1_13:   return "1.13"; break;
    case MC_1_14:   return "1.14"; break;
    case MC_1_15:   return "1.15"; break;
    case MC_1_16_1: return "1.16.1"; break;
    case MC_1_16:   return "1.16"; break;
    case MC_1_17:   return "1.17"; break;
    case MC_1_18:   return "1.18"; break;
    case MC_1_19_2: return "1.19.2"; break;
    case MC_1_19:   return "1.19"; break;
    case MC_1_20:   return "1.20"; break;
    default:        return NULL;
    }
}

int str2mc(const char *s)
{
    if (!strcmp(s, "1.20"))     return MC_1_20;
    if (!strcmp(s, "1.19"))     return MC_1_19;
    if (!strcmp(s, "1.19.2"))   return MC_1_19_2;
    if (!strcmp(s, "1.18"))     return MC_1_18;
    if (!strcmp(s, "1.17"))     return MC_1_17;
    if (!strcmp(s, "1.16.1"))   return MC_1_16_1;
    if (!strcmp(s, "1.16"))     return MC_1_16;
    if (!strcmp(s, "1.15"))     return MC_1_15;
    if (!strcmp(s, "1.14"))     return MC_1_14;
    if (!strcmp(s, "1.13"))     return MC_1_13;
    if (!strcmp(s, "1.12"))     return MC_1_12;
    if (!strcmp(s, "1.11"))     return MC_1_11;
    if (!strcmp(s, "1.10"))     return MC_1_10;
    if (!strcmp(s, "1.9"))      return MC_1_9;
    if (!strcmp(s, "1.8"))      return MC_1_8;
    if (!strcmp(s, "1.7"))      return MC_1_7;
    if (!strcmp(s, "1.6"))      return MC_1_6;
    if (!strcmp(s, "1.5"))      return MC_1_5;
    if (!strcmp(s, "1.4"))      return MC_1_4;
    if (!strcmp(s, "1.3"))      return MC_1_3;
    if (!strcmp(s, "1.2"))      return MC_1_2;
    if (!strcmp(s, "1.1"))      return MC_1_1;
    if (!strcmp(s, "1.0"))      return MC_1_0;
    if (!strcmp(s, "Beta 1.8")) return MC_B1_8;
    if (!strcmp(s, "Beta 1.7")) return MC_B1_7;
    return -1;
}


const char *biome2str(int mc, int id)
{
    if (mc >= MC_1_18)
    {
        // a bunch of 'new' biomes in 1.18 actually just got renamed
        // (based on their features and biome id conversion when upgrading)
        switch (id)
        {
        case old_growth_birch_forest: return "原始桦木森林";
        case old_growth_pine_taiga: return "原始松木针叶林";
        case old_growth_spruce_taiga: return "原始云杉针叶林";
        case snowy_plains: return "积雪的平原";
        case sparse_jungle: return "稀疏的丛林";
        case stony_shore: return "石岸";
        case windswept_hills: return "风袭丘陵";
        case windswept_forest: return "风袭森林";
        case windswept_gravelly_hills: return "风袭沙砾丘陵";
        case windswept_savanna: return "风袭热带草原";
        case wooded_badlands: return "繁茂的恶地";
        }
    }

    switch (id)
    {
    case ocean: return "海洋";
    case plains: return "平原";
    case desert: return "沙漠";
    case mountains: return "山地";
    case forest: return "森林";
    case taiga: return "针叶林";
    case swamp: return "沼泽";
    case river: return "河流";
    case nether_wastes: return "下界荒地";
    case the_end: return "末路之地";
    // 10
    case frozen_ocean: return "冻洋";
    case frozen_river: return "冻河";
    case snowy_tundra: return "积雪的冻原";
    case snowy_mountains: return "雪山";
    case mushroom_fields: return "蘑菇岛";
    case mushroom_field_shore: return "蘑菇岛岸";
    case beach: return "沙滩";
    case desert_hills: return "沙漠丘陵";
    case wooded_hills: return "繁茂的丘陵";
    case taiga_hills: return "针叶林丘陵";
    // 20
    case mountain_edge: return "山地边缘";
    case jungle: return "丛林";
    case jungle_hills: return "丛林丘陵";
    case jungle_edge: return "丛林边缘";
    case deep_ocean: return "深海";
    case stone_shore: return "石岸";
    case snowy_beach: return "积雪的沙滩";
    case birch_forest: return "桦木森林";
    case birch_forest_hills: return "桦木森林丘陵";
    case dark_forest: return "黑森林";
    // 30
    case snowy_taiga: return "积雪的针叶林";
    case snowy_taiga_hills: return "积雪的针叶林丘陵";
    case giant_tree_taiga: return "巨型针叶林";
    case giant_tree_taiga_hills: return "巨型针叶林丘陵";
    case wooded_mountains: return "繁茂的山地";
    case savanna: return "热带草原";
    case savanna_plateau: return "热带高原";
    case badlands: return "恶地";
    case wooded_badlands_plateau: return "繁茂的恶地高原";
    case badlands_plateau: return "恶地高原";
    // 40  --  1.13
    case small_end_islands: return "末地小型岛屿";
    case end_midlands: return "末地中型岛屿";
    case end_highlands: return "末地高岛";
    case end_barrens: return "末地荒岛";
    case warm_ocean: return "暖水海洋";
    case lukewarm_ocean: return "温水海洋";
    case cold_ocean: return "冷水海洋";
    case deep_warm_ocean: return "暖水深海";
    case deep_lukewarm_ocean: return "温水深海";
    case deep_cold_ocean: return "冷水深海";
    // 50
    case deep_frozen_ocean: return "冰冻深海";
    // Alpha 1.2 - Beta 1.7
    case seasonal_forest: return "季节性森林";
    case shrubland: return "灌木丛地";
    case rainforest: return "雨林";

    case the_void: return "虚空";

    // mutated variants
    case sunflower_plains: return "向日葵平原";
    case desert_lakes: return "沙漠湖泊";
    case gravelly_mountains: return "沙砾山地";
    case flower_forest: return "繁花森林";
    case taiga_mountains: return "针叶林山地";
    case swamp_hills: return "沼泽丘陵";
    case ice_spikes: return "冰刺平原";
    case modified_jungle: return "丛林变种";
    case modified_jungle_edge: return "丛林边缘变种";
    case tall_birch_forest: return "高大桦木森林";
    case tall_birch_hills: return "高大桦木丘陵";
    case dark_forest_hills: return "黑森林丘陵";
    case snowy_taiga_mountains: return "积雪的针叶林山地";
    case giant_spruce_taiga: return "巨型云杉针叶林";
    case giant_spruce_taiga_hills: return "巨型云杉针叶林丘陵";
    case modified_gravelly_mountains: return "沙砾山地+";
    case shattered_savanna: return "破碎的热带草原";
    case shattered_savanna_plateau: return "破碎的热带高原";
    case eroded_badlands: return "被风蚀的恶地";
    case modified_wooded_badlands_plateau: return "繁茂的恶地高原变种";
    case modified_badlands_plateau: return "恶地高原变种";
    // 1.14
    case bamboo_jungle: return "竹林";
    case bamboo_jungle_hills: return "竹林丘陵";
    // 1.16
    case soul_sand_valley: return "灵魂沙峡谷";
    case crimson_forest: return "绯红森林";
    case warped_forest: return "诡异森林";
    case basalt_deltas: return "玄武岩三角洲";
    // 1.17
    case dripstone_caves: return "溶洞";
    case lush_caves: return "繁茂洞穴";
    // 1.18
    case meadow: return "草甸";
    case grove: return "雪林";
    case snowy_slopes: return "积雪的山坡";
    case stony_peaks: return "冰封山峰";
    case jagged_peaks: return "尖峭山峰";
    case frozen_peaks: return "裸岩山峰";
    // 1.19
    case deep_dark: return "深暗之域";
    case mangrove_swamp: return "红树林沼泽";
    // 1.20
    case cherry_grove: return "樱花树林";
    }
    return NULL;
}

void setBiomeColor(unsigned char biomeColor[256][3], int id,
        unsigned char r, unsigned char g, unsigned char b)
{
    biomeColor[id][0] = r;
    biomeColor[id][1] = g;
    biomeColor[id][2] = b;
}

void setMutationColor(unsigned char biomeColor[256][3], int mutated, int parent)
{
    unsigned int c;
    biomeColor[mutated][0] = (c = biomeColor[parent][0] + 40) > 255 ? 255 : c;
    biomeColor[mutated][1] = (c = biomeColor[parent][1] + 40) > 255 ? 255 : c;
    biomeColor[mutated][2] = (c = biomeColor[parent][2] + 40) > 255 ? 255 : c;
}

void initBiomeColors(unsigned char biomeColors[256][3])
{
    // This coloring scheme is largely inspired by the AMIDST program:
    // https://github.com/toolbox4minecraft/amidst
    // https://sourceforge.net/projects/amidst.mirror/

    memset(biomeColors, 0, 256*3);

    setBiomeColor(biomeColors, ocean, 0, 0, 112);
    setBiomeColor(biomeColors, plains, 141, 179, 96);
    setBiomeColor(biomeColors, desert, 250, 148, 24);
    setBiomeColor(biomeColors, mountains, 96, 96, 96);
    setBiomeColor(biomeColors, forest, 5, 102, 33);
    setBiomeColor(biomeColors, taiga, 11, 106, 95);//11, 102, 89);
    setBiomeColor(biomeColors, swamp, 7, 249, 178);
    setBiomeColor(biomeColors, river, 0, 0, 255);
    setBiomeColor(biomeColors, nether_wastes, 87, 37, 38);
    setBiomeColor(biomeColors, the_end, 128, 128, 255);
    setBiomeColor(biomeColors, frozen_ocean, 112, 112, 214);
    setBiomeColor(biomeColors, frozen_river, 160, 160, 255);
    setBiomeColor(biomeColors, snowy_tundra, 255, 255, 255);
    setBiomeColor(biomeColors, snowy_mountains, 160, 160, 160);
    setBiomeColor(biomeColors, mushroom_fields, 255, 0, 255);
    setBiomeColor(biomeColors, mushroom_field_shore, 160, 0, 255);
    setBiomeColor(biomeColors, beach, 250, 222, 85);
    setBiomeColor(biomeColors, desert_hills, 210, 95, 18);
    setBiomeColor(biomeColors, wooded_hills, 34, 85, 28);
    setBiomeColor(biomeColors, taiga_hills, 22, 57, 51);
    setBiomeColor(biomeColors, mountain_edge, 114, 120, 154);
    setBiomeColor(biomeColors, jungle, 80, 123, 10);//83, 123, 9);
    setBiomeColor(biomeColors, jungle_hills, 44, 66, 5);
    setBiomeColor(biomeColors, jungle_edge, 96, 147, 15);//98, 139, 23);
    setBiomeColor(biomeColors, deep_ocean, 0, 0, 48);
    setBiomeColor(biomeColors, stone_shore, 162, 162, 132);
    setBiomeColor(biomeColors, snowy_beach, 250, 240, 192);
    setBiomeColor(biomeColors, birch_forest, 48, 116, 68);
    setBiomeColor(biomeColors, birch_forest_hills, 31, 95, 50);
    setBiomeColor(biomeColors, dark_forest, 64, 81, 26);
    setBiomeColor(biomeColors, snowy_taiga, 49, 85, 74);
    setBiomeColor(biomeColors, snowy_taiga_hills, 36, 63, 54);
    setBiomeColor(biomeColors, giant_tree_taiga, 89, 102, 81);
    setBiomeColor(biomeColors, giant_tree_taiga_hills, 69, 79, 62);
    setBiomeColor(biomeColors, wooded_mountains, 91, 115, 82);//80, 112, 80);
    setBiomeColor(biomeColors, savanna, 189, 178, 95);
    setBiomeColor(biomeColors, savanna_plateau, 167, 157, 100);
    setBiomeColor(biomeColors, badlands, 217, 69, 21);
    setBiomeColor(biomeColors, wooded_badlands_plateau, 176, 151, 101);
    setBiomeColor(biomeColors, badlands_plateau, 202, 140, 101);

    setBiomeColor(biomeColors, small_end_islands, 75, 75, 171);
    setBiomeColor(biomeColors, end_midlands, 201, 201, 89);
    setBiomeColor(biomeColors, end_highlands, 181, 181, 54);
    setBiomeColor(biomeColors, end_barrens, 112, 112, 204);

    setBiomeColor(biomeColors, warm_ocean, 0, 0, 172);
    setBiomeColor(biomeColors, lukewarm_ocean, 0, 0, 144);
    setBiomeColor(biomeColors, cold_ocean, 32, 32, 112);
    setBiomeColor(biomeColors, deep_warm_ocean, 0, 0, 80);
    setBiomeColor(biomeColors, deep_lukewarm_ocean, 0, 0, 64);
    setBiomeColor(biomeColors, deep_cold_ocean, 32, 32, 56);
    setBiomeColor(biomeColors, deep_frozen_ocean, 64, 64, 144);

    setBiomeColor(biomeColors, seasonal_forest, 47, 86, 15);
    setBiomeColor(biomeColors, shrubland, 120, 158, 49);
    setBiomeColor(biomeColors, rainforest, 71, 132, 14);

    setBiomeColor(biomeColors, the_void, 0, 0, 0);

    setMutationColor(biomeColors, sunflower_plains, plains);
    setMutationColor(biomeColors, desert_lakes, desert);
    setMutationColor(biomeColors, gravelly_mountains, mountains);
    setMutationColor(biomeColors, flower_forest, forest);
    setMutationColor(biomeColors, taiga_mountains, taiga);
    setMutationColor(biomeColors, swamp_hills, swamp);
    setBiomeColor(biomeColors, ice_spikes, 180, 220, 220);
    setMutationColor(biomeColors, modified_jungle, jungle);
    setMutationColor(biomeColors, modified_jungle_edge, jungle_edge);
    setMutationColor(biomeColors, tall_birch_forest, birch_forest);
    setMutationColor(biomeColors, tall_birch_hills, birch_forest_hills);
    setMutationColor(biomeColors, dark_forest_hills, dark_forest);
    setMutationColor(biomeColors, snowy_taiga_mountains, snowy_taiga);
    setMutationColor(biomeColors, giant_spruce_taiga, giant_tree_taiga);
    setMutationColor(biomeColors, giant_spruce_taiga_hills, giant_tree_taiga_hills);
    setMutationColor(biomeColors, modified_gravelly_mountains, wooded_mountains);
    setMutationColor(biomeColors, shattered_savanna, savanna);
    setMutationColor(biomeColors, shattered_savanna_plateau, savanna_plateau);
    setMutationColor(biomeColors, eroded_badlands, badlands);
    setMutationColor(biomeColors, modified_wooded_badlands_plateau, wooded_badlands_plateau);
    setMutationColor(biomeColors, modified_badlands_plateau, badlands_plateau);

    setBiomeColor(biomeColors, bamboo_jungle, 132, 149, 0);//118, 142, 20);
    setBiomeColor(biomeColors, bamboo_jungle_hills, 92, 108, 4);//;59, 71, 10);

    setBiomeColor(biomeColors, soul_sand_valley, 77, 58, 46);
    setBiomeColor(biomeColors, crimson_forest, 152, 26, 17);
    setBiomeColor(biomeColors, warped_forest, 73, 144, 123);
    setBiomeColor(biomeColors, basalt_deltas, 100, 95, 99);

    setBiomeColor(biomeColors, dripstone_caves, 78, 48, 18);
    setBiomeColor(biomeColors, lush_caves, 40, 60, 0);

    setBiomeColor(biomeColors, meadow, 96, 164, 69);
    setBiomeColor(biomeColors, grove, 71, 114, 108);
    setBiomeColor(biomeColors, snowy_slopes, 196, 196, 196);
    setBiomeColor(biomeColors, stony_peaks, 123, 143, 116);
    setBiomeColor(biomeColors, jagged_peaks, 220, 220, 200);
    setBiomeColor(biomeColors, frozen_peaks, 176, 179, 206);

    setBiomeColor(biomeColors, deep_dark, 3, 31, 41);
    setBiomeColor(biomeColors, mangrove_swamp, 44, 204, 142);
    setBiomeColor(biomeColors, cherry_grove, 255, 145, 200);
}

void initBiomeTypeColors(unsigned char biomeColors[256][3])
{
    memset(biomeColors, 0, 256*3);

    setBiomeColor(biomeColors, Oceanic,  0x00, 0x00, 0xa0);
    setBiomeColor(biomeColors, Warm,     0xff, 0xc0, 0x00);
    setBiomeColor(biomeColors, Lush,     0x00, 0xa0, 0x00);
    setBiomeColor(biomeColors, Cold,     0x60, 0x60, 0x60);
    setBiomeColor(biomeColors, Freezing, 0xff, 0xff, 0xff);
}


// find the longest biome name contained in 's'
static int _str2id(const char *s)
{
    if (*s == 0)
        return -1;
    const char *f = NULL;
    int ret = -1, id;
    for (id = 0; id < 256; id++)
    {
        const char *p = biome2str(MC_NEWEST, id);
        if (p && (!f || strlen(f) < strlen(p)))
            if (strstr(s, p)) {f = p; ret = id;}

        const char *t = biome2str(MC_1_17, id);
        if (t && t != p && (!f || strlen(f) < strlen(t)))
            if (strstr(s, t)) {f = t; ret = id;}
    }
    return ret;
}

int parseBiomeColors(unsigned char biomeColors[256][3], const char *buf)
{
    const char *p = buf;
    char bstr[64];
    int id, col[4], n, ib, ic;
    n = 0;
    while (*p)
    {
        for (ib = ic = 0; *p && *p != '\n' && *p != ';'; p++)
        {
            if ((size_t)ib+1 < sizeof(bstr))
            {
                if ((*p >= 'a' && *p <= 'z') || *p == '_')
                    bstr[ib++] = *p;
                else if (*p >= 'A' && *p <= 'Z')
                    bstr[ib++] = (*p - 'A') + 'a';
            }
            if (ic < 4 && (*p == '#' || (p[0] == '0' && p[1] == 'x')))
                col[ic++] = strtol(p+1+(*p=='0'), (char**)&p, 16);
            else if (ic < 4 && *p >= '0' && *p <= '9')
                col[ic++] = strtol(p, (char**)&p, 10);
            if (*p == '\n' || *p == ';')
                break;
        }
        while (*p && *p != '\n') p++;
        while (*p == '\n') p++;

        bstr[ib] = 0;
        id = _str2id(bstr);
        if (id >= 0 && id < 256)
        {
            if (ic == 3)
            {
                biomeColors[id][0] = col[0] & 0xff;
                biomeColors[id][1] = col[1] & 0xff;
                biomeColors[id][2] = col[2] & 0xff;
                n++;
            }
            else if (ic == 1)
            {
                biomeColors[id][0] = (col[0] >> 16) & 0xff;
                biomeColors[id][1] = (col[0] >>  8) & 0xff;
                biomeColors[id][2] = (col[0] >>  0) & 0xff;
                n++;
            }
        }
        else if (ic == 4)
        {
            id = col[0] & 0xff;
            biomeColors[id][0] = col[1] & 0xff;
            biomeColors[id][1] = col[2] & 0xff;
            biomeColors[id][2] = col[3] & 0xff;
            n++;
        }
        else if (ic == 2)
        {
            id = col[0] & 0xff;
            biomeColors[id][0] = (col[1] >> 16) & 0xff;
            biomeColors[id][1] = (col[1] >>  8) & 0xff;
            biomeColors[id][2] = (col[1] >>  0) & 0xff;
            n++;
        }
    }
    return n;
}


int biomesToImage(unsigned char *pixels,
        unsigned char biomeColors[256][3], const int *biomes,
        const unsigned int sx, const unsigned int sy,
        const unsigned int pixscale, const int flip)
{
    unsigned int i, j;
    int containsInvalidBiomes = 0;

    for (j = 0; j < sy; j++)
    {
        for (i = 0; i < sx; i++)
        {
            int id = biomes[j*sx+i];
            unsigned int r, g, b;

            if (id < 0 || id >= 256)
            {
                // This may happen for some intermediate layers
                containsInvalidBiomes = 1;
                r = biomeColors[id&0x7f][0]-40; r = (r>0xff) ? 0x00 : r&0xff;
                g = biomeColors[id&0x7f][1]-40; g = (g>0xff) ? 0x00 : g&0xff;
                b = biomeColors[id&0x7f][2]-40; b = (b>0xff) ? 0x00 : b&0xff;
            }
            else
            {
                r = biomeColors[id][0];
                g = biomeColors[id][1];
                b = biomeColors[id][2];
            }

            unsigned int m, n;
            for (m = 0; m < pixscale; m++) {
                for (n = 0; n < pixscale; n++) {
                    int idx = pixscale * i + n;
                    if (flip)
                        idx += (sx * pixscale) * ((pixscale * j) + m);
                    else
                        idx += (sx * pixscale) * ((pixscale * (sy-1-j)) + m);

                    unsigned char *pix = pixels + 3*idx;
                    pix[0] = (unsigned char)r;
                    pix[1] = (unsigned char)g;
                    pix[2] = (unsigned char)b;
                }
            }
        }
    }

    return containsInvalidBiomes;
}

int savePPM(const char *path, const unsigned char *pixels, const unsigned int sx, const unsigned int sy)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    fprintf(fp, "P6\n%d %d\n255\n", sx, sy);
    size_t pixelsLen = 3 * sx * sy;
    size_t written = fwrite(pixels, sizeof pixels[0], pixelsLen, fp);
    fclose(fp);
    return written != pixelsLen;
}


