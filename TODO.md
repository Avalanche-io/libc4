# libc4 TODO

## Immediate
- Fix base58 encoder: "alfa" matches Go but "foo" does not. Leading byte handling in base58Encode/Decode needs audit against Go reference (`c4/c4/tree.go` base58 implementation).
- Add `FromDigest()` static method to `c4::ID` to avoid round-tripping through string encoding for internal operations.

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
- Add .gitignore
- vcpkg / Conan packaging
- pkg-config file generation
- GitHub Actions CI (macOS, Linux, Windows)

## License
- Evaluate Apache 2.0 vs MIT for IP retention goals
