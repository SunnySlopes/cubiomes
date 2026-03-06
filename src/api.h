#ifndef API_H_
#define API_H_

#include <pthread.h>
#include <time.h>
#include <microhttpd.h>

/* ── rate limiter ─────────────────────────────────────────────────────────── */

#define RATE_LIMIT_WINDOW    60  /* sliding-window length in seconds          */
#define RATE_LIMIT_MAX_REQS  10  /* max requests per IP per window            */
#define RATE_TABLE_SIZE     256  /* hash-table slots for tracked IPs          */

typedef struct {
    char   ip[48];       /* client IP string (IPv4 or IPv6)        */
    int    count;        /* requests in the current window         */
    time_t window_start; /* unix timestamp when the window opened  */
} RateEntry;

typedef struct {
    RateEntry       entries[RATE_TABLE_SIZE];
    pthread_mutex_t mutex;
} RateLimiter;

void rate_limiter_init(RateLimiter *rl);
void rate_limiter_destroy(RateLimiter *rl);

/* Returns 1 if the request is allowed, 0 if the caller is rate-limited. */
int  rate_limiter_check(RateLimiter *rl, const char *ip);

/* ── MHD callbacks ────────────────────────────────────────────────────────── */

/*
 * MHD access-handler callback (see MHD_AccessHandlerCallback).
 * @cls must point to a RateLimiter.
 */
enum MHD_Result handle_request(void *cls,
                                struct MHD_Connection *connection,
                                const char            *url,
                                const char            *method,
                                const char            *version,
                                const char            *upload_data,
                                size_t                *upload_data_size,
                                void                 **con_cls);

/*
 * MHD request-completed callback.
 * Register this via MHD_OPTION_NOTIFY_COMPLETED.
 */
void cleanup_request(void *cls, struct MHD_Connection *connection,
                     void **con_cls, enum MHD_RequestTerminationCode toe);

#endif /* API_H_ */
