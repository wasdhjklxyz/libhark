#include <errno.h>
#include <hark/reactor.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "internal.h" /* IWYU pragma: keep */

static void hark__sig_handler(hark_reactor_t *r, int fd, uint32_t events,
                              void *ctx) {
  (void)r;
  (void)events;

  ssize_t n;
  struct signalfd_siginfo si;
  hark_sig_t *s = (hark_sig_t *)ctx;

  do {
    n = read(fd, &si, sizeof(si));
  } while (n == -1 && errno == EINTR);

  if (n != (ssize_t)sizeof(si))
    return;

  if (s->cb)
    s->cb(s, (int)si.ssi_signo, s->ctx);
}

hark_sig_t *hark_sig_create(hark_reactor_t *r, hark_sig_fn cb, void *ctx) {
  if (!r || !cb)
    return NULL;

  hark_sig_t *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;

  s->reactor = r;
  s->cb = cb;
  s->ctx = ctx;
  s->fd = -1;

  if (sigemptyset(&s->mask) < 0)
    goto free_s;

  s->fd = signalfd(-1, &s->mask, SFD_NONBLOCK | SFD_CLOEXEC);
  if (s->fd < 0)
    goto free_s;

  hark_err_t err =
      hark_reactor_add(r, s->fd, HARK_EV_READ, hark__sig_handler, s);
  if (err != HARK_OK)
    goto close_fd;

  return s;

close_fd:
  close(s->fd);
free_s:
  free(s);
  return NULL;
}

hark_err_t hark_sig_add(hark_sig_t *s, int signo) {
  if (!s || signo <= 0)
    return HARK_ERR_BADARG;

  if (sigaddset(&s->mask, signo) < 0)
    return HARK_ERR_SYSCALL;

  if (pthread_sigmask(SIG_BLOCK, &s->mask, NULL) != 0)
    return HARK_ERR_SYSCALL;

  /* update signalfd with new mask in place */
  if (signalfd(s->fd, &s->mask, SFD_NONBLOCK | SFD_CLOEXEC) < 0)
    return HARK_ERR_SYSCALL;

  return HARK_OK;
}

void hark_sig_destroy(hark_sig_t *s) {
  if (!s)
    return;

  if (s->fd >= 0) {
    hark_reactor_del(s->reactor, s->fd);
    close(s->fd);
  }

  pthread_sigmask(SIG_UNBLOCK, &s->mask, NULL);
  explicit_bzero(s, sizeof(*s));
  free(s);
}

HARK_API hark_reactor_t *hark_sig_reactor(hark_sig_t *c) {
  if (!c)
    return NULL;
  return c->reactor;
}
