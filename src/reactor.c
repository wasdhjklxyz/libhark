#include <hark/reactor.h>
#include <hark/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

#include "internal.h" /* IWYU pragma: keep */

#define HARK_MAX_EVENTS 64
#define HARK_REACTOR_TABLE_START_SIZE 64

static hark_err_t hark__reactor_ensure_fd(hark_reactor_t *r, int fd) {
  int new_size = 0;
  hark__fd_entry_t *new_table = NULL;

  if (fd < r->fd_table_size)
    return HARK_OK;

  new_size = r->fd_table_size;
  if (new_size == 0)
    new_size = HARK_REACTOR_TABLE_START_SIZE;
  while (new_size <= fd)
    new_size *= 2;

  new_table = realloc(r->fd_table, (size_t)new_size * sizeof(*new_table));
  if (!new_table)
    return HARK_ERR_NOMEM;

  memset(new_table + r->fd_table_size, 0,
         (size_t)(new_size - r->fd_table_size) * sizeof(*new_table));

  r->fd_table = new_table;
  r->fd_table_size = new_size;
  return HARK_OK;
}

static uint32_t hark__map_events(uint32_t events) {
  uint32_t ret = 0;
  if (events & HARK_EV_READ)
    ret |= EPOLLIN;
  if (events & HARK_EV_WRITE)
    ret |= EPOLLOUT;
  if (events & HARK_EV_ERROR)
    ret |= EPOLLERR;
  if (events & HARK_EV_HUP)
    ret |= EPOLLHUP;
  return ret;
}

static uint32_t hark__map_epoll_events(uint32_t events) {
  uint32_t ret = 0;
  if (events & EPOLLIN)
    ret |= HARK_EV_READ;
  if (events & EPOLLOUT)
    ret |= HARK_EV_WRITE;
  if (events & EPOLLERR)
    ret |= HARK_EV_ERROR;
  if (events & EPOLLHUP)
    ret |= HARK_EV_HUP;
  return ret;
}

HARK_API hark_reactor_t *hark_reactor_create(void) {
  hark_reactor_t *r = NULL;
  struct epoll_event ev = {.events = EPOLLIN};
  int saved = 0;

  r = calloc(1, sizeof(*r));
  if (!r)
    return NULL;

  r->epoll_fd = -1;
  r->wake_fd = -1;

  r->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (r->epoll_fd == -1)
    goto fail;

  r->wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (r->wake_fd == -1)
    goto fail;

  r->events = calloc(HARK_MAX_EVENTS, sizeof(*r->events));
  if (!r->events)
    goto fail;

  ev.data.ptr = NULL; /* sential: NULL ptr = wakeup fd */
  if (epoll_ctl(r->epoll_fd, EPOLL_CTL_ADD, r->wake_fd, &ev) == -1)
    goto fail;

  r->max_events = HARK_MAX_EVENTS;
  atomic_init(&r->running, 0);
  r->fd_table = NULL;
  r->fd_table_size = 0;

  return r;

fail:;
  saved = errno;
  if (r->wake_fd != -1)
    close(r->wake_fd);
  if (r->epoll_fd != -1)
    close(r->epoll_fd);
  free(r->events);
  free(r);
  errno = saved;
  return NULL;
}

HARK_API void hark_reactor_destroy(hark_reactor_t *r) {
  if (!r)
    return;

  if (r->fd_table) {
    explicit_bzero(r->fd_table,
                   (size_t)r->fd_table_size * sizeof(*r->fd_table));
    free(r->fd_table);
  }

  if (r->events) {
    explicit_bzero(r->events, (size_t)r->max_events * sizeof(*r->events));
    free(r->events);
  }

  if (r->wake_fd >= 0)
    close(r->wake_fd);
  if (r->epoll_fd >= 0)
    close(r->epoll_fd);

  explicit_bzero(r, sizeof(*r));
  free(r);
}

HARK_API hark_err_t hark_reactor_add(hark_reactor_t *r, int fd, uint32_t events,
                                     hark_fd_fn cb, void *ctx) {
  struct epoll_event ev = {0};
  hark_err_t err = HARK_OK;

  if (!r || fd < 0 || !cb)
    return HARK_ERR_BADARG;

  if (fcntl(fd, F_GETFD) == -1)
    return HARK_ERR_BADFD;

  err = hark__reactor_ensure_fd(r, fd);
  if (err != HARK_OK)
    return err;

  r->fd_table[fd].fd = fd;
  r->fd_table[fd].cb = cb;
  r->fd_table[fd].ctx = ctx;
  r->fd_table[fd].events = events;

  ev = (struct epoll_event){
      .events = hark__map_events(events),
      .data = {.ptr = &r->fd_table[fd]},
  };
  if (epoll_ctl(r->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
    return HARK_ERR_SYSCALL;

  return HARK_OK;
}

HARK_API hark_err_t hark_reactor_mod(hark_reactor_t *r, int fd,
                                     uint32_t events) {
  struct epoll_event ev = {0};

  if (!r || fd < 0)
    return HARK_ERR_BADARG;

  if (fcntl(fd, F_GETFD) == -1)
    return HARK_ERR_BADFD;

  ev = (struct epoll_event){
      .events = hark__map_events(events),
      .data = {.ptr = &r->fd_table[fd]},
  };
  if (epoll_ctl(r->epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1)
    return HARK_ERR_SYSCALL;

  return HARK_OK;
}

HARK_API hark_err_t hark_reactor_del(hark_reactor_t *r, int fd) {
  if (!r || fd < 0)
    return HARK_ERR_BADARG;

  if (fd >= r->fd_table_size || !r->fd_table[fd].cb)
    return HARK_ERR_BADFD;

  if (epoll_ctl(r->epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
    return HARK_ERR_SYSCALL;

  explicit_bzero(&r->fd_table[fd], sizeof(*r->fd_table));
  return HARK_OK;
}

HARK_API hark_err_t hark_reactor_run(hark_reactor_t *r) {
  int n = -1, i = 0;
  uint64_t val = 0;
  uint32_t hark_ev = 0;
  struct epoll_event *ev = NULL;
  hark__fd_entry_t *entry = NULL;

  if (!r)
    return HARK_ERR_BADARG;

  atomic_store(&r->running, 1);

  while (atomic_load(&r->running)) {
    n = epoll_wait(r->epoll_fd, r->events, r->max_events, -1);

    if (n == -1) {
      if (errno == EINTR)
        continue; /* signal interrupted */
      return HARK_ERR_SYSCALL;
    }

    for (i = 0; i < n; i++) {
      ev = &r->events[i];

      if (!ev->data.ptr) { /* wakeup fd */
        (void)read(r->wake_fd, &val, sizeof(val));
        continue;
      }

      entry = ev->data.ptr;
      if (entry->cb) {
        hark_ev = hark__map_epoll_events(ev->events);
        entry->cb(r, entry->fd, hark_ev, entry->ctx);
      }
    }
  }

  return HARK_OK;
}

HARK_API hark_err_t hark_reactor_stop(hark_reactor_t *r) {
  if (!r)
    return HARK_ERR_BADARG;

  atomic_store(&r->running, 0);
  return hark_reactor_wakeup(r);
}

HARK_API hark_err_t hark_reactor_wakeup(hark_reactor_t *r) {
  if (!r)
    return HARK_ERR_BADARG;

  uint64_t val = 1;
  if (write(r->wake_fd, &val, sizeof(val)) == -1) {
    if (errno == EAGAIN)
      return HARK_OK; /* already pending, that's fine */
    return HARK_ERR_SYSCALL;
  }
  return HARK_OK;
}
