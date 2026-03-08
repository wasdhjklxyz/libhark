/**
 * @file reactor.h
 * @brief Epoll-based file descriptor event loop.
 *
 * Each reactor owns its own epoll instance and runs on a single thread.
 * For multi-threaded use, create one reactor per thread and use
 * hark_reactor_wakeup() to signal across threads (backed by eventfd).
 */

#ifndef HARK_REACTOR_H
#define HARK_REACTOR_H

#include <hark/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new reactor.
 *
 * Allocates an epoll instance, an eventfd for cross-thread wakeup,
 * and an internal event buffer.
 *
 * @return New reactor, or NULL on failure (check @c errno).
 */
HARK_API hark_reactor_t *hark_reactor_create(void);

/**
 * @brief Destroy a reactor.
 *
 * If the reactor is running on a background thread, stops it and joins.
 * Closes the epoll and eventfd, frees all internal state. Safe to call
 * with NULL (no-op).
 *
 * @param r Reactor to destroy, or NULL.
 */
HARK_API void hark_reactor_destroy(hark_reactor_t *r);

/**
 * @brief Register a file descriptor with the reactor.
 *
 * @param r      Reactor instance.
 * @param fd     File descriptor (must be valid and open).
 * @param events Bitmask of @ref hark_events_t to watch for.
 * @param cb     Callback invoked when events fire.
 * @param ctx    User data passed to @p cb.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p r is NULL, @p fd < 0, or @p cb is NULL.
 * @retval HARK_ERR_BADFD   @p fd is not a valid open file descriptor.
 * @retval HARK_ERR_NOMEM   Internal table growth failed.
 * @retval HARK_ERR_SYSCALL epoll_ctl() failed; check @c errno.
 */
HARK_API hark_err_t hark_reactor_add(hark_reactor_t *r, int fd, uint32_t events,
                                     hark_fd_fn cb, void *ctx);

/**
 * @brief Modify watched events for an already-registered fd.
 *
 * @param r      Reactor instance.
 * @param fd     Registered file descriptor.
 * @param events New bitmask of @ref hark_events_t.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p r is NULL or @p fd < 0.
 * @retval HARK_ERR_BADFD   @p fd is not valid.
 * @retval HARK_ERR_SYSCALL epoll_ctl() failed; check @c errno.
 */
HARK_API hark_err_t hark_reactor_mod(hark_reactor_t *r, int fd,
                                     uint32_t events);

/**
 * @brief Remove a file descriptor from the reactor.
 *
 * Does not close the fd.
 *
 * @param r  Reactor instance.
 * @param fd File descriptor to remove.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p r is NULL or @p fd < 0.
 * @retval HARK_ERR_BADFD   @p fd is not registered.
 * @retval HARK_ERR_SYSCALL epoll_ctl() failed; check @c errno.
 */
HARK_API hark_err_t hark_reactor_del(hark_reactor_t *r, int fd);

/**
 * @brief Run the event loop.
 *
 * Blocks until hark_reactor_stop() is called or an unrecoverable
 * error occurs. Dispatches callbacks for all registered fds.
 *
 * @param r Reactor instance.
 * @return @ref HARK_OK on clean shutdown.
 * @retval HARK_ERR_BADARG  @p r is NULL.
 * @retval HARK_ERR_SYSCALL epoll_wait() failed; check @c errno.
 */
HARK_API hark_err_t hark_reactor_run(hark_reactor_t *r);

/**
 * @brief Signal the reactor to stop.
 *
 * The reactor will exit after completing the current dispatch cycle.
 * Thread-safe.
 *
 * @param r Reactor instance.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p r is NULL.
 */
HARK_API hark_err_t hark_reactor_stop(hark_reactor_t *r);

/**
 * @brief Wake a reactor blocked in epoll_wait from another thread.
 *
 * Writes to the internal eventfd. Thread-safe. Idempotent - calling
 * when already awake is harmless.
 *
 * @param r Reactor instance.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p r is NULL.
 * @retval HARK_ERR_SYSCALL write() to eventfd failed; check @c errno.
 */
HARK_API hark_err_t hark_reactor_wakeup(hark_reactor_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HARK_REACTOR_H */
