# Strut 0.0.2

Strut 0.0.2 consolidates the language, tooling, package ecosystem, and native
backend work completed since the first release.

## Highlights

- Runtime support is composed from program dependencies, keeping generated
  executables focused while preserving strict native-code certification.
- Structured `TypeId` information now drives nested types and stronger generic
  inference, including contextual empty literals, pointers, nullability, and
  nested collections.
- Nominal custom checked errors support structured payloads, propagation,
  typed handling, modules, generics, async code, diagnostics, and editor data.
- `atomic<int>` and `atomic<bool>` provide sequentially consistent operations;
  integer atomics additionally provide `fetch_add` and `fetch_sub`.
- Prefix and postfix `++`/`--` are distinct overload identities. Postfix
  declarations use the compile-time-only `postfix` marker.
- The canonical machine-readable API registry powers improved diagnostics and
  richer completion, hover, signature, definition, and checked-error metadata.
- Executable documentation and AI-DX benchmarks continuously certify examples,
  generated code, diagnostics, and common agent-authored programs.
- Package installs are reproducible through deterministic machine-independent
  lockfiles, immutable Git revisions, SHA-256 integrity, offline cache use, and
  package-aware editor tooling. Official shorthand such as `strut install sqlite`
  resolves only through the `strut-packages` organization.
- HTTPS serving, verified client TLS/private CAs, concurrent HTTP handling,
  lifecycle limits, timeouts, signals, and graceful idempotent shutdown are
  covered by backend dogfood and cross-platform CI.
- Release archives are relocatable, include JSONIC and required notices, and
  publish a generated `SHA256SUMS` file for Linux x64/ARM64, macOS ARM64, and
  Windows x64 artifacts.

# Strut 0.0.1

Strut 0.0.1 is the first packaged compiler release. It establishes the current
pre-1.0 language and toolchain baseline across Linux, macOS, and Windows.

## Highlights

- Executables use `function main() -> int` or
  `function main(string cmd, string[] args) -> int`; the returned value becomes
  the process exit status, `cmd` receives native `argv[0]`, and `args` contains
  only user-supplied arguments.
- Dynamic arrays (`T[]`) are a core language facility and no longer require
  `include <vector>;`; `vector<T>` remains available as a compatibility spelling.
- Filesystem `copy`, `move`, and `remove` operations accept compatible iterable
  path collections. Pairwise copy/move mappings require ordered collections and
  equal lengths.
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
