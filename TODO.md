# libc4 TODO

## C4M Parser
- [x] Implement .c4m format parser against specification (94+ tests passing)
- [x] Generate cross-language test vectors from Go implementation
- [x] Diff, merge operations (free functions: c4m::Diff, c4m::Merge)
- [~] Sequence detection and frame range parsing (partial)
- [~] Intersect operation (free function exists, limited scope)

## Python Bindings
- Add pybind11/nanobind subdirectory
- Wrap c4::ID and c4m::Manifest
- Package as `c4` Python module on PyPI

## Packaging
- vcpkg / Conan packaging
- pkg-config file generation
- GitHub Actions CI (macOS, Linux, Windows)

## License
- [x] Apache 2.0
