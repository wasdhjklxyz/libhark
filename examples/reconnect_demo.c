/**
 * @file reconnect_demo.c
 * @brief Demonstrates hark_conn with auto-reconnect and backoff.
 *
 * Connects to localhost:9000 (run echo_server first).
 * Sends a ping every 2 seconds. If the connection drops,
 * the connector auto-reconnects with exponential backoff.
 *
 * Usage: ./reconnect_demo
 *        (kill/restart echo_server to see reconnect behavior)
 */

#include <hark/hark.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#define TARGET_HOST "127.0.0.1"
#define TARGET_PORT 9000

typedef struct {
  hark_reactor_t *reactor;
  hark_conn_t *conn;
  hark_timer_t *ping_timer;
  int connected_fd; /* current fd when connected, -1 otherwise */
} client_ctx_t;

static int hook_open(void *ctx, int *fd) {
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (sock < 0)
    return -1;

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(TARGET_PORT),
  };
  inet_pton(AF_INET, TARGET_HOST, &addr.sin_addr);

  int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (ret == 0) {
    /* connected immediately (unlikely for TCP but possible on loopback) */
    *fd = sock;
    return HARK_CONN_READY;
  }

  if (errno == EINPROGRESS) {
    /* async connect - connector will watch for EPOLLOUT */
    *fd = sock;
    return HARK_CONN_PENDING;
  }

  /* actual error */
  close(sock);
  return -1;
}

static void hook_on_connect(void *ctx, int fd) {
  client_ctx_t *c = ctx;
  c->connected_fd = fd;
  printf("[demo] connected on fd=%d\n", fd);
}

static void hook_on_data(void *ctx, int fd) {
  uint8_t buf[1024];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    printf("[demo] recv: %s\n", (char *)buf);
  }
}

static void hook_on_disconnect(void *ctx, int reason) {
  client_ctx_t *c = ctx;
  c->connected_fd = -1;
  printf("[demo] disconnected (reason=%d), will reconnect...\n", reason);
}

static hark_err_t hook_on_reconnect(void *ctx, int attempt,
                                    uint64_t *delay_ms) {
  printf("[demo] reconnect attempt %d in %lums\n", attempt,
         (unsigned long)*delay_ms);

  if (attempt > 10) {
    printf("[demo] giving up after %d attempts\n", attempt);
    return HARK_ERR;
  }

  return HARK_OK;
}

static void hook_close(void *ctx, int fd) {
  client_ctx_t *c = ctx;
  c->connected_fd = -1;
  close(fd);
}

static void on_ping(hark_timer_t *t, void *ctx) {
  client_ctx_t *c = ctx;

  if (c->connected_fd < 0)
    return; /* not connected, skip */

  const char *msg = "ping";
  ssize_t n = write(c->connected_fd, msg, strlen(msg));
  if (n < 0)
    printf("[demo] ping failed: %s\n", strerror(errno));
  else
    printf("[demo] sent: %s\n", msg);
}

static void on_signal(hark_reactor_t *r, int fd, uint32_t events, void *ctx) {
  struct signalfd_siginfo info;
  (void)read(fd, &info, sizeof(info));
  printf("\n[demo] shutting down\n");
  hark_reactor_stop(r);
}

int main(void) {
  client_ctx_t ctx = {.connected_fd = -1};

  hark_reactor_t *r = hark_reactor_create();
  if (!r) {
    perror("hark_reactor_create");
    return 1;
  }
  ctx.reactor = r;

  hark_conn_t *conn = hark_conn_create(r, &ctx);
  if (!conn) {
    perror("hark_conn_create");
    return 1;
  }
  ctx.conn = conn;

  /** WARN: Unhandled returns */
  hark_conn_set_open_hook(conn, hook_open);
  hark_conn_set_on_connect_hook(conn, hook_on_connect);
  hark_conn_set_on_data_hook(conn, hook_on_data);
  hark_conn_set_on_disconnect_hook(conn, hook_on_disconnect);
  hark_conn_set_on_reconnect_hook(conn, hook_on_reconnect);
  hark_conn_set_close_hook(conn, hook_close);
  hark_conn_set_backoff(conn, 500, 10000, 1);

  hark_err_t err = hark_conn_open(conn);
  if (err != HARK_OK) {
    fprintf(stderr, "hark_conn_open: %s\n", hark_strerror(err));
    return 1;
  }

  hark_timer_t *ping = hark_timer_create(r, 2000, on_ping, &ctx);
  if (!ping) {
    perror("hark_timer_create");
    return 1;
  }
  ctx.ping_timer = ping;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  int sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  hark_reactor_add(r, sig_fd, HARK_EV_READ, on_signal, NULL);

  printf("[demo] starting (target %s:%d)\n", TARGET_HOST, TARGET_PORT);

  err = hark_reactor_run(r);
  if (err != HARK_OK)
    fprintf(stderr, "[demo] reactor: %s\n", hark_strerror(err));

  hark_timer_destroy(ping);
  hark_conn_destroy(conn);
  close(sig_fd);
  hark_reactor_destroy(r);

  printf("[demo] goodbye\n");
  return 0;
}
