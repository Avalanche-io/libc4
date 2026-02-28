# libc4 TODO

## C4M Parser
- Implement .c4m format parser against `~/ws/active/c4/c4/c4m/SPECIFICATION.md`
- Generate cross-language test vectors from Go implementation
- Sequence detection and frame range parsing
- Diff, merge, intersect operations

## Python Bindings
- Add pybind11/nanobind subdirectory
- Wrap c4::ID and c4m::Manifest
- Package as `c4` Python module on PyPI

## Packaging
- vcpkg / Conan packaging
- pkg-config file generation
- GitHub Actions CI (macOS, Linux, Windows)

## License
- Evaluate Apache 2.0 vs MIT for IP retention goals
