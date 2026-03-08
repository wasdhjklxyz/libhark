#include <hark/types.h>

HARK_API const char *hark_strerror(hark_err_t err) {
  switch (err) {
  case HARK_OK:
    return "success";
  case HARK_ERR:
    return "generic error";
  case HARK_ERR_NOMEM:
    return "out of memory";
  case HARK_ERR_SYSCALL:
    return "system call failed";
  case HARK_ERR_BADFD:
    return "bad file descriptor";
  case HARK_ERR_BADARG:
    return "invalid argument";
  case HARK_ERR_STATE:
    return "invalid state for operation";
  }
  return "unknown error";
}
