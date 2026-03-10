#include <hark/connector.h>
#include <hark/reactor.h>
#include <hark/timer.h>
#include <hark/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "internal.h" /* IWYU pragma: keep */

#define HARK_CONN_BACKOFF_DEFAULT_MS 1000
#define HARK_CONN_BACKOFF_DEFAULT_MAX_MS 30000
#define HARK_CONN_BACKOFF_DEFAULT_EXP 1

static void hark__conn_on_event(hark_reactor_t *r, int fd, uint32_t events,
                                void *ctx) {
  hark_conn_t *c = ctx;

  if (c->state == HARK_CONN_CONNECTING) {
    if (events & (HARK_EV_ERROR | HARK_EV_HUP)) {
      hark_reactor_del(r, fd);
      if (c->hooks.close)
        c->hooks.close(c->ctx, fd);
      c->fd = -1;
      c->state = HARK_CONN_DISCONNECTED;
      hark_timer_set(c->reconnect_timer, c->backoff_ms.cur);
      return;
    }

    if (events & HARK_EV_WRITE) {
      c->state = HARK_CONN_CONNECTED;
      c->attempt = 0;
      c->backoff_ms.cur = c->backoff_ms.def;
      hark_reactor_mod(r, fd, HARK_EV_READ | HARK_EV_ERROR | HARK_EV_HUP);
      if (c->hooks.on_connect)
        c->hooks.on_connect(c->ctx, fd);
    }
    return;
  }

  if (events & (HARK_EV_ERROR | HARK_EV_HUP)) {
    hark_reactor_del(r, fd);
    if (c->hooks.on_disconnect)
      c->hooks.on_disconnect(c->ctx, errno);
    if (c->hooks.close)
      c->hooks.close(c->ctx, fd);
    c->fd = -1;
    c->state = HARK_CONN_DISCONNECTED;
    hark_timer_set(c->reconnect_timer, c->backoff_ms.cur);
    return;
  }

  if (events & HARK_EV_READ) {
    if (c->hooks.on_data)
      c->hooks.on_data(c->ctx, fd);
  }
}

static void hark__conn_reconnect_tick(hark_timer_t *t, void *ctx) {
  hark_conn_t *c = ctx;
  uint64_t delay = c->backoff_ms.cur;
  hark_err_t err = HARK_OK;

  c->attempt++;

  if (c->hooks.on_reconnect) {
    err = c->hooks.on_reconnect(c->ctx, c->attempt, &delay);
    if (err != HARK_OK)
      return;                  /* user said give up */
    c->backoff_ms.cur = delay; /* user may have overridden */
  }

  hark_conn_open(c);

  if (c->backoff_ms.exp) {
    c->backoff_ms.cur *= 2;
    if (c->backoff_ms.cur > c->backoff_ms.max)
      c->backoff_ms.cur = c->backoff_ms.max;
  } else {
    c->backoff_ms.cur += c->backoff_ms.def;
    if (c->backoff_ms.cur > c->backoff_ms.max)
      c->backoff_ms.cur = c->backoff_ms.max;
  }
}

HARK_API hark_conn_t *hark_conn_create(hark_reactor_t *r, void *ctx) {
  hark_conn_t *c = NULL;

  if (!r) {
    errno = EINVAL;
    return NULL;
  }

  c = calloc(1, sizeof(*c));
  if (!c)
    return NULL;

  c->reconnect_timer = hark__timer_alloc(r, hark__conn_reconnect_tick, c, 1);
  if (!c->reconnect_timer) {
    free(c);
    return NULL;
  }

  c->fd = -1;
  c->reactor = r;
  c->ctx = ctx;
  c->state = HARK_CONN_DISCONNECTED;
  c->attempt = 0;

  c->backoff_ms.def = HARK_CONN_BACKOFF_DEFAULT_MS;
  c->backoff_ms.cur = HARK_CONN_BACKOFF_DEFAULT_MS;
  c->backoff_ms.max = HARK_CONN_BACKOFF_DEFAULT_MAX_MS;
  c->backoff_ms.exp = HARK_CONN_BACKOFF_DEFAULT_EXP;

  return c;
}

HARK_API hark_err_t hark_conn_set_backoff(hark_conn_t *c, uint64_t backoff_ms,
                                          uint64_t backoff_max_ms,
                                          int exponential) {
  if (!c || backoff_ms == 0 || backoff_max_ms < backoff_ms)
    return HARK_ERR_BADARG;

  c->backoff_ms.def = backoff_ms;
  c->backoff_ms.cur = backoff_ms;
  c->backoff_ms.max = backoff_max_ms;
  c->backoff_ms.exp = exponential;
  return HARK_OK;
}

HARK_API hark_err_t hark_conn_open(hark_conn_t *c) {
  int fd = -1;
  int ret = 0;
  hark_err_t err = HARK_OK;

  if (!c)
    return HARK_ERR_BADARG;
  if (c->state != HARK_CONN_DISCONNECTED)
    return HARK_ERR_STATE;
  if (!c->hooks.open)
    return HARK_ERR_INVAL;

  ret = c->hooks.open(c->ctx, &fd);

  if (ret < 0 || fd < 0) {
    c->state = HARK_CONN_DISCONNECTED;
    hark_timer_set(c->reconnect_timer, c->backoff_ms.cur);
    return HARK_OK; /* not an error - reconnect will handle it */
  }

  c->fd = fd;

  if (ret == HARK_CONN_PENDING) {
    c->state = HARK_CONN_CONNECTING;
    return hark_reactor_add(c->reactor, fd,
                            HARK_EV_WRITE | HARK_EV_ERROR | HARK_EV_HUP,
                            hark__conn_on_event, c);
  }

  c->state = HARK_CONN_CONNECTED;
  c->attempt = 0;
  c->backoff_ms.cur = c->backoff_ms.def;

  err = hark_reactor_add(c->reactor, fd,
                         HARK_EV_READ | HARK_EV_ERROR | HARK_EV_HUP,
                         hark__conn_on_event, c);
  if (err != HARK_OK)
    return err;

  if (c->hooks.on_connect)
    c->hooks.on_connect(c->ctx, fd);

  return HARK_OK;
}

HARK_API hark_err_t hark_conn_close(hark_conn_t *c) {
  if (!c)
    return HARK_ERR_BADARG;

  hark_timer_cancel(c->reconnect_timer);

  if (c->fd >= 0) {
    hark_reactor_del(c->reactor, c->fd);
    if (c->hooks.close)
      c->hooks.close(c->ctx, c->fd);
    c->fd = -1;
  }

  c->state = HARK_CONN_DISCONNECTED;
  c->attempt = 0;
  c->backoff_ms.cur = c->backoff_ms.def;
  return HARK_OK;
}

HARK_API void hark_conn_destroy(hark_conn_t *c) {
  if (!c)
    return;

  hark_conn_close(c);

  if (c->reconnect_timer)
    hark_timer_destroy(c->reconnect_timer);

  explicit_bzero(c, sizeof(*c));
  free(c);
}

HARK_API hark_conn_state_t hark_conn_state(const hark_conn_t *c) {
  if (!c)
    return HARK_CONN_DISCONNECTED;
  return c->state;
}

HARK_API hark_err_t hark_conn_set_open_hook(hark_conn_t *conn,
                                            int (*open)(void *ctx, int *fd)) {

  if (!conn)
    return HARK_ERR_BADARG;
  conn->hooks.open = open;
  return HARK_OK;
}

HARK_API hark_err_t hark_conn_set_on_connect_hook(hark_conn_t *conn,
                                                  void (*on_connect)(void *ctx,
                                                                     int fd)) {
  if (!conn)
    return HARK_ERR_BADARG;
  conn->hooks.on_connect = on_connect;
  return HARK_OK;
}

HARK_API hark_err_t hark_conn_set_on_data_hook(hark_conn_t *conn,
                                               void (*on_data)(void *ctx,
                                                               int fd)) {
  if (!conn)
    return HARK_ERR_BADARG;
  conn->hooks.on_data = on_data;
  return HARK_OK;
}

HARK_API hark_err_t hark_conn_set_on_disconnect_hook(
    hark_conn_t *conn, void (*on_disconnect)(void *ctx, int reason)) {
  if (!conn)
    return HARK_ERR_BADARG;
  conn->hooks.on_disconnect = on_disconnect;

  return HARK_OK;
}

HARK_API hark_err_t hark_conn_set_on_reconnect_hook(
    hark_conn_t *conn,
    hark_err_t (*on_reconnect)(void *ctx, int attempt, uint64_t *delay_ms)) {
  if (!conn)
    return HARK_ERR_BADARG;
  conn->hooks.on_reconnect = on_reconnect;

  return HARK_OK;
}

HARK_API hark_err_t hark_conn_set_close_hook(hark_conn_t *conn,
                                             void (*close)(void *ctx, int fd)) {
  if (!conn)
    return HARK_ERR_BADARG;
  conn->hooks.close = close;
  return HARK_OK;
}
