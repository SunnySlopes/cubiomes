#ifndef ENGINE_H_
#define ENGINE_H_

#include <stdint.h>

#define MAX_STRUCT_QUERIES 16
#define MAX_RESULTS        10
#define MAX_THREADS        16

typedef struct {
    int  type;          /* StructureType enum value */
    int  max_distance;  /* max block distance from (0,0) */
    int  biome;         /* required BiomeID at structure pos, or -1 for any */
} StructureQuery;

typedef struct {
    int            mc_version;
    int64_t        seed_start;
    int64_t        seed_end;
    int            max_results;
    StructureQuery structures[MAX_STRUCT_QUERIES];
    int            num_structures;
} SearchRequest;

typedef struct {
    int64_t  seeds[MAX_RESULTS];
    int      count;
    int64_t  scanned;
} SearchResult;

/*
 * Parse a Minecraft version string (e.g. "1.16.1") into an MCVersion enum
 * value.  Returns MC_UNDEF (0) on failure.
 */
int parse_mc_version(const char *str);

/*
 * Parse a structure-type name (e.g. "village") into a StructureType enum
 * value.  Returns -1 on failure.
 */
int parse_structure_type(const char *name);

/*
 * Parse a biome name (e.g. "plains") into a BiomeID enum value.
 * Returns -1 on failure.
 */
int parse_biome_name(const char *name);

/*
 * Run a multithreaded seed search according to *req and write the results
 * into *result (which must be zero-initialised by the caller).
 */
void search_seeds(const SearchRequest *req, SearchResult *result);

/*
 * Callback invoked for every seed that passes all structure checks.
 * Called serially (under the engine's internal mutex) so the implementation
 * does not need additional locking.
 */
typedef void (*seed_found_cb)(int64_t seed, void *userdata);

/*
 * Like search_seeds() but streams results via a callback instead of
 * collecting them in a SearchResult array.  Total seeds scanned is written
 * to *scanned_out if non-NULL.
 */
void search_seeds_stream(const SearchRequest *req,
                         seed_found_cb on_seed, void *userdata,
                         int64_t *scanned_out);

/*
 * Returns a NULL-terminated array of all supported structure-type name
 * strings (e.g. "village", "monument", …).  The array is static; do not
 * free or modify it.
 */
const char * const *get_structure_names(void);

/*
 * Returns a NULL-terminated array of all supported biome name strings
 * (e.g. "plains", "forest", …).  The array is static; do not free or
 * modify it.
 */
const char * const *get_biome_names(void);

#endif /* ENGINE_H_ */
