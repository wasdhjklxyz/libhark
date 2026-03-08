/**
 * @file timer.h
 * @brief Timerfd wrapper registered into a hark reactor.
 *
 * Supports one-shot and recurring timers. Internally creates a timerfd
 * and registers it with the provided reactor. The reactor's epoll loop
 * dispatches the callback when the timer fires.
 */

#ifndef HARK_TIMER_H
#define HARK_TIMER_H

#include <hark/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a recurring timer.
 *
 * Allocates a timerfd, registers it with the reactor, and arms it.
 * The callback fires every @p interval_ms milliseconds until
 * cancelled or destroyed.
 *
 * @param r           Reactor to register with (must outlive the timer).
 * @param interval_ms Interval in milliseconds. Must be > 0.
 * @param cb          Callback invoked on each tick.
 * @param ctx         User data passed to @p cb.
 * @return New timer, or NULL on failure (check @c errno).
 */
HARK_API hark_timer_t *hark_timer_create(hark_reactor_t *r,
                                         uint64_t interval_ms, hark_timer_fn cb,
                                         void *ctx);

/**
 * @brief Create a one-shot timer.
 *
 * Fires once after @p delay_ms, then automatically disarms.
 * The timer remains allocated - call hark_timer_destroy() to free it,
 * or hark_timer_set() to rearm it.
 *
 * @param r        Reactor to register with.
 * @param delay_ms Delay in milliseconds before firing. Must be > 0.
 * @param cb       Callback invoked when the timer fires.
 * @param ctx      User data passed to @p cb.
 * @return New timer, or NULL on failure (check @c errno).
 */
HARK_API hark_timer_t *hark_timer_oneshot(hark_reactor_t *r, uint64_t delay_ms,
                                          hark_timer_fn cb, void *ctx);

/**
 * @brief Change the interval of an existing timer.
 *
 * Takes effect immediately. Can rearm a disarmed or cancelled timer.
 *
 * @param t           Timer to modify.
 * @param interval_ms New interval in milliseconds. Must be > 0.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p t is NULL or @p interval_ms is 0.
 * @retval HARK_ERR_SYSCALL timerfd_settime() failed; check @c errno.
 */
HARK_API hark_err_t hark_timer_set(hark_timer_t *t, uint64_t interval_ms);

/**
 * @brief Disarm the timer.
 *
 * Stops the timer from firing but does not destroy it. The timerfd
 * remains registered with the reactor. Call hark_timer_set() to rearm.
 *
 * @param t Timer to cancel.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p t is NULL.
 * @retval HARK_ERR_SYSCALL timerfd_settime() failed; check @c errno.
 */
HARK_API hark_err_t hark_timer_cancel(hark_timer_t *t);

/**
 * @brief Destroy the timer.
 *
 * Removes the timerfd from the reactor, closes it, and frees memory.
 * Safe to call with NULL (no-op).
 *
 * @param t Timer to destroy, or NULL.
 */
HARK_API void hark_timer_destroy(hark_timer_t *t);

#ifdef __cplusplus
}
#endif

#endif /* HARK_TIMER_H */
