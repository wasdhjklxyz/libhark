/**
 * @file types.h
 * @brief Public types, error codes, event flags, and callback signatures.
 *
 * All public libhark types are defined here. Headers for individual modules
 * (reactor.h, timer.h, connector.h) include this automatically.
 */

#ifndef HARK_TYPES_H
#define HARK_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include <hark/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes returned by libhark functions.
 *
 * Functions that return @c hark_err_t use @ref HARK_OK on success
 * and a negative value on failure. Use hark_strerror() for a
 * human-readable description.
 */
typedef enum {
  HARK_OK = 0,           /**< Success. */
  HARK_ERR = -1,         /**< Generic / unrecoverable error. */
  HARK_ERR_NOMEM = -2,   /**< Allocation failed (out of memory). */
  HARK_ERR_SYSCALL = -3, /**< A system call failed; check @c errno. */
  HARK_ERR_BADFD = -4,   /**< File descriptor is invalid or not registered. */
  HARK_ERR_BADARG = -5,  /**< One or more arguments are invalid/NULL. */
  HARK_ERR_STATE = -6,   /**< Operation not valid in the current state. */
  HARK_ERR_INVAL = -7,   /**< Invalid value obtained/reached. */
} hark_err_t;

/**
 * @brief Return a human-readable string for a @ref hark_err_t code.
 *
 * @param err Error code.
 * @return Static string describing the error. Never NULL.
 */
HARK_API const char *hark_strerror(hark_err_t err);

/**
 * @brief Bitmask of events for reactor fd registration.
 *
 * Combine with bitwise OR when calling hark_reactor_add() or
 * hark_reactor_mod(). Delivered to the @ref hark_fd_fn callback.
 */
typedef enum {
  HARK_EV_READ = 0x01,  /**< File descriptor is readable. */
  HARK_EV_WRITE = 0x02, /**< File descriptor is writable. */
  HARK_EV_ERROR = 0x04, /**< Error condition on the file descriptor. */
  HARK_EV_HUP = 0x08,   /**< Hang-up (peer closed connection). */
} hark_events_t;

/**
 * @brief States of a @ref hark_conn_t connection.
 */
typedef enum {
  HARK_CONN_DISCONNECTED = 0, /**< Not connected. */
  HARK_CONN_CONNECTING = 1,   /**< Async connect in progress. */
  HARK_CONN_CONNECTED = 2,    /**< Connected and registered with reactor. */
} hark_conn_state_t;

/**
 * @brief Return value from the connector @c open hook.
 *
 * Tells the connector whether the fd is immediately usable
 * or still completing an asynchronous connect.
 */
typedef enum {
  HARK_CONN_READY = 0,   /**< fd is connected and ready for I/O. */
  HARK_CONN_PENDING = 1, /**< Async connect; wait for writability. */
} hark_conn_open_result_t;

/** @brief Opaque reactor handle. */
typedef struct hark_reactor hark_reactor_t;

/** @brief Opaque timer handle. */
typedef struct hark_timer hark_timer_t;

/** @brief Opaque connector handle. */
typedef struct hark_conn hark_conn_t;

/**
 * @brief Callback for timer events.
 *
 * @param t   The timer that fired.
 * @param ctx User data provided at creation.
 */
typedef void (*hark_timer_fn)(hark_timer_t *t, void *ctx);

/**
 * @brief Callback for file descriptor events.
 *
 * @param r      Reactor that dispatched the event.
 * @param fd     The file descriptor with activity.
 * @param events Bitmask of @ref hark_events_t flags.
 * @param ctx    User data provided at registration.
 */
typedef void (*hark_fd_fn)(hark_reactor_t *r, int fd, uint32_t events,
                           void *ctx);

/**
 * @brief Callback table for the connector state machine.
 *
 * The connector is protocol-agnostic. You provide hooks that create,
 * observe, and tear down a connection. The connector manages the lifecycle:
 * registration with the reactor, error detection, and reconnect with backoff.
 *
 * All hooks receive the @c ctx pointer given to hark_conn_create().
 */
typedef struct {
  /**
   * @brief Create a connection and return a file descriptor.
   *
   * Set @p *fd to the new descriptor. Return @ref HARK_CONN_READY if the
   * fd is immediately usable, @ref HARK_CONN_PENDING for an async connect
   * (the connector will watch for writability), or @c -1 on failure.
   *
   * @param ctx User data.
   * @param fd  [out] Receives the new file descriptor.
   * @return @ref hark_conn_open_result_t on success, @c -1 on failure.
   */
  int (*open)(void *ctx, int *fd);

  /**
   * @brief Connection established.
   *
   * Called once the fd is confirmed ready (immediately for @ref
   * HARK_CONN_READY, after writability for @ref HARK_CONN_PENDING).
   *
   * @param ctx User data.
   * @param fd  The connected file descriptor.
   */
  void (*on_connect)(void *ctx, int fd);

  /**
   * @brief Data available to read.
   *
   * The reactor detected @ref HARK_EV_READ on the fd. Read from @p fd
   * using your protocol framing. The connector never calls read() itself.
   *
   * @param ctx User data.
   * @param fd  The readable file descriptor.
   */
  void (*on_data)(void *ctx, int fd);

  /**
   * @brief Connection lost.
   *
   * Called on @ref HARK_EV_ERROR or @ref HARK_EV_HUP before the
   * reconnect timer starts.
   *
   * @param ctx    User data.
   * @param reason The errno at the time of disconnect.
   */
  void (*on_disconnect)(void *ctx, int reason);

  /**
   * @brief About to attempt a reconnect.
   *
   * Called before each reconnect attempt. Inspect or override the
   * backoff delay via @p *delay_ms. Return @ref HARK_OK to proceed,
   * or any error to give up (no further reconnect attempts).
   *
   * @param ctx      User data.
   * @param attempt  Retry count (1-indexed).
   * @param delay_ms [in/out] Current backoff delay; modify to override.
   * @return @ref HARK_OK to reconnect, @ref HARK_ERR to stop trying.
   */
  hark_err_t (*on_reconnect)(void *ctx, int attempt, uint64_t *delay_ms);

  /**
   * @brief Close the connection.
   *
   * Called when the connector needs to shut down the fd - on disconnect,
   * during reconnect cleanup, or on hark_conn_close(). You own the fd;
   * call close() or your protocol's cleanup here.
   *
   * @param ctx User data.
   * @param fd  The file descriptor to close.
   */
  void (*close)(void *ctx, int fd);
} hark_conn_hooks_t;

#ifdef __cplusplus
}
#endif

#endif /* HARK_TYPES_H */
