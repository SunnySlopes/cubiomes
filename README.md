# cubiomes

Cubiomes is a standalone library, written in C, that mimics the biome and
feature generation of Minecraft Java Edition. It is designed to be a fast,
low-memory tool for building custom seed-finders, map viewers, and
seed-search APIs.

This repository ships **two things**:

| Component | Description |
|-----------|-------------|
| **C library** (`libcubiomes`) | Link directly into your own programs to query biomes and structures with nanosecond-level performance. |
| **HTTP / WebSocket API server** (`server`) | Drop-in REST + streaming server that exposes seed-search over the network. See [Hosting as an API](#hosting-as-an-api). |

#### Cubiomes-Viewer

If you prefer a GUI, the [cubiomes-viewer](https://github.com/Cubitect/cubiomes-viewer) graphical application is built on this library.

#### Audience

You should be comfortable with C (for the library) or with running a small
HTTP server (for the API). A basic understanding of Minecraft biome generation
is helpful but not required.

---

## Table of Contents

- [Building](#building)
- [Getting Started (Library)](#getting-started-library)
  - [Biome Generator](#biome-generator)
  - [Biome Generation in a Range](#biome-generation-in-a-range)
  - [Structure Generation](#structure-generation)
  - [Quad-Witch-Huts](#quad-witch-huts)
  - [Speedrunning Seeds](#speedrunning-seeds-mcsr-style)
  - [Strongholds and Spawn](#strongholds-and-spawn)
- [Hosting as an API](#hosting-as-an-api)
  - [Prerequisites](#prerequisites)
  - [Build the Server](#build-the-server)
  - [Run the Server](#run-the-server)
  - [Configuration](#configuration)
  - [API Reference](#api-reference)
    - [GET /structures](#get-structures)
    - [GET /biomes](#get-biomes)
    - [POST /search](#post-search)
    - [WS /search/stream](#ws-searchstream)
  - [Rate Limiting](#rate-limiting)
  - [Deploying with systemd](#deploying-with-systemd)
  - [Deploying with Docker](#deploying-with-docker)
  - [Reverse-Proxy with nginx](#reverse-proxy-with-nginx)

---

## Building

### Library only

```sh
# Debug build
make -f makefile debug

# Optimized release build (default)
make -f makefile release

# Native-tuned build (fastest, non-portable)
make -f makefile native
```

This produces `libcubiomes.a` in the working directory.

### API server + library

The `Makefile` (capital M) builds both the library and the HTTP server:

```sh
make          # produces ./server and libcubiomes.a
make clean    # remove build artefacts
```

Requires `gcc`, `libmicrohttpd`, and `pthreads` (see [Prerequisites](#prerequisites)).

---

## Getting Started (Library)

This section is meant to give you a quick starting point with small example
programs if you want to use this library to find your own biome-dependent
features.


### Biome Generator

Let's create a simple program called `find_biome_at.c` which tests seeds for a Mushroom Fields biome at a predefined location.

```C
// check the biome at a block position
#include "generator.h"
#include <stdio.h>

int main()
{
    // Set up a biome generator that reflects the biome generation of
    // Minecraft 1.18.
    Generator g;
    setupGenerator(&g, MC_1_18, 0);

    // Seeds are internally represented as unsigned 64-bit integers.
    uint64_t seed;
    for (seed = 0; ; seed++)
    {
        // Apply the seed to the generator for the Overworld dimension.
        applySeed(&g, DIM_OVERWORLD, seed);

        // To get the biome at a single block position, we can use getBiomeAt().
        int scale = 1; // scale=1: block coordinates, scale=4: biome coordinates
        int x = 0, y = 63, z = 0;
        int biomeID = getBiomeAt(&g, scale, x, y, z);
        if (biomeID == mushroom_fields)
        {
            printf("Seed %" PRId64 " has a Mushroom Fields biome at "
                "block position (%d, %d).\n", (int64_t) seed, x, z);
            break;
        }
    }

    return 0;
}
```

You can compile this code either by directly adding a target to the makefile via
```
$ cd cubiomes
$ make
```
...or you can compile and link to a cubiomes archive using either of the following commands.
```
$ gcc find_biome_at.c libcubiomes.a -fwrapv -lm   # static
$ gcc find_biome_at.c -L. -lcubiomes -fwrapv -lm  # dynamic
```
Both commands assume that your source code is saved as `find_biome_at.c` in the cubiomes working directory. If your makefile is configured to use pthreads, you may also need to add the `-lpthread` option for the compiler.
The option `-fwrapv` enforces two's complement for signed integer overflow, which is otherwise undefined behavior. It is not really necessary for this example, but it is a common pitfall when dealing with code that emulates the behavior of Java.
Running the program should output:
```
$ ./a.out
Seed 262 has a Mushroom Fields biome at block position (0, 0).
```

### Biome Generation in a Range

We can also generate biomes for an area or volume using `genBiomes()`. This will utilize whatever optimizations are available for the generator, which can be much faster than generating each position individually. (The layered generators for versions up to 1.17 will benefit significantly more from this than the noise-based ones.)

Before we can generate the biomes for an area or volume, we need to define the bounds with a `Range` structure and allocate the necessary buffer using `allocCache()`. The `Range` is described by a scale, position, and size, where each cell inside the `Range` represents an amount of `scale` blocks in the horizontal axes. The vertical direction is treated separately and always follows the biome coordinate scaling of 1:4, except for when `scale == 1`, in which case the vertical scaling is also 1:1.

The only supported values for `scale` are 1, 4, 16, 64, and (for the Overworld) 256. For versions up to 1.17, the scale is matched to an appropriate biome layer and will influence the biomes that can generate.

```C
// generate an image of the world
#include "generator.h"
#include "util.h"

int main()
{
    Generator g;
    setupGenerator(&g, MC_1_18, LARGE_BIOMES);

    uint64_t seed = 123LL;
    applySeed(&g, DIM_OVERWORLD, seed);

    Range r;
    // 1:16, a.k.a. horizontal chunk scaling
    r.scale = 16;
    // Define the position and size for a horizontal area:
    r.x = -60, r.z = -60;   // position (x,z)
    r.sx = 120, r.sz = 120; // size (width,height)
    // Set the vertical range as a plane near sea level at scale 1:4.
    r.y = 15, r.sy = 1;

    // Allocate the necessary cache for this range.
    int *biomeIds = allocCache(&g, r);

    // Generate the area inside biomeIds, indexed as:
    // biomeIds[i_y*r.sx*r.sz + i_z*r.sx + i_x]
    // where (i_x, i_y, i_z) is a position relative to the range cuboid.
    genBiomes(&g, biomeIds, r);

    // Map the biomes to an image buffer, with 4 pixels per biome cell.
    int pix4cell = 4;
    int imgWidth = pix4cell*r.sx, imgHeight = pix4cell*r.sz;
    unsigned char biomeColors[256][3];
    initBiomeColors(biomeColors);
    unsigned char *rgb = (unsigned char *) malloc(3*imgWidth*imgHeight);
    biomesToImage(rgb, biomeColors, biomeIds, r.sx, r.sz, pix4cell, 2);

    // Save the RGB buffer to a PPM image file.
    savePPM("map.ppm", rgb, imgWidth, imgHeight);

    // Clean up.
    free(biomeIds);
    free(rgb);

    return 0;
}
```


### Structure Generation

The generation of structures can usually be regarded as a two-stage process: generation attempts and biome checks. For most structures, Minecraft divides the world into a grid of regions (usually 32x32 chunks) and performs one generation attempt in each. We can use `getStructurePos()` to get the position of such a generation attempt, and then test whether a structure will actually generate there with `isViableStructurePos()`; however, this is more expensive to compute (requiring many microseconds instead of nanoseconds).

Note: some structures (in particular desert pyramids, jungle temples, and woodland mansions) in 1.18 no longer depend solely on the biomes and can also fail to generate based on the surface height near the generation attempt. Unfortunately, cubiomes does not provide block-level world generation and cannot check for this, and may therefore yield false positive positions. Support for an approximation of the surface height might be added in the future to improve accuracy.


```C
// find a seed with a certain structure at the origin chunk
#include "finders.h"
#include <stdio.h>

int main()
{
    int structType = Outpost;
    int mc = MC_1_18;

    Generator g;
    setupGenerator(&g, mc, 0);

    uint64_t lower48;
    for (lower48 = 0; ; lower48++)
    {
        // The structure position depends only on the region coordinates and
        // the lower 48-bits of the world seed.
        Pos p;
        if (!getStructurePos(structType, mc, lower48, 0, 0, &p))
            continue;

        // Look for a seed with the structure at the origin chunk.
        if (p.x >= 16 || p.z >= 16)
            continue;

        // Look for a full 64-bit seed with viable biomes.
        uint64_t upper16;
        for (upper16 = 0; upper16 < 0x10000; upper16++)
        {
            uint64_t seed = lower48 | (upper16 << 48);
            applySeed(&g, DIM_OVERWORLD, seed);
            if (isViableStructurePos(structType, &g, p.x, p.z, 0))
            {
                printf("Seed %" PRId64 " has a Pillager Outpost at (%d, %d).\n",
                    (int64_t) seed, p.x, p.z);
                return 0;
            }
        }
    }
}
```

#### Quad-Witch-Huts

A commonly desired feature is Quad-Witch-Huts or similar multi-structure clusters. To test for these types of seeds, we can look a little deeper into how the generation attempts are determined. Notice that the positions depend only on the structure type, region coordinates, and the lower 48 bits of the seed. Also, once we have found a seed with the desired generation attempts, we can move them around by transforming the 48-bit seed using `moveStructure()`. This means there is a set of seed bases that can function as a starting point to generate all other seeds with similar structure placement.

The function `searchAll48()` can be used to find a complete set of 48-bit seed bases for a custom criterion. Given that in general, it can take a very long time to check all 2^48 seeds (days or weeks), the function provides some functionality to save the results to disk which can be loaded again using `loadSavedSeeds()`. Luckily, it is possible in some cases to reduce the search space even further: for Swamp Huts and structures with a similar structure configuration, there are only a handful of constellations where the structures are close enough together to run simultaneously. Conveniently, these constellations differ uniquely at the lower 20 bits. (This is hard to prove, or at least I haven't found a rigorous proof that doesn't rely on brute forcing.) By specifying a list of lower 20-bit values, we can reduce the search space to the order of 2^28, which can be checked in a reasonable amount of time.


```C
// find seeds with a quad-witch-hut about the origin
#include "quadbase.h"
#include <stdio.h>

int check(uint64_t s48, void *data)
{
    const StructureConfig sconf = *(const StructureConfig*) data;
    return isQuadBase(sconf, s48 - sconf.salt, 128);
}

int main()
{
    int styp = Swamp_Hut;
    int mc = MC_1_18;
    uint64_t basecnt = 0;
    uint64_t *bases = NULL;
    int threads = 8;
    Generator g;

    StructureConfig sconf;
    getStructureConfig(styp, mc, &sconf);

    printf("Preparing seed bases...\n");
    // Get all 48-bit quad-witch-hut bases, but consider only the best 20-bit
    // constellations where the structures are the closest together.
    int err = searchAll48(&bases, &basecnt, NULL, threads,
        low20QuadIdeal, 20, check, &sconf);

    if (err || !bases)
    {
        printf("Failed to generate seed bases.\n");
        exit(1);
    }

    setupGenerator(&g, mc, 0);

    uint64_t i;
    for (i = 0; i < basecnt; i++)
    {
        // The quad bases by themselves have structures in regions (0,0)-(1,1)
        // so we can move them by -1 regions to have them around the origin.
        uint64_t s48 = moveStructure(bases[i] - sconf.salt, -1, -1);

        Pos pos[4];
        getStructurePos(styp, mc, s48, -1, -1, &pos[0]);
        getStructurePos(styp, mc, s48, -1,  0, &pos[1]);
        getStructurePos(styp, mc, s48,  0, -1, &pos[2]);
        getStructurePos(styp, mc, s48,  0,  0, &pos[3]);

        uint64_t high;
        for (high = 0; high < 0x10000; high++)
        {
            uint64_t seed = s48 | (high << 48);
            applySeed(&g, DIM_OVERWORLD, seed);

            if (isViableStructurePos(styp, &g, pos[0].x, pos[0].z, 0) &&
                isViableStructurePos(styp, &g, pos[1].x, pos[1].z, 0) &&
                isViableStructurePos(styp, &g, pos[2].x, pos[2].z, 0) &&
                isViableStructurePos(styp, &g, pos[3].x, pos[3].z, 0))
            {
                printf("%" PRId64 "\n", (int64_t) seed);
            }
        }
    }

    free(bases);
    return 0;
}
```

#### Speedrunning Seeds (MCSR-style)

When speedrunning Minecraft, a "good" seed is one where the three structures
needed to reach the credits appear close to the world origin:

| Structure | Purpose |
|-----------|---------|
| **Nether Fortress** | Source of Blaze Rods → Eyes of Ender |
| **Bastion Remnant** | Gold for Piglin bartering → Ender Pearls |
| **Stronghold** | End Portal used to reach the Ender Dragon |

The example below (`speedrun_seed.c`) applies a two-stage filter:

1. **Geometry check (fast):** scans the 3×3 Nether region grid around the
   origin and rejects seeds where no Fortress/Bastion generation attempt
   falls within the desired Nether-block radius.  The first Stronghold
   approximate position is also checked at this stage.
2. **Biome validation (slow, only when stage 1 passes):** calls
   `isViableStructurePos()` to confirm the biome requirements are actually
   met, then resolves the exact Stronghold location and estimates spawn.

> Note: Nether coordinates are 1:8 relative to the Overworld, so 200 Nether
> blocks equals ~1600 Overworld blocks.

Save the snippet below as `speedrun_seed.c` inside the cubiomes directory,
then build and run:

```sh
$ make                    # builds libcubiomes.a
$ gcc speedrun_seed.c libcubiomes.a -fwrapv -lm -lpthread -o speedrun_seed
$ ./speedrun_seed
```

Expected output (exact seeds vary because the search starts from a random
offset each run):

```
Searching for speedrun seeds (MC 1.16, Java Edition)...
Criteria:
  Nether Fortress within 200 Nether blocks of the Nether origin
  Bastion Remnant within 200 Nether blocks of the Nether origin
  First Stronghold within 2000 Overworld blocks of the world origin

=== Seed #1 ===
  World seed:               152303138064409
  Overworld spawn:          ( -152,    -8)
  Nether Fortress:          ( -128,   112)  [~170 Nether blocks from origin]
  Bastion Remnant:          (    0,  -112)  [~112 Nether blocks from origin]
  First Stronghold:         ( 1716,  -124)  [~1720 blocks from origin]
...
```

```C
// speedrun_seed.c  –  find seeds with a nearby Fortress, Bastion and Stronghold
#include "finders.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

#define MC_VERSION          MC_1_16
#define MAX_FORTRESS_DIST   200   /* Nether blocks from the Nether origin */
#define MAX_BASTION_DIST    200   /* Nether blocks from the Nether origin */
#define MAX_STRONGHOLD_DIST 2000  /* Overworld blocks from the world origin */
#define SEEDS_TO_FIND       5

static double sq_dist(int x, int z) { return (double)x*x + (double)z*z; }

int main(void)
{
    /* Hash the current timestamp for a platform-independent random start. */
    uint64_t s48 = (uint64_t)(unsigned long)time(NULL);
    s48 = (s48 ^ (s48 >> 30)) * 0xbf58476d1ce4e5b9ULL;
    s48 = (s48 ^ (s48 >> 27)) * 0x94d049bb133111ebULL;
    s48 = (s48 ^ (s48 >> 31)) & 0xffffffffffffULL;

    Generator g;
    setupGenerator(&g, MC_VERSION, 0);

    const double maxFD2 = (double)MAX_FORTRESS_DIST   * MAX_FORTRESS_DIST;
    const double maxBD2 = (double)MAX_BASTION_DIST    * MAX_BASTION_DIST;
    const double maxSD2 = (double)MAX_STRONGHOLD_DIST * MAX_STRONGHOLD_DIST;

    int found = 0;
    for (; found < SEEDS_TO_FIND; s48 = (s48 + 1) & 0xffffffffffffULL)
    {
        /* Stage 1: geometry filter (cheap) */
        Pos fortressPos = {0, 0}, bastionPos = {0, 0};
        int hasFortress = 0, hasBastion = 0;
        int rx, rz;
        for (rx = -1; rx <= 1; rx++) {
            for (rz = -1; rz <= 1; rz++) {
                Pos p;
                if (!hasFortress &&
                    getStructurePos(Fortress, MC_VERSION, s48, rx, rz, &p) &&
                    sq_dist(p.x, p.z) <= maxFD2)
                { fortressPos = p; hasFortress = 1; }
                if (!hasBastion &&
                    getStructurePos(Bastion, MC_VERSION, s48, rx, rz, &p) &&
                    sq_dist(p.x, p.z) <= maxBD2)
                { bastionPos = p; hasBastion = 1; }
                if (hasFortress && hasBastion) break;
            }
            if (hasFortress && hasBastion) break;
        }
        if (!hasFortress || !hasBastion) continue;

        StrongholdIter sh;
        Pos shApprox = initFirstStronghold(&sh, MC_VERSION, s48);
        if (sq_dist(shApprox.x, shApprox.z) > maxSD2) continue;

        /* Stage 2: biome validation (expensive) */
        applySeed(&g, DIM_NETHER, s48);
        if (!isViableStructurePos(Fortress, &g, fortressPos.x, fortressPos.z, 0)) continue;
        if (!isViableStructurePos(Bastion,  &g, bastionPos.x,  bastionPos.z,  0)) continue;

        applySeed(&g, DIM_OVERWORLD, s48);
        if (nextStronghold(&sh, &g) < 0) continue;
        Pos spawn = estimateSpawn(&g, NULL);

        found++;
        printf("=== Seed #%d ===\n", found);
        printf("  World seed:       %" PRId64 "\n", (int64_t)s48);
        printf("  Overworld spawn:  (%5d, %5d)\n", spawn.x, spawn.z);
        printf("  Nether Fortress:  (%5d, %5d)  [~%.0f Nether blocks]\n",
               fortressPos.x, fortressPos.z,
               sqrt(sq_dist(fortressPos.x, fortressPos.z)));
        printf("  Bastion Remnant:  (%5d, %5d)  [~%.0f Nether blocks]\n",
               bastionPos.x, bastionPos.z,
               sqrt(sq_dist(bastionPos.x, bastionPos.z)));
        printf("  First Stronghold: (%5d, %5d)  [~%.0f blocks]\n",
               sh.pos.x, sh.pos.z,
               sqrt(sq_dist(sh.pos.x, sh.pos.z)));
        printf("\n");
    }
    return 0;
}
```

#### Strongholds and Spawn

Strongholds, as well as the world spawn point, actually search until they find a suitable location, rather than checking a single spot like most other structures. This causes them to be particularly performance expensive to find. Furthermore, the positions of strongholds have to be generated in a certain order, which can be done in iteratively with `initFirstStronghold()` and `nextStronghold()`. For the world spawn, the generation starts with a search for a suitable biome near the origin and will continue until a grass or podzol block is found. There is no reliable way to check actual blocks, so the search relies on a statistic, matching grass presence to biomes. Alternatively, we can simply use `estimateSpawn()` and terminate the search after the first biome check under the assumption that grass is nearby.


```C
// find spawn and the first N strongholds
#include "finders.h"
#include <stdio.h>

int main()
{
    int mc = MC_1_18;
    uint64_t seed = 3055141959546LL;

    // Only the first stronghold has a position that can be estimated
    // (+/-112 blocks) without biome check.
    StrongholdIter sh;
    Pos pos = initFirstStronghold(&sh, mc, seed);

    printf("Seed: %" PRId64 "\n", (int64_t) seed);
    printf("Estimated position of first stronghold: (%d, %d)\n", pos.x, pos.z);

    Generator g;
    setupGenerator(&g, mc, 0);
    applySeed(&g, DIM_OVERWORLD, seed);

    pos = getSpawn(&g);
    printf("Spawn: (%d, %d)\n", pos.x, pos.z);

    int i, N = 12;
    for (i = 1; i <= N; i++)
    {
        if (nextStronghold(&sh, &g) <= 0)
            break;
        printf("Stronghold #%-3d: (%6d, %6d)\n", i, sh.pos.x, sh.pos.z);
    }

    return 0;
}
```






---

## Hosting as an API

The repository includes a ready-to-run HTTP/WebSocket seed-search server built
on [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/). Once running,
any language that can speak HTTP or WebSockets can query it.

### Prerequisites

| Dependency | Ubuntu / Debian | Fedora / RHEL | macOS (Homebrew) |
|------------|-----------------|---------------|-----------------|
| GCC | `gcc` | `gcc` | `gcc` via Xcode CLT |
| libmicrohttpd | `libmicrohttpd-dev` | `libmicrohttpd-devel` | `libmicrohttpd` |
| pthreads | (included in glibc) | (included in glibc) | (included in libc) |

```sh
# Ubuntu / Debian
sudo apt-get install gcc libmicrohttpd-dev

# Fedora / RHEL
sudo dnf install gcc libmicrohttpd-devel

# macOS
brew install libmicrohttpd
```

### Build the Server

```sh
git clone https://github.com/Praveenkumar801/cubiomes.git
cd cubiomes
make          # builds ./server and libcubiomes.a
```

### Run the Server

```sh
./server              # listens on port 8080 (default)
./server 9000         # listens on port 9000
```

Startup output:

```
Cubiomes seed-search API listening on port 8080
  GET  http://localhost:8080/structures
  GET  http://localhost:8080/biomes
  POST http://localhost:8080/search
  WS   ws://localhost:8080/search/stream
Rate limit: 10 requests per 60 seconds per IP
Press Ctrl-C to stop.
```

Press **Ctrl-C** (or send `SIGTERM`) to shut down gracefully.

### Configuration

The following compile-time constants in `src/api.h` and `src/engine.h` can be
adjusted before building:

| Constant | File | Default | Description |
|----------|------|---------|-------------|
| `DEFAULT_PORT` | `src/main.c` | `8080` | TCP port the server listens on |
| `RATE_LIMIT_WINDOW` | `src/api.h` | `60` | Sliding-window length (seconds) |
| `RATE_LIMIT_MAX_REQS` | `src/api.h` | `10` | Max requests per IP per window |
| `RATE_TABLE_SIZE` | `src/api.h` | `256` | Hash-table slots for tracked IPs |
| `MAX_STRUCT_QUERIES` | `src/engine.h` | `16` | Max structure constraints per request |
| `MAX_RESULTS` | `src/engine.h` | `10` | Hard cap on seeds returned per request |
| `MAX_THREADS` | `src/engine.h` | `16` | Worker threads used for seed search |

---

### API Reference

All responses use `Content-Type: application/json`.

#### GET /structures

Returns the list of structure types that the server understands.

```sh
curl http://localhost:8080/structures
```

**Response `200 OK`:**

```json
{
  "structures": [
    "feature", "desert_pyramid", "jungle_temple", "swamp_hut",
    "igloo", "village", "ocean_ruin", "shipwreck", "monument",
    "mansion", "outpost", "ruined_portal", "ancient_city", "treasure",
    "fortress", "bastion", "end_city", "trail_ruins", "trial_chambers"
  ]
}
```

---

#### GET /biomes

Returns the list of biome names that can be used in the `biome` field of a
structure constraint.

```sh
curl http://localhost:8080/biomes
```

**Response `200 OK`:**

```json
{
  "biomes": [
    "ocean", "plains", "desert", "mountains", "forest", "taiga", "swamp",
    "river", "nether_wastes", "the_end", "frozen_ocean", "frozen_river",
    "snowy_tundra", "snowy_mountains", "mushroom_fields",
    "mushroom_field_shore", "beach", "desert_hills", "wooded_hills",
    "taiga_hills", "mountain_edge", "jungle", "jungle_hills", "jungle_edge",
    "deep_ocean", "stone_shore", "snowy_beach", "birch_forest",
    "birch_forest_hills", "dark_forest", "snowy_taiga", "snowy_taiga_hills",
    "giant_tree_taiga", "giant_tree_taiga_hills", "wooded_mountains",
    "savanna", "savanna_plateau", "badlands", "wooded_badlands_plateau",
    "badlands_plateau", "warm_ocean", "lukewarm_ocean", "cold_ocean",
    "bamboo_jungle", "soul_sand_valley", "crimson_forest", "warped_forest",
    "basalt_deltas", "meadow", "grove", "snowy_slopes", "jagged_peaks",
    "frozen_peaks", "stony_peaks", "deep_dark", "mangrove_swamp",
    "cherry_grove", "pale_garden"
  ]
}
```

---

#### POST /search

Synchronously searches a seed range and returns up to `max_results` seeds
(hard-capped at `MAX_RESULTS`, default 10) that satisfy **all** specified
structure constraints. The seed range must not exceed **1 billion** seeds.

**Request body (JSON):**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `version` | string | ✓ | Minecraft version, e.g. `"1.21"`, `"1.18"`, `"1.16.5"` |
| `seed_start` | integer | ✓ | First seed to check (signed 64-bit) |
| `seed_end` | integer | ✓ | Last seed to check (inclusive, signed 64-bit) |
| `max_results` | integer | ✓ | Stop after finding this many seeds (≤ `MAX_RESULTS`) |
| `structures` | array | ✓ | One or more structure constraint objects (see below) |

Each **structure constraint** object:

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | ✓ | Structure name from `GET /structures` |
| `max_distance` | integer | ✓ | Max block distance from `(0, 0)` |
| `biome` | string | ✗ | If provided, the structure must spawn in this biome (name from `GET /biomes`). Omit to accept any biome. |

**Example — find seeds with a Village within 500 blocks of spawn:**

```sh
curl -X POST http://localhost:8080/search \
     -H "Content-Type: application/json" \
     -d '{
       "version":    "1.21",
       "seed_start": 0,
       "seed_end":   1000000,
       "max_results": 5,
       "structures": [
         { "type": "village", "max_distance": 500 }
       ]
     }'
```

**Response `200 OK`:**

```json
{
  "seeds": [1337, 42891, 103742, 204519, 387201],
  "scanned": 387202
}
```

**Example — multiple structure constraints (Village + Monument):**

```sh
curl -X POST http://localhost:8080/search \
     -H "Content-Type: application/json" \
     -d '{
       "version":    "1.18",
       "seed_start": 0,
       "seed_end":   10000000,
       "max_results": 3,
       "structures": [
         { "type": "village",  "max_distance": 400 },
         { "type": "monument", "max_distance": 800 }
       ]
     }'
```

**Example — Village in a specific biome (plains) within 500 blocks:**

```sh
curl -X POST http://localhost:8080/search \
     -H "Content-Type: application/json" \
     -d '{
       "version":    "1.21",
       "seed_start": 0,
       "seed_end":   5000000,
       "max_results": 3,
       "structures": [
         { "type": "village", "max_distance": 500, "biome": "plains" }
       ]
     }'
```

**Error responses:**

| HTTP status | Condition |
|-------------|-----------|
| `400 Bad Request` | Missing/invalid fields, unknown version, structure type, or biome name, seed range > 1 billion |
| `404 Not Found` | Unknown URL path |
| `405 Method Not Allowed` | Wrong HTTP method |
| `429 Too Many Requests` | Rate limit exceeded |
| `500 Internal Server Error` | Unexpected server error |

Error body: `{"error": "<human-readable message>"}`

---

#### WS /search/stream

A WebSocket endpoint that streams matching seeds in real time as they are
found, instead of waiting for the full search to complete. Useful for large
seed ranges or interactive UIs.

**Upgrade:** send a standard WebSocket upgrade to `ws://host:port/search/stream`.

**After the connection is established**, send a single text frame containing
the same JSON object accepted by `POST /search`:

```json
{
  "version":    "1.21",
  "seed_start": 0,
  "seed_end":   50000000,
  "max_results": 10,
  "structures": [
    { "type": "mansion", "max_distance": 1000 }
  ]
}
```

**Server sends** one text frame per matching seed:

```json
{"seed": 4831729}
```

When the search is complete (or `max_results` is reached), the server sends a
final summary frame and then closes the connection normally:

```json
{"done": true, "scanned": 32019847}
```

**Browser JavaScript example:**

```js
const ws = new WebSocket('ws://localhost:8080/search/stream');

ws.onopen = () => {
  ws.send(JSON.stringify({
    version: '1.21',
    seed_start: 0,
    seed_end: 50_000_000,
    max_results: 10,
    structures: [{ type: 'mansion', max_distance: 1000 }]
  }));
};

ws.onmessage = ({ data }) => {
  const msg = JSON.parse(data);
  if (msg.done) {
    console.log(`Search complete. Scanned ${msg.scanned} seeds.`);
    ws.close();
  } else {
    console.log('Found seed:', msg.seed);
  }
};

ws.onerror = (err) => console.error('WebSocket error:', err);
```

**Node.js example** (using the `ws` package):

```js
const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:8080/search/stream');

ws.on('open', () => {
  ws.send(JSON.stringify({
    version: '1.18',
    seed_start: 0,
    seed_end: 5_000_000,
    max_results: 5,
    structures: [{ type: 'village', max_distance: 300 }]
  }));
});

ws.on('message', (data) => {
  const msg = JSON.parse(data);
  if (msg.done) {
    console.log(`Done. Scanned: ${msg.scanned}`);
    ws.terminate();
  } else {
    console.log('Seed:', msg.seed);
  }
});
```

---

### Rate Limiting

The server enforces a per-IP sliding-window rate limit to prevent abuse.
By default each IP address may send at most **10 requests per 60 seconds**.
When the limit is exceeded the server responds with:

```
HTTP 429 Too Many Requests
{"error":"rate limit exceeded, try again later"}
```

The limit applies to all endpoints including WebSocket upgrade requests.
Adjust `RATE_LIMIT_MAX_REQS` and `RATE_LIMIT_WINDOW` in `src/api.h` and
rebuild to change the defaults.

---

### Deploying with systemd

Create `/etc/systemd/system/cubiomes-api.service`:

```ini
[Unit]
Description=Cubiomes Seed-Search API
After=network.target

[Service]
Type=simple
User=cubiomes
WorkingDirectory=/opt/cubiomes
ExecStart=/opt/cubiomes/server 8080
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Then enable and start it:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now cubiomes-api
sudo journalctl -u cubiomes-api -f   # follow logs
```

---

### Deploying with Docker

Create a `Dockerfile` in the repository root:

```dockerfile
FROM debian:bookworm-slim AS builder
RUN apt-get update && apt-get install -y gcc libmicrohttpd-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY . .
RUN make

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y libmicrohttpd12 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /build/server .
EXPOSE 8080
ENTRYPOINT ["./server", "8080"]
```

Build and run:

```sh
docker build -t cubiomes-api .
docker run -d -p 8080:8080 --name cubiomes-api cubiomes-api
```

---

### Reverse-Proxy with nginx

To serve the API behind nginx (e.g. at `https://example.com/api/`):

```nginx
# /etc/nginx/sites-available/cubiomes
server {
    listen 443 ssl;
    server_name example.com;

    # … your SSL certificate directives here …

    location /api/ {
        proxy_pass         http://127.0.0.1:8080/;
        proxy_http_version 1.1;

        # Required for WebSocket upgrade
        proxy_set_header Upgrade    $http_upgrade;
        proxy_set_header Connection "upgrade";

        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Allow long-running seed searches
        proxy_read_timeout 120s;
    }
}
```

Reload nginx after editing:

```sh
sudo nginx -t && sudo systemctl reload nginx
```
