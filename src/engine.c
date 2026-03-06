#include "engine.h"

#include <string.h>
#include <pthread.h>

#include "../finders.h"
#include "../generator.h"
#include "../biomes.h"

/* ── version string → MCVersion ─────────────────────────────────────────── */

static const struct { const char *str; int mc; } g_versions[] = {
    { "1.0",    MC_1_0    }, { "1.1",    MC_1_1    },
    { "1.2",    MC_1_2    }, { "1.3",    MC_1_3    },
    { "1.4",    MC_1_4    }, { "1.5",    MC_1_5    },
    { "1.6",    MC_1_6    }, { "1.7",    MC_1_7    },
    { "1.8",    MC_1_8    }, { "1.9",    MC_1_9    },
    { "1.10",   MC_1_10   }, { "1.11",   MC_1_11   },
    { "1.12",   MC_1_12   }, { "1.13",   MC_1_13   },
    { "1.14",   MC_1_14   }, { "1.15",   MC_1_15   },
    { "1.16.1", MC_1_16_1 }, { "1.16.5", MC_1_16_5 }, { "1.16", MC_1_16 },
    { "1.17",   MC_1_17   }, { "1.17.1", MC_1_17_1 },
    { "1.18",   MC_1_18   }, { "1.18.2", MC_1_18_2 },
    { "1.19",   MC_1_19   }, { "1.19.2", MC_1_19_2 }, { "1.19.4", MC_1_19_4 },
    { "1.20",   MC_1_20   }, { "1.20.6", MC_1_20_6 },
    { "1.21",   MC_1_21   },
    { NULL, 0 }
};

int parse_mc_version(const char *str)
{
    for (int i = 0; g_versions[i].str; i++)
        if (strcmp(str, g_versions[i].str) == 0)
            return g_versions[i].mc;
    return MC_UNDEF;
}

/* ── structure-type name → StructureType ────────────────────────────────── */

static const struct { const char *name; int type; } g_struct_names[] = {
    { "feature",        Feature        },
    { "desert_pyramid", Desert_Pyramid },
    { "jungle_temple",  Jungle_Temple  },
    { "swamp_hut",      Swamp_Hut      },
    { "igloo",          Igloo          },
    { "village",        Village        },
    { "ocean_ruin",     Ocean_Ruin     },
    { "shipwreck",      Shipwreck      },
    { "monument",       Monument       },
    { "mansion",        Mansion        },
    { "outpost",        Outpost        },
    { "ruined_portal",  Ruined_Portal  },
    { "ancient_city",   Ancient_City   },
    { "treasure",       Treasure       },
    { "fortress",       Fortress       },
    { "bastion",        Bastion        },
    { "end_city",       End_City       },
    { "trail_ruins",    Trail_Ruins    },
    { "trial_chambers", Trial_Chambers },
    { NULL, -1 }
};

int parse_structure_type(const char *name)
{
    for (int i = 0; g_struct_names[i].name; i++)
        if (strcmp(name, g_struct_names[i].name) == 0)
            return g_struct_names[i].type;
    return -1;
}

const char * const *get_structure_names(void)
{
    /* Build a static NULL-terminated array of name pointers once. */
    #define MAX_STRUCTURE_TYPES 32
    static const char *names[MAX_STRUCTURE_TYPES];
    static int initialized = 0;
    if (!initialized) {
        int n = 0;
        for (int i = 0; g_struct_names[i].name && n < MAX_STRUCTURE_TYPES - 1; i++)
            names[n++] = g_struct_names[i].name;
        names[n] = NULL;
        initialized = 1;
    }
    return (const char * const *)names;
}

/* ── biome-name → BiomeID ────────────────────────────────────────────────── */

static const struct { const char *name; int id; } g_biome_names[] = {
    { "ocean",                        0   },
    { "plains",                       1   },
    { "desert",                       2   },
    { "mountains",                    3   },
    { "forest",                       4   },
    { "taiga",                        5   },
    { "swamp",                        6   },
    { "river",                        7   },
    { "nether_wastes",                8   },
    { "the_end",                      9   },
    { "frozen_ocean",                 10  },
    { "frozen_river",                 11  },
    { "snowy_tundra",                 12  },
    { "snowy_mountains",              13  },
    { "mushroom_fields",              14  },
    { "mushroom_field_shore",         15  },
    { "beach",                        16  },
    { "desert_hills",                 17  },
    { "wooded_hills",                 18  },
    { "taiga_hills",                  19  },
    { "mountain_edge",                20  },
    { "jungle",                       21  },
    { "jungle_hills",                 22  },
    { "jungle_edge",                  23  },
    { "deep_ocean",                   24  },
    { "stone_shore",                  25  },
    { "snowy_beach",                  26  },
    { "birch_forest",                 27  },
    { "birch_forest_hills",           28  },
    { "dark_forest",                  29  },
    { "snowy_taiga",                  30  },
    { "snowy_taiga_hills",            31  },
    { "giant_tree_taiga",             32  },
    { "giant_tree_taiga_hills",       33  },
    { "wooded_mountains",             34  },
    { "savanna",                      35  },
    { "savanna_plateau",              36  },
    { "badlands",                     37  },
    { "wooded_badlands_plateau",      38  },
    { "badlands_plateau",             39  },
    { "small_end_islands",            40  },
    { "end_midlands",                 41  },
    { "end_highlands",                42  },
    { "end_barrens",                  43  },
    { "warm_ocean",                   44  },
    { "lukewarm_ocean",               45  },
    { "cold_ocean",                   46  },
    { "deep_warm_ocean",              47  },
    { "deep_lukewarm_ocean",          48  },
    { "deep_cold_ocean",              49  },
    { "deep_frozen_ocean",            50  },
    { "the_void",                     127 },
    { "sunflower_plains",             129 },
    { "desert_lakes",                 130 },
    { "gravelly_mountains",           131 },
    { "flower_forest",                132 },
    { "taiga_mountains",              133 },
    { "swamp_hills",                  134 },
    { "ice_spikes",                   140 },
    { "modified_jungle",              149 },
    { "modified_jungle_edge",         151 },
    { "tall_birch_forest",            155 },
    { "tall_birch_hills",             156 },
    { "dark_forest_hills",            157 },
    { "snowy_taiga_mountains",        158 },
    { "giant_spruce_taiga",           160 },
    { "giant_spruce_taiga_hills",     161 },
    { "modified_gravelly_mountains",  162 },
    { "shattered_savanna",            163 },
    { "shattered_savanna_plateau",    164 },
    { "eroded_badlands",              165 },
    { "modified_wooded_badlands_plateau", 166 },
    { "modified_badlands_plateau",    167 },
    { "bamboo_jungle",                168 },
    { "bamboo_jungle_hills",          169 },
    { "soul_sand_valley",             170 },
    { "crimson_forest",               171 },
    { "warped_forest",                172 },
    { "basalt_deltas",                173 },
    { "dripstone_caves",              174 },
    { "lush_caves",                   175 },
    { "meadow",                       177 },
    { "grove",                        178 },
    { "snowy_slopes",                 179 },
    { "jagged_peaks",                 180 },
    { "frozen_peaks",                 181 },
    { "stony_peaks",                  182 },
    { "deep_dark",                    183 },
    { "mangrove_swamp",               184 },
    { "cherry_grove",                 185 },
    { "pale_garden",                  186 },
    { NULL, -1 }
};

int parse_biome_name(const char *name)
{
    for (int i = 0; g_biome_names[i].name; i++)
        if (strcmp(name, g_biome_names[i].name) == 0)
            return g_biome_names[i].id;
    return -1;
}

#define MAX_BIOME_TYPES 128  /* must be > number of entries in g_biome_names */
const char * const *get_biome_names(void)
{
    static const char *names[MAX_BIOME_TYPES];
    static int initialized = 0;
    if (!initialized) {
        int n = 0;
        for (int i = 0; g_biome_names[i].name && n < MAX_BIOME_TYPES - 1; i++)
            names[n++] = g_biome_names[i].name;
        names[n] = NULL;
        initialized = 1;
    }
    return (const char * const *)names;
}

/* How often (in seeds scanned) each thread re-checks the shared done flag */
#define RESULT_CHECK_INTERVAL 0x1000  /* every 4096 seeds */

/* ── biome check helper ──────────────────────────────────────────────────── */

/* Returns 1 if the structure position satisfies the optional biome filter,
 * 0 otherwise.  When sq->biome < 0 (no filter) this always returns 1.
 * The generator dimension is restored to DIM_OVERWORLD on return. */
static int check_biome_filter(Generator *g, const StructureQuery *sq,
                               int mc_version, int64_t seed, Pos pos)
{
    if (sq->biome < 0)
        return 1;
    StructureConfig sconf;
    getStructureConfig(sq->type, mc_version, &sconf);
    applySeed(g, sconf.dim, (uint64_t)seed);
    int biome_at = getBiomeAt(g, 4, pos.x >> 2, 15, pos.z >> 2);
    applySeed(g, DIM_OVERWORLD, (uint64_t)seed);
    return biome_at == sq->biome;
}

/* ── per-thread work ─────────────────────────────────────────────────────── */

typedef struct {
    const SearchRequest *req;
    int64_t              seed_start;
    int64_t              seed_end;
    SearchResult        *result;
    pthread_mutex_t     *mutex;
} ThreadArg;

static void *thread_worker(void *arg)
{
    ThreadArg          *targ = (ThreadArg *)arg;
    const SearchRequest *req  = targ->req;

    /* One Generator per thread, allocated on the stack – no heap needed */
    Generator g;
    setupGenerator(&g, req->mc_version, 0);

    int64_t local_scanned = 0;

    for (int64_t seed = targ->seed_start; seed <= targ->seed_end; seed++) {

        /* Periodically check whether the global result is already full */
        if ((local_scanned & (RESULT_CHECK_INTERVAL - 1)) == 0) {
            pthread_mutex_lock(targ->mutex);
            int done = targ->result->count >= req->max_results;
            pthread_mutex_unlock(targ->mutex);
            if (done)
                break;
        }

        local_scanned++;

        applySeed(&g, DIM_OVERWORLD, (uint64_t)seed);

        int valid = 1;

        for (int s = 0; s < req->num_structures && valid; s++) {
            const StructureQuery *sq = &req->structures[s];

            StructureConfig sconf;
            if (!getStructureConfig(sq->type, req->mc_version, &sconf)) {
                valid = 0;
                break;
            }

            /* How many regions to scan in each direction */
            int region_blocks = (int)sconf.regionSize * 16;
            int max_reg = (sq->max_distance / region_blocks) + 2;

            int found = 0;
            for (int rx = -max_reg; rx <= max_reg && !found; rx++) {
                for (int rz = -max_reg; rz <= max_reg && !found; rz++) {
                    Pos pos;
                    if (!getStructurePos(sq->type, req->mc_version,
                                        (uint64_t)seed, rx, rz, &pos))
                        continue;

                    /* Distance check (squared to avoid sqrt) */
                    int64_t dx = pos.x, dz = pos.z;
                    int64_t d2 = dx*dx + dz*dz;
                    int64_t md = sq->max_distance;
                    if (d2 > md * md)
                        continue;

                    /* Biome viability check */
                    if (!isViableStructurePos(sq->type, &g,
                                             pos.x, pos.z, 0))
                        continue;

                    /* Optional biome filter */
                    if (!check_biome_filter(&g, sq, req->mc_version,
                                           seed, pos))
                        continue;

                    found = 1;
                }
            }

            if (!found)
                valid = 0;
        }

        if (valid) {
            pthread_mutex_lock(targ->mutex);
            int done = 0;
            if (targ->result->count < req->max_results)
                targ->result->seeds[targ->result->count++] = seed;
            done = targ->result->count >= req->max_results;
            pthread_mutex_unlock(targ->mutex);
            if (done)
                break;
        }
    }

    pthread_mutex_lock(targ->mutex);
    targ->result->scanned += local_scanned;
    pthread_mutex_unlock(targ->mutex);

    return NULL;
}

/* ── streaming per-thread work ───────────────────────────────────────────── */

typedef struct {
    const SearchRequest *req;
    int64_t              seed_start;
    int64_t              seed_end;
    seed_found_cb        on_seed;
    void                *cb_userdata;
    int                 *found_total;   /* shared count of found seeds      */
    int64_t             *scanned_total; /* shared count of scanned seeds    */
    pthread_mutex_t     *mutex;
} StreamThreadArg;

static void *stream_thread_worker(void *arg)
{
    StreamThreadArg     *targ = (StreamThreadArg *)arg;
    const SearchRequest *req  = targ->req;

    Generator g;
    setupGenerator(&g, req->mc_version, 0);

    int64_t local_scanned = 0;

    for (int64_t seed = targ->seed_start; seed <= targ->seed_end; seed++) {

        if ((local_scanned & (RESULT_CHECK_INTERVAL - 1)) == 0) {
            pthread_mutex_lock(targ->mutex);
            int done = *targ->found_total >= req->max_results;
            pthread_mutex_unlock(targ->mutex);
            if (done)
                break;
        }

        local_scanned++;

        applySeed(&g, DIM_OVERWORLD, (uint64_t)seed);

        int valid = 1;

        for (int s = 0; s < req->num_structures && valid; s++) {
            const StructureQuery *sq = &req->structures[s];

            StructureConfig sconf;
            if (!getStructureConfig(sq->type, req->mc_version, &sconf)) {
                valid = 0;
                break;
            }

            int region_blocks = (int)sconf.regionSize * 16;
            int max_reg = (sq->max_distance / region_blocks) + 2;

            int found = 0;
            for (int rx = -max_reg; rx <= max_reg && !found; rx++) {
                for (int rz = -max_reg; rz <= max_reg && !found; rz++) {
                    Pos pos;
                    if (!getStructurePos(sq->type, req->mc_version,
                                        (uint64_t)seed, rx, rz, &pos))
                        continue;

                    int64_t dx = pos.x, dz = pos.z;
                    int64_t md = sq->max_distance;
                    if (dx*dx + dz*dz > md * md)
                        continue;

                    if (!isViableStructurePos(sq->type, &g, pos.x, pos.z, 0))
                        continue;

                    /* Optional biome filter */
                    if (!check_biome_filter(&g, sq, req->mc_version,
                                           seed, pos))
                        continue;

                    found = 1;
                }
            }

            if (!found)
                valid = 0;
        }

        if (valid) {
            pthread_mutex_lock(targ->mutex);
            int done = 0;
            if (*targ->found_total < req->max_results) {
                /* on_seed is called while holding the mutex so callers need
                 * not worry about concurrent invocations. */
                targ->on_seed(seed, targ->cb_userdata);
                (*targ->found_total)++;
            }
            done = *targ->found_total >= req->max_results;
            pthread_mutex_unlock(targ->mutex);
            if (done)
                break;
        }
    }

    pthread_mutex_lock(targ->mutex);
    *targ->scanned_total += local_scanned;
    pthread_mutex_unlock(targ->mutex);

    return NULL;
}

/* ── public entry point ──────────────────────────────────────────────────── */

void search_seeds(const SearchRequest *req, SearchResult *result)
{
    int64_t total = req->seed_end - req->seed_start + 1;
    if (total <= 0)
        return;

    /* Clamp thread count */
    int nthreads = MAX_THREADS;
    if (total < nthreads)
        nthreads = (int)total;

    int64_t chunk = total / nthreads;

    pthread_t     threads[MAX_THREADS];
    ThreadArg     args[MAX_THREADS];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < nthreads; i++) {
        args[i].req        = req;
        args[i].seed_start = req->seed_start + (int64_t)i * chunk;
        args[i].seed_end   = (i == nthreads - 1)
                                 ? req->seed_end
                                 : args[i].seed_start + chunk - 1;
        args[i].result     = result;
        args[i].mutex      = &mutex;
        pthread_create(&threads[i], NULL, thread_worker, &args[i]);
    }

    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&mutex);
}

void search_seeds_stream(const SearchRequest *req,
                         seed_found_cb on_seed, void *userdata,
                         int64_t *scanned_out)
{
    int64_t total = req->seed_end - req->seed_start + 1;
    if (total <= 0) {
        if (scanned_out) *scanned_out = 0;
        return;
    }

    int nthreads = MAX_THREADS;
    if (total < nthreads)
        nthreads = (int)total;

    int64_t chunk = total / nthreads;

    pthread_t       threads[MAX_THREADS];
    StreamThreadArg args[MAX_THREADS];
    pthread_mutex_t mutex         = PTHREAD_MUTEX_INITIALIZER;
    int             found_total   = 0;
    int64_t         scanned_total = 0;

    for (int i = 0; i < nthreads; i++) {
        args[i].req           = req;
        args[i].seed_start    = req->seed_start + (int64_t)i * chunk;
        args[i].seed_end      = (i == nthreads - 1)
                                    ? req->seed_end
                                    : args[i].seed_start + chunk - 1;
        args[i].on_seed       = on_seed;
        args[i].cb_userdata   = userdata;
        args[i].found_total   = &found_total;
        args[i].scanned_total = &scanned_total;
        args[i].mutex         = &mutex;
        pthread_create(&threads[i], NULL, stream_thread_worker, &args[i]);
    }

    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&mutex);

    if (scanned_out)
        *scanned_out = scanned_total;
}
