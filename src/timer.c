#include <hark/reactor.h>
#include <hark/timer.h>
#include <hark/types.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "internal.h" /* IWYU pragma: keep */

void hark__timer_handler(hark_reactor_t *r, int fd, uint32_t events,
                         void *ctx) {
  hark_timer_t *t = ctx;
  uint64_t expirations;

  (void)r;
  (void)events;

  if (read(fd, &expirations, sizeof(expirations)) < 0)
    return; /* EAGAIN or error, either way nothing to do */

  if (t->oneshot)
    hark_timer_disarm(t);

  if (t->cb)
    t->cb(t, t->ctx);
}

hark_timer_t *hark__timer_alloc(hark_reactor_t *r, hark_timer_fn cb, void *ctx,
                                uint64_t ms, int oneshot) {
  hark_timer_t *t = NULL;

  if (!r || !cb || ms == 0) {
    errno = EINVAL;
    return NULL;
  }

  t = calloc(1, sizeof(*t));
  if (!t)
    return NULL;

  t->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (t->timer_fd == -1) {
    free(t);
    return NULL;
  }

  t->reactor = r;
  t->cb = cb;
  t->ctx = ctx;
  t->oneshot = oneshot;
  t->interval_ms = ms;

  if (hark_reactor_add(r, t->timer_fd, HARK_EV_READ, hark__timer_handler, t) !=
      HARK_OK) {
    int saved = errno;
    close(t->timer_fd);
    free(t);
    errno = saved;
    return NULL;
  }

  return t;
}

HARK_API hark_timer_t *hark_timer_create(hark_reactor_t *r,
                                         uint64_t interval_ms, hark_timer_fn cb,
                                         void *ctx) {
  return hark__timer_alloc(r, cb, ctx, interval_ms, false);
}

HARK_API hark_timer_t *hark_timer_oneshot(hark_reactor_t *r,
                                          uint64_t interval_ms,
                                          hark_timer_fn cb, void *ctx) {
  return hark__timer_alloc(r, cb, ctx, interval_ms, true);
}

HARK_API hark_err_t hark_timer_arm(hark_timer_t *t) {
  struct itimerspec ts = {0};

  if (!t)
    return HARK_ERR_BADARG;

  if (t->interval_ms == 0)
    return HARK_ERR_INVAL;

  ts.it_value.tv_sec = (time_t)(t->interval_ms / 1000);
  ts.it_value.tv_nsec = (time_t)((t->interval_ms % 1000) * 1000000);

  if (!t->oneshot)
    ts.it_interval = ts.it_value;

  if (timerfd_settime(t->timer_fd, 0, &ts, NULL) == -1)
    return HARK_ERR_SYSCALL;

  return HARK_OK;
}

HARK_API hark_err_t hark_timer_disarm(hark_timer_t *t) {
  struct itimerspec ts = {0};

  if (!t)
    return HARK_ERR_BADARG;

  if (timerfd_settime(t->timer_fd, 0, &ts, NULL) == -1)
    return HARK_ERR_SYSCALL;

  return HARK_OK;
}

HARK_API hark_err_t hark_timer_set_interval(hark_timer_t *t,
                                            uint64_t interval_ms) {
  if (!t || interval_ms == 0)
    return HARK_ERR_BADARG;

  t->interval_ms = interval_ms;
  return HARK_OK;
}

HARK_API void hark_timer_destroy(hark_timer_t *t) {
  if (!t)
    return;

  if (t->timer_fd >= 0) {
    if (t->reactor)
      hark_reactor_del(t->reactor, t->timer_fd);
    close(t->timer_fd);
  }

  explicit_bzero(t, sizeof(*t));
  free(t);
}
