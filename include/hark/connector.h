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
 * @brief Set the @ref hark_conn_hooks_t::open hook on @p c.
 *
 * @param c    Connector instance.
 * @param open See @ref hark_conn_hooks_t::open for contract.
 * @return @ref HARK_OK on success.
 * @retval HARK_ERR_INVAL @p c is NULL or @p open is NULL.
 *
 * @see hark_conn_hooks_t
 */
HARK_API hark_err_t hark_conn_set_on_data_hook(hark_conn_t *c,
                                               void (*on_data)(void *ctx,
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

#ifdef __cplusplus
}
#endif

#endif /* HARK_CONNECTOR_H */
