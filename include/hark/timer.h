/**
 * @file timer.h
 * @brief Timerfd wrapper registered into a hark reactor.
 *
 * Supports one-shot and recurring timers. Internally creates a timerfd
 * and registers it with the provided reactor. The reactor's epoll loop
 * dispatches the callback when the timer fires.
 *
 * Timers are created disarmed. Call hark_timer_arm() to start them.
 */

#ifndef HARK_TIMER_H
#define HARK_TIMER_H

#include <hark/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a recurring timer (disarmed).
 *
 * Allocates a timerfd and registers it with the reactor.
 * The timer does not fire until hark_timer_arm() is called.
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
 * @brief Create a one-shot timer (disarmed).
 *
 * Fires once after the configured delay, then automatically disarms.
 * Call hark_timer_arm() to start or restart it.
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
 * @brief Arm (start) the timer with its configured interval.
 *
 * If already armed, resets the timer from the beginning of its interval.
 *
 * @param t Timer to arm.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p t is NULL.
 * @retval HARK_ERR_INVAL   Interval is 0.
 * @retval HARK_ERR_SYSCALL timerfd_settime() failed; check @c errno.
 */
HARK_API hark_err_t hark_timer_arm(hark_timer_t *t);

/**
 * @brief Disarm (stop) the timer without destroying it.
 *
 * The timerfd remains registered with the reactor.
 * Call hark_timer_arm() to restart.
 *
 * @param t Timer to disarm.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p t is NULL.
 * @retval HARK_ERR_SYSCALL timerfd_settime() failed; check @c errno.
 */
HARK_API hark_err_t hark_timer_disarm(hark_timer_t *t);

/**
 * @brief Change the timer's interval.
 *
 * Does NOT arm or disarm the timer. If the timer is currently armed,
 * the new interval takes effect on the next arm() call. If you want
 * the change to take effect immediately, call arm() after this.
 *
 * @param t           Timer to modify.
 * @param interval_ms New interval in milliseconds. Must be > 0.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG  @p t is NULL or @p interval_ms is 0.
 */
HARK_API hark_err_t hark_timer_set_interval(hark_timer_t *t,
                                            uint64_t interval_ms);

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
