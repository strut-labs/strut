# Unreleased

## Highlights

- Binary-first `<crypto>` APIs provide secure random bytes, SHA-256,
  HMAC-SHA-256 and constant-time comparison through OpenSSL 3.0 or newer.
- Dependency-free `<encoding>` APIs provide strict canonical Base64 and
  unpadded Base64url encoding and decoding.
- Buffered outbound HTTP now has strict metadata and option validation,
  bounded request/response memory, HTTP(S)-only redirect policy, normalized
  errors, and shared synchronous/asynchronous libcurl ownership.
- Outbound HTTP adds binary callback streaming, known or unknown-length uploads,
  final-only downloads, cancellation, conservative redirect replay, bounded
  executor admission, and 64 MiB constant-memory resource certification.

# Strut 0.0.3

Strut 0.0.3 is a focused compiler, generated-code performance, and HTTP runtime
release. It does not change the core language syntax contract established by
0.0.2.

## Highlights

- String-only programs now use a focused string runtime instead of compiling
  the complete generated runtime.
- Plain threads use a focused thread runtime, and compatible atomic programs
  use the same small concurrency foundation rather than the full runtime.
- Runtime-feature discovery now traverses nested lambda bodies and switch cases,
  preserving required helpers while keeping generated programs small.
- On the documented Intel i7-12700H/GCC 15.2 certification host, Hello World
  compilation improved from approximately 2,070 ms to 306 ms (about 85%) and
  the representative plain-thread fixture improved from approximately 2,618 ms
  to 729 ms (about 72%). These are environment-specific benchmark results, not
  universal latency guarantees.
- Structural code-size guards prevent the focused string, thread, and atomic
  paths from silently regressing to the complete runtime.
- HTTP servers now provide lossless repeated query/form/cookie values, bounded
  buffered body helpers, structured response cookies, and validated redirects.
- Explicit application-owned files can be streamed with bounded reads and
  single byte-range support through the existing response writer.
- Cancellation-aware NDJSON records use compact JSONIC serialization and the
  existing writer's flush, framing, backpressure, TLS, and failure behavior.
- Compiler, generated C++, package, HTTP/TLS, sanitizer, regression, and
  cross-platform certification continue to cover GCC, Clang/AppleClang, and
  MSVC targets.

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
