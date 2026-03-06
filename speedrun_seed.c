/*
 * speedrun_seed.c
 *
 * Finds Minecraft Java Edition seeds suited for speedrunning, similar to
 * the criteria used by the MCSR (Minecraft Speedrunning) community.
 *
 * A quality speedrun seed needs three key structures close to the origin:
 *
 *   1. Nether Fortress  – provides Blaze Rods for Eyes of Ender.
 *   2. Bastion Remnant  – provides gold for Piglin bartering (Ender Pearls).
 *   3. Stronghold       – houses the End Portal used to reach the dragon.
 *
 * Nether coordinates are 1:8 relative to the Overworld, so 200 Nether
 * blocks equals roughly 1600 Overworld blocks.
 *
 * Build:
 *   cd cubiomes
 *   make                             # builds libcubiomes.a
 *   gcc speedrun_seed.c libcubiomes.a -fwrapv -lm -lpthread -o speedrun_seed
 *
 * Run:
 *   ./speedrun_seed
 */

#include "finders.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

/* Target Minecraft version.  Bastion Remnants require MC 1.16+. */
#define MC_VERSION  MC_1_16

/* Maximum distance from the Nether origin (0, 0) for Nether structures. */
#define MAX_FORTRESS_DIST  200   /* Nether blocks */
#define MAX_BASTION_DIST   200   /* Nether blocks */

/* Maximum distance from the Overworld origin for the first Stronghold. */
#define MAX_STRONGHOLD_DIST  2000  /* Overworld blocks */

/* How many matching seeds to print before stopping. */
#define SEEDS_TO_FIND  5

/* Squared Euclidean distance (avoids sqrt in hot path). */
static double sq_dist(int x, int z)
{
    return (double)x * x + (double)z * z;
}

int main(void)
{
    /*
     * Start from a hashed timestamp so successive runs produce different
     * results without relying on rand()'s platform-dependent RAND_MAX.
     * Structure positions depend only on the lower 48 bits of the seed.
     */
    uint64_t s48 = (uint64_t)(unsigned long)time(NULL);
    s48 = (s48 ^ (s48 >> 30)) * 0xbf58476d1ce4e5b9ULL;
    s48 = (s48 ^ (s48 >> 27)) * 0x94d049bb133111ebULL;
    s48 = (s48 ^ (s48 >> 31)) & 0xffffffffffffULL;

    Generator g;
    setupGenerator(&g, MC_VERSION, 0);

    printf("Searching for speedrun seeds (MC 1.16, Java Edition)...\n");
    printf("Criteria:\n");
    printf("  Nether Fortress within %d Nether blocks of the Nether origin\n",
           MAX_FORTRESS_DIST);
    printf("  Bastion Remnant within %d Nether blocks of the Nether origin\n",
           MAX_BASTION_DIST);
    printf("  First Stronghold within %d Overworld blocks of the world origin\n\n",
           MAX_STRONGHOLD_DIST);

    const double maxFortDist2 = (double)MAX_FORTRESS_DIST * MAX_FORTRESS_DIST;
    const double maxBastDist2 = (double)MAX_BASTION_DIST  * MAX_BASTION_DIST;
    const double maxShDist2   = (double)MAX_STRONGHOLD_DIST * MAX_STRONGHOLD_DIST;

    int found = 0;

    for (; found < SEEDS_TO_FIND; s48 = (s48 + 1) & 0xffffffffffffULL)
    {
        /* ------------------------------------------------------------------ *
         * Stage 1 – cheap geometry filter.                                   *
         *                                                                     *
         * Fortress and Bastion share the same region grid (27 chunks, 432   *
         * Nether blocks per region side).  We scan the 3×3 region grid      *
         * centered on the Nether origin.                                       *
         * ------------------------------------------------------------------ */
        Pos fortressPos = {0, 0};
        Pos bastionPos  = {0, 0};
        int hasFortress = 0;
        int hasBastion  = 0;

        int rx, rz;
        for (rx = -1; rx <= 1; rx++)
        {
            for (rz = -1; rz <= 1; rz++)
            {
                Pos p;
                if (!hasFortress &&
                    getStructurePos(Fortress, MC_VERSION, s48, rx, rz, &p) &&
                    sq_dist(p.x, p.z) <= maxFortDist2)
                {
                    fortressPos = p;
                    hasFortress = 1;
                }
                if (!hasBastion &&
                    getStructurePos(Bastion, MC_VERSION, s48, rx, rz, &p) &&
                    sq_dist(p.x, p.z) <= maxBastDist2)
                {
                    bastionPos = p;
                    hasBastion = 1;
                }
                if (hasFortress && hasBastion)
                    break;
            }
            if (hasFortress && hasBastion)
                break;
        }

        if (!hasFortress || !hasBastion)
            continue;

        /* Approximate first stronghold position (no biome check needed). */
        StrongholdIter sh;
        Pos shApprox = initFirstStronghold(&sh, MC_VERSION, s48);
        if (sq_dist(shApprox.x, shApprox.z) > maxShDist2)
            continue;

        /* ------------------------------------------------------------------ *
         * Stage 2 – biome validation (more expensive).                       *
         *                                                                     *
         * Confirm the Fortress and Bastion can actually generate there,      *
         * then locate the exact Stronghold and estimate the Overworld spawn. *
         * ------------------------------------------------------------------ */
        applySeed(&g, DIM_NETHER, s48);
        if (!isViableStructurePos(Fortress, &g, fortressPos.x, fortressPos.z, 0))
            continue;
        if (!isViableStructurePos(Bastion, &g, bastionPos.x, bastionPos.z, 0))
            continue;

        applySeed(&g, DIM_OVERWORLD, s48);
        if (nextStronghold(&sh, &g) < 0)
            continue;

        /* estimateSpawn() is faster than getSpawn() and accurate enough. */
        Pos spawn = estimateSpawn(&g, NULL);

        found++;
        printf("=== Seed #%d ===\n", found);
        printf("  World seed:               %" PRId64 "\n", (int64_t)s48);
        printf("  Overworld spawn:          (%5d, %5d)\n",
               spawn.x, spawn.z);
        printf("  Nether Fortress:          (%5d, %5d)"
               "  [~%.0f Nether blocks from origin]\n",
               fortressPos.x, fortressPos.z,
               sqrt(sq_dist(fortressPos.x, fortressPos.z)));
        printf("  Bastion Remnant:          (%5d, %5d)"
               "  [~%.0f Nether blocks from origin]\n",
               bastionPos.x, bastionPos.z,
               sqrt(sq_dist(bastionPos.x, bastionPos.z)));
        printf("  First Stronghold:         (%5d, %5d)"
               "  [~%.0f blocks from origin]\n",
               sh.pos.x, sh.pos.z,
               sqrt(sq_dist(sh.pos.x, sh.pos.z)));
        printf("\n");
    }

    return 0;
}
