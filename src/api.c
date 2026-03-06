#include "api.h"
#include "engine.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../biomes.h"
#include "../finders.h"

/* ══════════════════════════════════════════════════════════════════════════
 * Rate limiter
 * ══════════════════════════════════════════════════════════════════════════ */

void rate_limiter_init(RateLimiter *rl)
{
    memset(rl->entries, 0, sizeof(rl->entries));
    pthread_mutex_init(&rl->mutex, NULL);
}

void rate_limiter_destroy(RateLimiter *rl)
{
    pthread_mutex_destroy(&rl->mutex);
}

int rate_limiter_check(RateLimiter *rl, const char *ip)
{
    /* djb2 hash of the IP string */
    unsigned int h = 5381;
    for (const char *p = ip; *p; p++)
        h = ((h << 5) + h) ^ (unsigned char)*p;
    int slot = (int)(h % RATE_TABLE_SIZE);

    time_t now = time(NULL);
    int allowed;

    pthread_mutex_lock(&rl->mutex);
    RateEntry *e = &rl->entries[slot];
    if (e->ip[0] == '\0' || strcmp(e->ip, ip) != 0) {
        /* New IP or evicted entry – start fresh */
        strncpy(e->ip, ip, sizeof(e->ip) - 1);
        e->ip[sizeof(e->ip) - 1] = '\0';
        e->count        = 1;
        e->window_start = now;
        allowed = 1;
    } else if (now - e->window_start >= RATE_LIMIT_WINDOW) {
        /* Window expired – reset */
        e->count        = 1;
        e->window_start = now;
        allowed = 1;
    } else {
        e->count++;
        allowed = e->count <= RATE_LIMIT_MAX_REQS;
    }
    pthread_mutex_unlock(&rl->mutex);
    return allowed;
}

/* Extract the client IP from an MHD connection into buf (NUL-terminated). */
static void get_client_ip(struct MHD_Connection *conn, char *buf, size_t buflen)
{
    const union MHD_ConnectionInfo *ci =
        MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (ci && ci->client_addr) {
        const struct sockaddr *sa = ci->client_addr;
        if (sa->sa_family == AF_INET) {
            inet_ntop(AF_INET,
                      &((const struct sockaddr_in  *)sa)->sin_addr,
                      buf, (socklen_t)buflen);
            return;
        } else if (sa->sa_family == AF_INET6) {
            inet_ntop(AF_INET6,
                      &((const struct sockaddr_in6 *)sa)->sin6_addr,
                      buf, (socklen_t)buflen);
            return;
        }
    }
    strncpy(buf, "unknown", buflen - 1);
    buf[buflen - 1] = '\0';
}

/* ══════════════════════════════════════════════════════════════════════════
 * Tiny JSON helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static const char *skip_ws_colon(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == ':') p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static const char *json_find_value(const char *json, const char *key)
{
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    return skip_ws_colon(p);
}

static int json_read_string(const char *json, const char *key,
                             char *out, int maxlen)
{
    const char *p = json_find_value(json, key);
    if (!p || *p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < maxlen - 1) out[i++] = *p++;
    out[i] = '\0';
    return 1;
}

static int json_read_int64(const char *json, const char *key, int64_t *out)
{
    const char *p = json_find_value(json, key);
    if (!p) return 0;
    char *end;
    *out = (int64_t)strtoll(p, &end, 10);
    return end != p;
}

static int json_read_int(const char *json, const char *key, int *out)
{
    int64_t v;
    if (!json_read_int64(json, key, &v)) return 0;
    *out = (int)v;
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * SHA-1  (RFC 3174) — used only for the WebSocket handshake
 * ══════════════════════════════════════════════════════════════════════════ */

static void sha1_hash(const uint8_t *msg, size_t len, uint8_t out[20])
{
    uint32_t h[5] = {
        0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u
    };

    size_t pad_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *p = (uint8_t *)calloc(1, pad_len);
    if (!p) return;

    memcpy(p, msg, len);
    p[len] = 0x80;

    uint64_t bit_len = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        p[pad_len - 8 + i] = (uint8_t)(bit_len >> ((7 - i) * 8));

    for (size_t off = 0; off < pad_len; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)p[off+i*4]   << 24) |
                   ((uint32_t)p[off+i*4+1] << 16) |
                   ((uint32_t)p[off+i*4+2] <<  8) |
                    (uint32_t)p[off+i*4+3];
        for (int i = 16; i < 80; i++) {
            uint32_t v = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (v << 1) | (v >> 31);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if      (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;             k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;             k = 0xCA62C1D6u; }
            uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    free(p);

    for (int i = 0; i < 5; i++) {
        out[i*4]   = (h[i] >> 24) & 0xFF;
        out[i*4+1] = (h[i] >> 16) & 0xFF;
        out[i*4+2] = (h[i] >>  8) & 0xFF;
        out[i*4+3] =  h[i]        & 0xFF;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Base-64 encoder
 * ══════════════════════════════════════════════════════════════════════════ */

static void base64_encode(const uint8_t *in, size_t inlen,
                           char *out, size_t outmax)
{
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t j = 0;
    for (size_t i = 0; i < inlen && j + 4 < outmax; i += 3) {
        uint32_t a = in[i];
        uint32_t b = (i + 1 < inlen) ? in[i + 1] : 0;
        uint32_t c = (i + 2 < inlen) ? in[i + 2] : 0;
        uint32_t v = (a << 16) | (b << 8) | c;
        out[j++] = t[(v >> 18) & 0x3F];
        out[j++] = t[(v >> 12) & 0x3F];
        out[j++] = (i + 1 < inlen) ? t[(v >>  6) & 0x3F] : '=';
        out[j++] = (i + 2 < inlen) ? t[ v        & 0x3F] : '=';
    }
    out[j] = '\0';
}

/* ══════════════════════════════════════════════════════════════════════════
 * WebSocket primitives
 * ══════════════════════════════════════════════════════════════════════════ */

/* Compute Sec-WebSocket-Accept from Sec-WebSocket-Key. */
static void ws_compute_accept(const char *key, char *out, size_t outmax)
{
    static const char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[128];
    snprintf(combined, sizeof(combined), "%s%s", key, magic);
    uint8_t sha[20];
    sha1_hash((const uint8_t *)combined, strlen(combined), sha);
    base64_encode(sha, 20, out, outmax);
}

/* Send an unmasked text frame (FIN=1, opcode=0x1).
 * Returns bytes sent, or -1 on error. */
static ssize_t ws_send_text(MHD_socket sock, const char *payload, size_t len)
{
    uint8_t hdr[4];
    size_t  hlen;
    hdr[0] = 0x81; /* FIN | text opcode */
    if (len < 126) {
        hdr[1] = (uint8_t)len;
        hlen = 2;
    } else if (len < 65536) {
        hdr[1] = 126;
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t) len;
        hlen = 4;
    } else {
        return -1; /* payload too large */
    }
    if (send(sock, hdr, hlen, MSG_NOSIGNAL) != (ssize_t)hlen)
        return -1;
    if (send(sock, payload, len, MSG_NOSIGNAL) != (ssize_t)len)
        return -1;
    return (ssize_t)(hlen + len);
}

/* Send a WebSocket close frame with a 2-byte status code. */
static void ws_send_close(MHD_socket sock, uint16_t code)
{
    uint8_t frame[4];
    frame[0] = 0x88; /* FIN | close opcode */
    frame[1] = 2;
    frame[2] = (uint8_t)(code >> 8);
    frame[3] = (uint8_t) code;
    send(sock, frame, 4, MSG_NOSIGNAL);
}

/* Receive one complete WebSocket frame from the client, unmasking the
 * payload.  Returns payload length on success, or -1 on error or close. */
static ssize_t ws_recv_frame(MHD_socket sock, char *buf, size_t bufmax)
{
    uint8_t hdr[2];
    if (recv(sock, hdr, 2, MSG_WAITALL) != 2) return -1;

    int opcode = hdr[0] & 0x0F;
    if (opcode == 0x8) return -1; /* close frame */
    if (opcode != 0x1 && opcode != 0x2) return -1; /* only text/binary */

    int    masked = (hdr[1] >> 7) & 1;
    size_t plen   =  hdr[1] & 0x7F;

    if (plen == 126) {
        uint8_t ext[2];
        if (recv(sock, ext, 2, MSG_WAITALL) != 2) return -1;
        plen = ((size_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        return -1; /* 64-bit length not supported */
    }

    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked && recv(sock, mask, 4, MSG_WAITALL) != 4) return -1;

    if (plen >= bufmax) return -1;
    if (recv(sock, buf, plen, MSG_WAITALL) != (ssize_t)plen) return -1;

    if (masked)
        for (size_t i = 0; i < plen; i++) buf[i] ^= mask[i % 4];

    buf[plen] = '\0';
    return (ssize_t)plen;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Request parsing
 * ══════════════════════════════════════════════════════════════════════════ */

static int parse_request(const char *body, SearchRequest *req,
                          const char **errmsg)
{
    memset(req, 0, sizeof(*req));

    char version_str[32] = {0};
    if (!json_read_string(body, "version", version_str, sizeof(version_str))) {
        *errmsg = "missing version";
        return 0;
    }
    req->mc_version = parse_mc_version(version_str);
    if (req->mc_version == MC_UNDEF) {
        *errmsg = "unknown version string";
        return 0;
    }

    if (!json_read_int64(body, "seed_start", &req->seed_start)) {
        *errmsg = "missing seed_start";
        return 0;
    }
    if (!json_read_int64(body, "seed_end", &req->seed_end)) {
        *errmsg = "missing seed_end";
        return 0;
    }
    if (req->seed_end < req->seed_start) {
        *errmsg = "seed_end must be >= seed_start";
        return 0;
    }
    if (req->seed_end - req->seed_start > 1000000000LL) {
        *errmsg = "seed range must not exceed 1 billion";
        return 0;
    }

    if (!json_read_int(body, "max_results", &req->max_results) ||
        req->max_results <= 0) {
        *errmsg = "missing or invalid max_results";
        return 0;
    }
    if (req->max_results > MAX_RESULTS)
        req->max_results = MAX_RESULTS;

    const char *arr = strstr(body, "\"structures\"");
    if (!arr) { *errmsg = "missing structures"; return 0; }
    arr = strchr(arr, '[');
    if (!arr) { *errmsg = "structures is not an array"; return 0; }
    arr++;

    req->num_structures = 0;
    while (*arr && req->num_structures < MAX_STRUCT_QUERIES) {
        const char *obj = strchr(arr, '{');
        if (!obj) break;
        const char *end = strchr(obj, '}');
        if (!end) break;
        const char *close_arr = strchr(arr, ']');
        if (close_arr && obj > close_arr) break;

        char buf[256];
        size_t len = (size_t)(end - obj) + 1;
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, obj, len);
        buf[len] = '\0';

        char    type_name[64] = {0};
        char    biome_name[64] = {0};
        int64_t max_dist = 0;
        json_read_string(buf, "type",         type_name, sizeof(type_name));
        json_read_int64(buf,  "max_distance", &max_dist);
        json_read_string(buf, "biome",        biome_name, sizeof(biome_name));

        int stype = parse_structure_type(type_name);
        if (stype < 0) { *errmsg = "unknown structure type"; return 0; }
        if (max_dist <= 0) { *errmsg = "max_distance must be positive"; return 0; }

        StructureConfig sconf;
        if (!getStructureConfig(stype, req->mc_version, &sconf)) {
            *errmsg = "structure type not available in requested version";
            return 0;
        }

        int biome_id = -1;
        if (biome_name[0] != '\0') {
            biome_id = parse_biome_name(biome_name);
            if (biome_id < 0) { *errmsg = "unknown biome name"; return 0; }
        }

        req->structures[req->num_structures].type         = stype;
        req->structures[req->num_structures].max_distance = (int)max_dist;
        req->structures[req->num_structures].biome        = biome_id;
        req->num_structures++;
        arr = end + 1;
    }

    if (req->num_structures == 0) { *errmsg = "structures array is empty"; return 0; }
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * GET /structures response builder
 * ══════════════════════════════════════════════════════════════════════════ */

static char *build_structures_json(void)
{
    const char * const *names = get_structure_names();
    size_t cap = 32;
    for (int i = 0; names[i]; i++) cap += strlen(names[i]) + 4;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, cap - (size_t)pos, "{\"structures\":[");
    for (int i = 0; names[i]; i++) {
        if (i > 0) pos += snprintf(buf + pos, cap - (size_t)pos, ",");
        pos += snprintf(buf + pos, cap - (size_t)pos, "\"%s\"", names[i]);
    }
    snprintf(buf + pos, cap - (size_t)pos, "]}");
    return buf;
}

/* ══════════════════════════════════════════════════════════════════════════
 * GET /biomes response builder
 * ══════════════════════════════════════════════════════════════════════════ */

static char *build_biomes_json(void)
{
    const char * const *names = get_biome_names();
    size_t cap = 32;
    for (int i = 0; names[i]; i++) cap += strlen(names[i]) + 4;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, cap - (size_t)pos, "{\"biomes\":[");
    for (int i = 0; names[i]; i++) {
        if (i > 0) pos += snprintf(buf + pos, cap - (size_t)pos, ",");
        pos += snprintf(buf + pos, cap - (size_t)pos, "\"%s\"", names[i]);
    }
    snprintf(buf + pos, cap - (size_t)pos, "]}");
    return buf;
}

/* ══════════════════════════════════════════════════════════════════════════
 * POST /search response formatter
 * ══════════════════════════════════════════════════════════════════════════ */

/* Max bytes per seed entry in the JSON array: up to 20 digits + comma */
#define JSON_SEED_ENTRY_MAX 22

static char *format_response(const SearchResult *result)
{
    size_t cap = 64 + (size_t)result->count * JSON_SEED_ENTRY_MAX;
    char  *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, cap - (size_t)pos, "{\"seeds\":[");
    for (int i = 0; i < result->count; i++) {
        if (i > 0) pos += snprintf(buf + pos, cap - (size_t)pos, ",");
        pos += snprintf(buf + pos, cap - (size_t)pos,
                        "%lld", (long long)result->seeds[i]);
    }
    snprintf(buf + pos, cap - (size_t)pos,
             "],\"scanned\":%lld}", (long long)result->scanned);
    return buf;
}

/* ══════════════════════════════════════════════════════════════════════════
 * WebSocket streaming handler  (GET /search/stream)
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    MHD_socket   sock;
    volatile int error;
} WsSendCtx;

/* Called by search_seeds_stream() for each matching seed.
 * Invocations are serialised by the engine's internal mutex. */
static void ws_on_seed(int64_t seed, void *userdata)
{
    WsSendCtx *ctx = (WsSendCtx *)userdata;
    if (ctx->error) return;
    char msg[48];
    int  len = snprintf(msg, sizeof(msg), "{\"seed\":%lld}", (long long)seed);
    if (ws_send_text(ctx->sock, msg, (size_t)len) < 0)
        ctx->error = 1;
}

/* MHD upgrade callback — runs in its own thread. */
static void ws_upgrade_handler(void *cls,
                                struct MHD_Connection *connection,
                                void *req_cls,
                                const char *extra_in, size_t extra_in_size,
                                MHD_socket sock,
                                struct MHD_UpgradeResponseHandle *urh)
{
    (void)cls; (void)connection; (void)req_cls;
    (void)extra_in; (void)extra_in_size;

    /* MHD may leave the socket in non-blocking mode; switch to blocking so
     * that recv() waits for the client's first frame. */
    int flags = fcntl((int)sock, F_GETFL, 0);
    if (flags != -1)
        fcntl((int)sock, F_SETFL, flags & ~O_NONBLOCK);

    /* 1. Read the search-request JSON from the client's first WebSocket frame */
    char    reqbuf[4096];
    ssize_t rlen = ws_recv_frame(sock, reqbuf, sizeof(reqbuf) - 1);
    if (rlen < 0) {
        ws_send_close(sock, 1003 /* unsupported data */);
        goto done;
    }

    /* 2. Parse */
    SearchRequest req;
    const char   *errmsg = NULL;
    if (!parse_request(reqbuf, &req, &errmsg)) {
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf), "{\"error\":\"%s\"}",
                 errmsg ? errmsg : "bad request");
        ws_send_text(sock, errbuf, strlen(errbuf));
        ws_send_close(sock, 1003);
        goto done;
    }

    /* 3. Stream matching seeds back, one frame per seed */
    WsSendCtx send_ctx;
    send_ctx.sock  = sock;
    send_ctx.error = 0;
    int64_t scanned = 0;
    search_seeds_stream(&req, ws_on_seed, &send_ctx, &scanned);

    /* 4. Send the done-summary frame */
    if (!send_ctx.error) {
        char done_buf[64];
        snprintf(done_buf, sizeof(done_buf),
                 "{\"done\":true,\"scanned\":%lld}", (long long)scanned);
        ws_send_text(sock, done_buf, strlen(done_buf));
    }

    ws_send_close(sock, 1000 /* normal closure */);

done:
    MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
}

/* ══════════════════════════════════════════════════════════════════════════
 * HTTP response helper
 * ══════════════════════════════════════════════════════════════════════════ */

static enum MHD_Result send_response(struct MHD_Connection *conn,
                                      unsigned int           status,
                                      const char            *body)
{
    struct MHD_Response *resp =
        MHD_create_response_from_buffer(strlen(body),
                                        (void *)body,
                                        MHD_RESPMEM_MUST_COPY);
    if (!resp) return MHD_NO;
    MHD_add_response_header(resp, "Content-Type", "application/json");
    enum MHD_Result ret = MHD_queue_response(conn, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Per-connection POST body accumulation state
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char  *data;
    size_t size;
} PostBuffer;

/* ══════════════════════════════════════════════════════════════════════════
 * Main request router
 * ══════════════════════════════════════════════════════════════════════════ */

enum MHD_Result handle_request(void *cls,
                                struct MHD_Connection *connection,
                                const char            *url,
                                const char            *method,
                                const char            *version,
                                const char            *upload_data,
                                size_t                *upload_data_size,
                                void                 **con_cls)
{
    (void)version;
    RateLimiter *rl = (RateLimiter *)cls;

    /* ── First call per request ─────────────────────────────────────────── */
    if (*con_cls == NULL) {

        /* Rate-limit all incoming requests */
        if (rl) {
            char ip[48] = "unknown";
            get_client_ip(connection, ip, sizeof(ip));
            if (!rate_limiter_check(rl, ip)) {
                return send_response(connection,
                    MHD_HTTP_TOO_MANY_REQUESTS,
                    "{\"error\":\"rate limit exceeded, try again later\"}");
            }
        }

        /* ── GET /structures ─────────────────────────────────────────────── */
        if (strcmp(url, "/structures") == 0) {
            if (strcmp(method, "GET") != 0)
                return send_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                                     "{\"error\":\"use GET\"}");
            char *body = build_structures_json();
            if (!body)
                return send_response(connection,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"error\":\"out of memory\"}");
            enum MHD_Result r = send_response(connection, MHD_HTTP_OK, body);
            free(body);
            return r;
        }

        /* ── GET /biomes ─────────────────────────────────────────────────── */
        if (strcmp(url, "/biomes") == 0) {
            if (strcmp(method, "GET") != 0)
                return send_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                                     "{\"error\":\"use GET\"}");
            char *body = build_biomes_json();
            if (!body)
                return send_response(connection,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"error\":\"out of memory\"}");
            enum MHD_Result r = send_response(connection, MHD_HTTP_OK, body);
            free(body);
            return r;
        }

        /* ── GET /search/stream  (WebSocket upgrade) ─────────────────────── */
        if (strcmp(url, "/search/stream") == 0) {
            if (strcmp(method, "GET") != 0)
                return send_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                    "{\"error\":\"use GET with a WebSocket upgrade\"}");

            const char *ws_key = MHD_lookup_connection_value(
                connection, MHD_HEADER_KIND, "Sec-WebSocket-Key");
            if (!ws_key)
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                    "{\"error\":\"missing Sec-WebSocket-Key header\"}");

            char accept[64];
            ws_compute_accept(ws_key, accept, sizeof(accept));

            struct MHD_Response *resp =
                MHD_create_response_for_upgrade(ws_upgrade_handler, NULL);
            if (!resp) return MHD_NO;
            MHD_add_response_header(resp, "Upgrade",              "websocket");
            MHD_add_response_header(resp, "Connection",           "Upgrade");
            MHD_add_response_header(resp, "Sec-WebSocket-Accept", accept);
            enum MHD_Result r = MHD_queue_response(
                connection, MHD_HTTP_SWITCHING_PROTOCOLS, resp);
            MHD_destroy_response(resp);
            return r;
        }

        /* ── POST /search ────────────────────────────────────────────────── */
        if (strcmp(url, "/search") == 0) {
            if (strcmp(method, "POST") != 0)
                return send_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                                     "{\"error\":\"use POST\"}");
            PostBuffer *pb = (PostBuffer *)calloc(1, sizeof(PostBuffer));
            if (!pb) return MHD_NO;
            *con_cls = pb;
            return MHD_YES;
        }

        return send_response(connection, MHD_HTTP_NOT_FOUND,
                             "{\"error\":\"not found\"}");
    }

    /* ── Subsequent calls: accumulate POST /search body ─────────────────── */
    PostBuffer *pb = (PostBuffer *)(*con_cls);

    if (*upload_data_size > 0) {
        char *tmp = (char *)realloc(pb->data,
                                    pb->size + *upload_data_size + 1);
        if (!tmp) return MHD_NO;
        pb->data = tmp;
        memcpy(pb->data + pb->size, upload_data, *upload_data_size);
        pb->size += *upload_data_size;
        pb->data[pb->size] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }

    /* ── Body complete: process POST /search ─────────────────────────────── */
    if (!pb->data || pb->size == 0)
        return send_response(connection, MHD_HTTP_BAD_REQUEST,
                             "{\"error\":\"empty body\"}");

    SearchRequest req;
    const char   *errmsg = NULL;
    if (!parse_request(pb->data, &req, &errmsg)) {
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf), "{\"error\":\"%s\"}",
                 errmsg ? errmsg : "bad request");
        return send_response(connection, MHD_HTTP_BAD_REQUEST, errbuf);
    }

    SearchResult result;
    memset(&result, 0, sizeof(result));
    search_seeds(&req, &result);

    char *resp_body = format_response(&result);
    if (!resp_body)
        return send_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
                             "{\"error\":\"out of memory\"}");

    enum MHD_Result ret = send_response(connection, MHD_HTTP_OK, resp_body);
    free(resp_body);
    return ret;
}

void cleanup_request(void *cls, struct MHD_Connection *connection,
                     void **con_cls, enum MHD_RequestTerminationCode toe)
{
    (void)cls; (void)connection; (void)toe;
    PostBuffer *pb = (PostBuffer *)(*con_cls);
    if (pb) {
        free(pb->data);
        free(pb);
        *con_cls = NULL;
    }
}
