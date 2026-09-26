# Strut 0.0.1

Strut 0.0.1 is the first packaged compiler release. It establishes the current
pre-1.0 language and toolchain baseline across Linux, macOS, and Windows.

## Highlights

- Safe owning pointers use `T* owner := new(...)`; raw pointer extraction is an
  explicit unsafe operation written `ptr<T> raw := ptr(owner)`.
- Typed and inferred lambdas compose with generic functions, containers, async
  work, and operator definitions.
- Nested generic types are parsed and canonicalized recursively, including safe
  pointer and reference types inside generic containers.
- Cross-feature runtime behavior is more reliable across collections, JSON,
  filesystem, processes, networking, concurrency, FFI, and SQLite workloads.
- Diagnostics, source spans, formatting, and documentation were tightened around
  the supported language contract.
- Regression cases run in isolation, preventing one failure from hiding or
  contaminating another.
- Generated C++ is certified with strict warnings under GCC, Clang/AppleClang,
  and MSVC, with dogfood programs exercised on Linux, macOS, and Windows.

Strut remains pre-1.0. See `COMPATIBILITY.md` for the compatibility policy and
`BUILDING.md` for source and installed-layout instructions.
