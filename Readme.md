# libc4

A minimal, efficient C implementation of the C4 ID specification (SMPTE ST 2114:2017).

## What is C4?

C4 stands for Cinema Content Creation Cloud - an open framework for distributed media production. Media as in movies, tv, music, broadcast, etc. Distributed as in studios, editors, post production locations that are in many locations around the world, and distributed as in remote storage and compute (aka the cloud).

Though it started in media, C4 is useful for all kinds of distributed data problems. In particular the C4 identification system (C4 ID), an international standard published by SMPTE, is a universally unique ID that provides a solution to distributed multi-party consensus.

Think source code management / revision control for huge media files distributed around the world.

**C4 IDs** are 90-character base58-encoded SHA-512 hashes that provide consistent identification of data across all observers worldwide without requiring a central registry.

## Quick Start

```c
#include <c4.h>
#include <stdio.h>

int main(void) {
    /* Initialize library */
    c4_init();

    /* Create encoder */
    c4id_encoder_t* enc = c4id_encoder_new();

    /* Hash some data */
    const char* data = "Hello, World!";
    c4id_encoder_write(enc, data, strlen(data));

    /* Get the C4 ID */
    c4_id_t* id = c4id_encoder_id(enc);
    char* str = c4id_string(id);

    printf("C4 ID: %s\n", str);

    /* Cleanup */
    free(str);
    c4id_free(id);
    c4id_encoder_free(enc);

    return 0;
}
```

Compile:
```bash
gcc -o example example.c -lc4 -lgmp -lssl -lcrypto
```

## Installation

### Using Make

```bash
make
sudo cp lib/libc4.a /usr/local/lib/
sudo cp include/c4.h /usr/local/include/
```

### Using CMake

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

See [INSTALL.md](INSTALL.md) for detailed build instructions.

## Documentation

- **[API.md](API.md)** - Complete API reference with examples
- **[INSTALL.md](INSTALL.md)** - Build and installation instructions
- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Implementation details and changes
- **[docs/C4.md](docs/C4.md)** - Additional usage documentation

## Features

- ✅ **Opaque types** - Complete information hiding, no external dependencies in public headers
- ✅ **Idiomatic C** - Standard C99 conventions, const correctness, proper memory management
- ✅ **Enhanced error reporting** - Detailed error information with position tracking
- ✅ **Cross-platform** - CMake build system for easy compilation on any platform
- ✅ **Performance** - Efficient implementation with benchmarking suite
- ✅ **Robust** - Comprehensive fuzzing tests for security and stability
- ✅ **Feature complete** - Full API parity with Go reference implementation
- ✅ **Thread-safe error handling** - Thread-local error state

### C Implementation

`libc4` is a C library based on the golang reference implementation found at github.com/Avalanche-io/c4.

Core C4 ID features implemented:
- Generating and parsing C4 IDs
- C4 Digests with sum operations
- Streaming encoder for large data
- Error reporting with detailed context

### Roadmap

`libc4` is in active development so expect regular updates for features and bug fixes. There may be a chance for API changes prior to a 1.0 release, but we are working towards a stable API.

### Contributing

Please submit issues to github if you find bugs, and we welcome pull requests.

Call for platform maintainers. Please open an issue if you would be willing to help us maintain build configurations for other operating systems.

### LICENSE

MIT License see [LICENSE](LICENSE) for more information.

