/**
 * @file connector.h
 * @brief Protocol-agnostic connection manager with auto-reconnect.
 *
 * The connector manages the lifecycle of a file descriptor through a
 * state machine: DISCONNECTED -> CONNECTING -> CONNECTED -> DISCONNECTED.
 *
 * It is protocol-agnostic - you provide hooks that create, observe, and
 * tear down the connection. The connector handles reactor registration,
 * error detection, and reconnect with configurable backoff.
 */

#ifndef HARK_CONNECTOR_H
#define HARK_CONNECTOR_H

#include <hark/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a connector.
 *
 * Allocates the connector and a disarmed reconnect timer. The connector
 * starts in @ref HARK_CONN_DISCONNECTED state. Call hark_conn_open()
 * to initiate a connection.
 *
 * Default backoff: 1000ms initial, 30000ms max, exponential.
 * Override with hark_conn_set_backoff().
 *
 * @param r     Reactor to register fds with (must outlive the connector).
 * @param ctx   User data passed to all hooks.
 * @return New connector, or NULL on failure (check @c errno).
 */
HARK_API hark_conn_t *hark_conn_create(hark_reactor_t *r, void *ctx);

/**
 * @brief Configure reconnect backoff.
 *
 * @param c             Connector instance.
 * @param backoff_ms    Initial backoff delay in milliseconds. Must be > 0.
 * @param backoff_max_ms Maximum backoff delay. Must be >= @p backoff_ms.
 * @param exponential   If non-zero, double the delay each attempt.
 *                      If zero, increment linearly by @p backoff_ms.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL, @p backoff_ms is 0, or max < initial.
 */
HARK_API hark_err_t hark_conn_set_backoff(hark_conn_t *c, uint64_t backoff_ms,
                                          uint64_t backoff_max_ms,
                                          int exponential);

/**
 * @brief Initiate a connection.
 *
 * Calls the @c open hook to obtain a file descriptor. Depending on the
 * hook's return value:
 *
 * - @ref HARK_CONN_READY: state becomes @ref HARK_CONN_CONNECTED,
 *   fd is registered for read events, @c on_connect is called.
 * - @ref HARK_CONN_PENDING: state becomes @ref HARK_CONN_CONNECTING,
 *   fd is registered for write events (async connect completion).
 * - Failure (-1): the reconnect timer is armed automatically.
 *
 * @param c Connector instance.
 * @return @ref HARK_OK on success (including deferred reconnect).
 * @retval HARK_ERR_BADARG  @p c is NULL.
 * @retval HARK_ERR_STATE   In @ref HARK_CONN_CONNECTED state.
 * @retval HARK_ERR_BADARG  No @c open hook configured.
 */
HARK_API hark_err_t hark_conn_open(hark_conn_t *c);

/**
 * @brief Gracefully close the connection.
 *
 * Cancels any pending reconnect timer, removes the fd from the reactor,
 * calls the @c close hook, and resets state to @ref HARK_CONN_DISCONNECTED.
 * Does not trigger reconnect.
 *
 * @param c Connector instance.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL.
 */
HARK_API hark_err_t hark_conn_close(hark_conn_t *c);

/**
 * @brief Signal an unrecoverable connection failure and schedule reconnect.
 *
 * Closes the connection (removes fd from reactor, calls @c close hook) and
 * arms the reconnect backoff timer. Unlike hark_conn_close(), this does not
 * reset the backoff state or attempt counter - use this when the failure was
 * detected from within a callback (e.g. a failed read/write) rather than an
 * explicit teardown.
 *
 * The reconnect cycle proceeds identically to an epoll-detected error:
 * backoff timer fires → @c on_reconnect → @c open.
 *
 * Safe to call from within @c on_read or @c on_write hooks.
 *
 * @param c Connector instance.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL.
 */
HARK_API hark_err_t hark_conn_reset(hark_conn_t *c);

/**
 * @brief Signal that a @ref HARK_OPEN_READY connection is logically ready.
 *
 * For protocols where the transport is immediately usable (e.g. UDP) but
 * logical connectivity must be confirmed out-of-band (e.g. first MAVLink
 * heartbeat received), @ref HARK_OPEN_READY intentionally defers the backoff
 * reset and @c on_connect callback. Call this function once the protocol
 * confirms the remote peer is alive.
 *
 * Resets the attempt counter and backoff delay to their initial values, then
 * calls the @c on_connect hook.
 *
 * Has no effect and returns @ref HARK_ERR_STATE if the connector is not in
 * @ref HARK_CONN_CONNECTED state (e.g. called before @c open or after
 * disconnect).
 *
 * @param c Connector instance.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL.
 * @retval HARK_ERR_STATE  Not in @ref HARK_CONN_CONNECTED state.
 */
HARK_API hark_err_t hark_conn_ready(hark_conn_t *c);

/**
 * @brief Destroy the connector.
 *
 * Calls hark_conn_close() if connected, destroys the reconnect timer,
 * and frees all memory. Safe to call with NULL (no-op).
 *
 * @param c Connector to destroy, or NULL.
 */
HARK_API void hark_conn_destroy(hark_conn_t *c);

/**
 * @brief Query the current connection state.
 *
 * @param c Connector instance, or NULL.
 * @return Current @ref hark_conn_state_t. Returns @ref HARK_CONN_DISCONNECTED
 *         if @p c is NULL.
 */
HARK_API hark_conn_state_t hark_conn_state(const hark_conn_t *c);

/**
 * @brief Set the @ref hark_conn_hooks_t::open hook on @p c.
 *
 * @param c    Connector instance.
 * @param open See @ref hark_conn_hooks_t::open for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_INVAL @p c is NULL or @p open is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_open_hook(hark_conn_t *c,
                                            int (*open)(void *ctx, int *fd));

/**
 * @brief Set the @ref hark_conn_hooks_t::open hook on @p c.
 *
 * @param c    Connector instance.
 * @param open See @ref hark_conn_hooks_t::open for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_INVAL @p c is NULL or @p open is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_on_connect_hook(hark_conn_t *c,
                                                  void (*on_connect)(void *ctx,
                                                                     int fd));

/**
 * @brief Set the @ref hark_conn_hooks_t::on_read hook on @p c.
 *
 * Called when the reactor detects @ref HARK_EV_READ on the connection fd. The
 * mask must include @ref HARK_EV_READ via hark_reactor_mod() for this hook to
 * fire.
 *
 * @param c       Connector instance.
 * @param on_read See @ref hark_conn_hooks_t::on_read for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_on_read_hook(hark_conn_t *c,
                                               void (*on_read)(void *ctx,
                                                               int fd));

/**
 * @brief Set the @ref hark_conn_hooks_t::on_write hook on @p c.
 *
 * Called when the reactor detects @ref HARK_EV_WRITE on the connection fd.
 * The mask must include @ref HARK_EV_WRITE via hark_reactor_mod() for this
 * hook to fire.
 *
 * @param c        Connector instance.
 * @param on_write See @ref hark_conn_hooks_t::on_write for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_on_write_hook(hark_conn_t *c,
                                                void (*on_write)(void *ctx,
                                                                 int fd));

/**
 * @brief Set the @ref hark_conn_hooks_t::open hook on @p c.
 *
 * @param c    Connector instance.
 * @param open See @ref hark_conn_hooks_t::open for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_INVAL @p c is NULL or @p open is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_on_disconnect_hook(
    hark_conn_t *c, void (*on_disconnect)(void *ctx, int reason));

/**
 * @brief Set the @ref hark_conn_hooks_t::open hook on @p c.
 *
 * @param c    Connector instance.
 * @param open See @ref hark_conn_hooks_t::open for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_INVAL @p c is NULL or @p open is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_on_reconnect_hook(
    hark_conn_t *c,
    hark_err_t (*on_reconnect)(void *ctx, int attempt, uint64_t *delay_ms));

/**
 * @brief Set the @ref hark_conn_hooks_t::open hook on @p c.
 *
 * @param c    Connector instance.
 * @param open See @ref hark_conn_hooks_t::open for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_INVAL @p c is NULL or @p open is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_close_hook(hark_conn_t *c,
                                             void (*close)(void *ctx, int fd));

/**
 * @brief Get the reactor associated with @p c.
 *
 * @param c Connector instance.
 * @return Reactor, or NULL if @p c is NULL.
 */
HARK_API hark_reactor_t *hark_conn_reactor(hark_conn_t *c);

/**
 * @brief Get the fd associated with @p c.
 *
 * @param c Connector instance.
 * @return fd, or -1 if @p c is NULL.
 */
HARK_API int hark_conn_fd(hark_conn_t *c);

/**
 * @brief Adopt an existing fd, bypassing the @c open hook.
 *
 * Registers @p fd on the reactor and transitions to
 * @ref HARK_CONN_CONNECTED. Resets backoff and calls @c on_connect.
 *
 * @param c  Connector instance.
 * @param fd File descriptor to adopt.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_BADARG @p c is NULL or @p fd < 0.
 * @retval HARK_ERR_STATE  Not in @ref HARK_CONN_DISCONNECTED state.
 */
HARK_API hark_err_t hark_conn_adopt(hark_conn_t *c, int fd);

#ifdef __cplusplus
}
#endif

#endif /* HARK_CONNECTOR_H */
