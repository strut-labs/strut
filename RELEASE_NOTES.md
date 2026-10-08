# Unreleased (0.0.5 development)

## Highlights

- (in progress) Two-way FFI / embedding campaign: stable C ABI, native->Strut calls,
  callbacks, ownership/lifetime rules, checked-error propagation, cross-platform ABI
  correctness (GCC/Clang/AppleClang/MSVC).

# Strut 0.0.4

Strut 0.0.4 is a large HTTP/backend, runtime, and performance release. It does
not change the core language syntax contract established by 0.0.2/0.0.3.

## Highlights

- **Production-oriented HTTP server runtime.** The Linux server can use a
  reactor backend (epoll) with a bounded pool of CPU-sized application workers
  and a bounded stream-worker pool instead of thread-per-connection, with TCP
  fast-open-style per-connection state, backpressure, and bounded shutdown/drain.
  The reactor backend is opt-in via `STRUT_HTTP_REACTOR=1`; the classic backend
  remains the default.
- **Reactor-native TLS.** Non-blocking `SSL_accept`/`SSL_read`/`SSL_write` are
  driven through the readiness loop, with the handshake handled incrementally and
  slow/pathological peers consuming connection state rather than an application
  worker.
- **Streaming, WebSockets, and lifecycle.** Incremental request-body streaming,
  incremental response streaming, RFC 6455 WebSockets, static byte-range
  responses, and cancellation-aware NDJSON streaming are available on the reactor
  path; request framing/parsing and connection lifecycle handling were hardened.
- **Indexed route lookup.** Route registration builds an exact static-path index
  plus a parameter-pattern trie while preserving first-registration semantics
  (including 405/404, trailing-slash, and multi-parameter behavior). Route lookup
  no longer degrades linearly with the total number of registered routes for
  indexed cases; a controlled campaign showed large improvements for
  late-position and miss cases at high route counts versus the previous scan.
- **Request/response hot path.** Response-head serialization writes directly into
  a bounded buffer (no `std::ostringstream`); request ownership is moved rather
  than deep-copied through the reactor/worker handoff; request-head/header
  construction was rewritten to reduce temporary strings and redundant
  representation; and request headers use a specialized compact container while
  preserving the `map<string,string>` language semantics. These changes
  substantially reduced per-request allocation/copy work in a controlled
  benchmark campaign.
- **Outbound HTTP.** Buffered outbound HTTP has strict metadata/option
  validation, bounded request/response memory, HTTP(S)-only redirect policy,
  normalized errors, and shared synchronous/asynchronous libcurl ownership;
  binary callback streaming, known/unknown-length uploads, final-only downloads,
  cancellation, conservative redirect replay, and bounded executor admission are
  supported.
- **Runtime building blocks.** First-class `bytes` with binary reader/writer
  contracts, unified cancellation primitives, cancellation-aware blocking process
  streams, `<crypto>` (secure random, SHA-256, HMAC-SHA-256, constant-time
  compare) over OpenSSL 3.0+, and dependency-free `<encoding>` (canonical Base64
  and unpadded Base64url).
- **Process and platform.** POSIX PTY primitives and hardened POSIX/hosted
  process lifecycle, socket shutdown, and shutdown portability.
- **Tooling, packages, docs, certification.** Post-0.0.3 project consolidation,
  authoritative builtin API registry, focused string/thread/atomic runtime
  slicing, and extensive regression/certification expansion (see below).

## Performance

Performance work is verified by an isolated, environment-specific benchmark
campaign, not by universal claims. Representative retained results from that
campaign (single-vCPU Linux control, `c=50` `/plaintext`) include a direct
response serializer (~+8.4%), request copy/ownership elimination (~+4.6% and
~+2%) and a specialized request-header container (~+3.4%); header construction
and storage changes showed larger gains on a header-heavier workload; and the
indexed router removed a large route-count scaling collapse. Absolute numbers
depend on hardware, load generator, and session; see the benchmark reports in
`strut-benchmarks/` for methodology and controls. Strut has not reached the
historical Rust floor; the Reactor campaign was closed after its evidenced
bounded candidates were exhausted.

## Compatibility

- No core language syntax changes relative to 0.0.2/0.0.3.
- Reactor HTTP server backend is opt-in (`STRUT_HTTP_REACTOR=1`); the classic
  backend remains the default.
- The request-header map remains a `map<string,string>` with unchanged
  language-level semantics (indexing, `contains`, `insert`, `remove`, `length`,
  `clear`, copy/pass/assign, equality).


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
