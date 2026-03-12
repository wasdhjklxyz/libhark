/**
 * @file signal.h
 * @brief signalfd wrapper registered into a hark reactor.
 *
 * Handles signalfd creation, signal masking, and reactor registration.
 * Signals are delivered as callbacks with the raw signo - interpretation
 * is left to the caller.
 *
 * @note SIGABRT is intentionally excluded from the API. Do not add it.
 */

#ifndef HARK_SIGNAL_H
#define HARK_SIGNAL_H

#include <hark/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a signal watcher.
 *
 * Allocates a signalfd, registers it with the reactor, and blocks
 * delivery of signals added via hark_sig_add() to the calling thread.
 * The callback fires whenever a watched signal is delivered.
 *
 * @param r   Reactor to register with (must outlive the signal watcher).
 * @param cb  Callback invoked when a watched signal fires.
 * @param ctx User data passed to @p cb.
 * @return New signal watcher, or NULL on failure (check @c errno).
 */
HARK_API hark_sig_t *hark_sig_create(hark_reactor_t *r, hark_sig_fn cb,
                                     void *ctx);

/**
 * @brief Watch an additional signal.
 *
 * Adds @p signo to the signalfd mask and blocks it via pthread_sigmask.
 * Safe to call before or after hark_sig_create() - the signalfd is
 * updated in place via signalfd() with the new mask.
 *
 * @param s     Signal watcher instance.
 * @param signo Signal number to watch (e.g. SIGINT, SIGTERM).
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p s is NULL or @p signo is invalid.
 * @retval HARK_ERR_SYSCALL sigaddset(), pthread_sigmask(), or signalfd()
 * failed; check @c errno.
 */
HARK_API hark_err_t hark_sig_add(hark_sig_t *s, int signo);

/**
 * @brief Destroy the signal watcher.
 *
 * Removes the signalfd from the reactor, unblocks all watched signals
 * via pthread_sigmask, closes the fd, and frees memory.
 * Safe to call with NULL (no-op).
 *
 * @param s Signal watcher to destroy, or NULL.
 */
HARK_API void hark_sig_destroy(hark_sig_t *s);

#ifdef __cplusplus
}
#endif

#endif /* HARK_SIGNAL_H */
