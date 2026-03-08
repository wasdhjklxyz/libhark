# libhark

Event-driven fd reactor for Linux. Epoll-based event loop with timers and a protocol-agnostic connection manager with auto-reconnect.
This was taken from a module within a capstone project and repurposed into a library - do not expect great things.

## API

```c
#include <hark/hark.h>

/** reactor - epoll event loop */
hark_reactor_t *r = hark_reactor_create();
hark_reactor_add(r, fd, HARK_EV_READ, callback, ctx);
hark_reactor_run(r);  // blocks until hark_reactor_stop()

/** timer - timerfd wrapper */
hark_timer_t *t = hark_timer_create(r, 1000, on_tick, ctx);      // recurring
hark_timer_t *t = hark_timer_oneshot(r, 5000, on_timeout, ctx);  // one-shot

/** connector - auto-reconnect state machine */
hark_conn_t *c = hark_conn_create(r, &hooks, ctx);
hark_conn_open(c);  // calls your hooks.open() to get an fd
```

The connector is protocol-agnostic. You provide hooks (`open`, `on_data`, `close`, etc.) - it manages the fd lifecycle, reactor registration, and reconnect with configurable backoff. Works with TCP, UDP, Unix sockets, serial ports, or anything that produces an fd.

## Build

```bash
cmake -B build
cmake --build build
```

Options:

| Flag | Default | Description |
|------|---------|-------------|
| `hark_BUILD_EXAMPLES` | `ON` | Build examples |
| `hark_BUILD_SHARED` | `OFF` | Shared library instead of static |
| `hark_ASAN` | `OFF` | AddressSanitizer |

## Install

```bash
cmake --install build --prefix /usr/local
```

Then in your project:

```cmake
find_package(hark REQUIRED)
target_link_libraries(myapp PRIVATE hark::hark)
```

## Nix

### As a dependency in your flake

```nix
{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    libhark.url = "github:wasdhjklxyz/libhark";
  };

  outputs = { self, nixpkgs, libhark, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      hark = libhark.packages.${system}.default;
    in {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "myapp";
        version = "0.1.0";
        src = ./.;
        nativeBuildInputs = [ pkgs.cmake ];
        buildInputs = [ hark ];
      };
    };
}
```

Your CMake `find_package(hark)` will resolve automatically since Nix puts the installed headers and `.a` into the build environment.

### Dev shell

```bash
nix develop github:wasdhjklxyz/libhark  # drop into libhark's dev shell
```

### Cross-compile for aarch64-musl

```bash
nix build .#aarch64-static
```

## License

MIT
