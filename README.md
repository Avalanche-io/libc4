# libc4

C/C++ reference implementation of the C4 ID system ([SMPTE ST 2114:2017](https://ieeexplore.ieee.org/document/8255805)).

```cpp
#include <c4/c4.hpp>

auto id = c4::ID::Identify("hello world");
std::cout << id << std::endl;
// output: c4YGsLNdHGW3vnRB... (90 characters)
```

## What is C4?

C4 IDs are universally unique, consistent identifiers derived from file content using SHA-512. The same data always produces the same 90-character ID, regardless of filename, location, or time. C4 IDs are URL-safe, filename-safe, and double-click selectable.

This library provides:

- **C4 ID computation** — identify any data by content
- **C4 ID encoding** — base58 encoding with `c4` prefix (90 chars)
- **Tree IDs** — compute a single ID for a set of IDs
- **C4M format** — parse and generate `.c4m` files
- **C4M operations** — diff, merge, intersect manifests
- **Dual API** — C API (`c4.h`) for maximum portability, C++ API (`c4.hpp`) for ergonomics

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Requires:
- CMake 3.16+
- C++17 compiler
- OpenSSL (for SHA-512)

## API

### C++ API

```cpp
#include <c4/c4.hpp>
#include <c4/c4m.hpp>

// Identify data
auto id = c4::ID::Identify("some data");
auto file_id = c4::ID::IdentifyFile("/path/to/file");

// Parse/encode
auto parsed = c4::ID::Parse("c43inc2q...");
std::string str = id.String();

// Tree ID (set of IDs -> single ID)
c4::IDs ids;
ids.Append(c4::ID::Identify("alfa"));
ids.Append(c4::ID::Identify("bravo"));
auto tree_id = ids.ID();  // order-independent

// C4M manifests (TODO: parser implementation in progress)
// auto manifest = c4m::Manifest::ParseFile("project.c4m");
// auto diff = manifest.Diff(other);
```

### C API

```c
#include <c4/c4.h>

c4_id_t *id = c4_id_new();
c4_identify("some data", 9, id);

char buf[91];
c4_id_string(id, buf, sizeof(buf));
printf("%s\n", buf);

c4_id_free(id);
```

## Cross-language compatibility

This implementation produces identical output to the [Go reference implementation](https://github.com/Avalanche-io/c4). Test vectors are shared to ensure all implementations agree on ID computation.

## License

Apache 2.0 — see [LICENSE](./LICENSE) and [NOTICE](./NOTICE).
