/**
 * @file echo_server.c
 * @brief TCP echo server using hark reactor + timers + signal handling.
 *
 * Listens on port 9000, echoes back anything received.
 * A 5-second recurring timer prints stats.
 * Ctrl-C (SIGINT) or SIGTERM shuts down cleanly via hark_sig_t.
 *
 * Usage: ./echo_server
 */

#include <hark/hark.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9000
#define BACKLOG 16

typedef struct {
  hark_reactor_t *reactor;
  int connections;
  int total_bytes;
} server_ctx_t;

static void on_client(hark_reactor_t *r, int fd, uint32_t events, void *ctx) {
  server_ctx_t *srv = ctx;
  uint8_t buf[4096];

  if (events & (HARK_EV_ERROR | HARK_EV_HUP)) {
    printf("[echo] client fd=%d disconnected\n", fd);
    hark_reactor_del(r, fd);
    close(fd);
    srv->connections--;
    return;
  }

  if (events & HARK_EV_READ) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
      printf("[echo] client fd=%d closed\n", fd);
      hark_reactor_del(r, fd);
      close(fd);
      srv->connections--;
      return;
    }

    /* echo it back */
    (void)write(fd, buf, (size_t)n);
    srv->total_bytes += (int)n;
  }
}

static void on_accept(hark_reactor_t *r, int fd, uint32_t events, void *ctx) {
  server_ctx_t *srv = ctx;
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);

  int client = accept(fd, (struct sockaddr *)&addr, &len);
  if (client < 0) {
    perror("accept");
    return;
  }

  printf("[echo] new client fd=%d from %s:%d\n", client,
         inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

  hark_reactor_add(r, client, HARK_EV_READ | HARK_EV_HUP, on_client, srv);
  srv->connections++;
}

static void on_stats(hark_timer_t *t, void *ctx) {
  server_ctx_t *srv = ctx;
  printf("[stats] connections=%d total_bytes=%d\n", srv->connections,
         srv->total_bytes);
}

static void on_signal(hark_sig_t *s, int signo, void *ctx) {
  server_ctx_t *srv = ctx;
  printf("\n[echo] caught signal %d (%s), shutting down\n", signo,
         signo == SIGINT ? "SIGINT" : "SIGTERM");
  hark_reactor_stop(srv->reactor);
}

int main(void) {
  server_ctx_t srv = {0};

  hark_reactor_t *r = hark_reactor_create();
  if (!r) {
    perror("hark_reactor_create");
    return 1;
  }
  srv.reactor = r;

  int listen_fd =
      socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr = {.s_addr = INADDR_ANY},
  };

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(listen_fd, BACKLOG) < 0) {
    perror("listen");
    return 1;
  }

  hark_reactor_add(r, listen_fd, HARK_EV_READ, on_accept, &srv);
  printf("[echo] listening on port %d\n", PORT);

  hark_timer_t *stats = hark_timer_create(r, 5000, on_stats, &srv);
  if (!stats) {
    perror("hark_timer_create");
    return 1;
  }

  hark_sig_t *sig = hark_sig_create(r, on_signal, &srv);
  if (!sig) {
    perror("hark_sig_create");
    return 1;
  }
  hark_sig_add(sig, SIGINT);
  hark_sig_add(sig, SIGTERM);

  hark_err_t err = hark_reactor_run(r);
  if (err != HARK_OK)
    fprintf(stderr, "[echo] reactor exited: %s\n", hark_strerror(err));

  hark_sig_destroy(sig);
  hark_timer_destroy(stats);
  close(listen_fd);
  hark_reactor_destroy(r);

  printf("[echo] goodbye\n");
  return 0;
}
