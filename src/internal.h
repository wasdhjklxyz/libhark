/**
 * @file internal.h
 * @brief Private definitions - never installed, never included by consumers.
 */

#ifndef HARK_INTERNAL_H
#define HARK_INTERNAL_H

#include <hark/types.h>

#include <pthread.h>
#include <signal.h> // IWYU pragma: keep
#include <stdatomic.h>
#include <stdint.h>

/** Maximum epoll events per dispatch cycle. */
#define HARK_MAX_EVENTS 64

/** Default reconnect backoff in milliseconds. */
#define HARK_DEFAULT_BACKOFF_MS 1000

/** Maximum reconnect backoff in milliseconds. */
#define HARK_MAX_BACKOFF_MS 30000

/** @internal fd-to-handler mapping stored in the reactor's fd table. */
typedef struct hark__fd_entry {
  int fd;
  hark_fd_fn cb;
  void *ctx;
  uint32_t events;
} hark__fd_entry_t;

/** @internal Full reactor state. */
struct hark_reactor {
  int epoll_fd;
  int wake_fd;                /**< eventfd for cross-thread wakeup. */
  struct epoll_event *events; /**< epoll_wait result buffer. */
  int max_events;
  atomic_int running;         /**< Atomic stop flag. */
  hark__fd_entry_t *fd_table; /**< Array indexed by fd. */
  int fd_table_size;
};

/** @internal Full timer state. */
struct hark_timer {
  int timer_fd;
  hark_reactor_t *reactor;
  hark_timer_fn cb;
  void *ctx;
  uint64_t interval_ms;
  int oneshot;
};

/** @internal Full connector state. */
struct hark_conn {
  int fd; /**< Current fd, or -1. */
  hark_reactor_t *reactor;
  hark_timer_t *reconnect_timer;
  hark_conn_hooks_t hooks;
  void *ctx;
  hark_conn_state_t state;
  int attempt;
  struct {
    uint64_t def; /**< Initial / reset value. */
    uint64_t cur; /**< Current backoff delay. */
    uint64_t max; /**< Upper bound. */
    int exp;      /**< Non-zero for exponential, zero for linear. */
  } backoff_ms;
};

/** @internal Full signal state. */
struct hark_sig {
  int fd;
  hark_reactor_t *reactor;
  hark_sig_fn cb;
  void *ctx;
  sigset_t mask;
};

/**
 * @internal Timer epoll callback - reads the timerfd and dispatches user cb.
 */
void hark__timer_handler(hark_reactor_t *r, int fd, uint32_t events, void *ctx);

/**
 * @internal Allocate a timer and register with reactor, but do not arm it.
 * Used by the connector to pre-allocate a disarmed reconnect timer.
 */
hark_timer_t *hark__timer_alloc(hark_reactor_t *r, hark_timer_fn cb, void *ctx,
                                uint64_t ms, int oneshot);

#endif /* HARK_INTERNAL_H */
