# Backend runtime expansion handover

This document records the backend/runtime expansion that starts after Strut
0.0.3. It is deliberately separate from the compiler bootstrap checkpoint
history in `IMPLEMENTATION_HANDOVER.md`.

## Accepted campaign closure

The production backend foundation campaign is complete through P10. P10 is a
deliberate campaign boundary, not an invitation to infer another numbered
checkpoint. No repository roadmap freezes a P11 contract.

Accepted immutable P10 implementation baseline:

- compiler: `1d7e15ea1ce18dd95a0fac609c61536491023986`;
- website source at acceptance: `9189916da5164a31504716956da070af1fa68b62`;
- published website at acceptance: `f25b016296cdf57a848cca48f0f57e7bfda8bc8a`.

Accepted hosted evidence:

- five-platform compiler certification: <https://github.com/strut-labs/strut/actions/runs/36916992356>;
- exact-compiler-SHA independent Linux/macOS/Windows regressions: <https://github.com/strut-labs/strut-regression-suite/actions/runs/36948321664>;
- website source certification: <https://github.com/strut-labs/strut-labs.github.io/actions/runs/36951853937>;
- Pages build and deployment: <https://github.com/strut-labs/strut-labs.github.io/actions/runs/36951838316>.

The latest tagged release remains `v0.0.3`. The accepted website documents
P10/current `main`; post-v0.0.3 APIs must not be attributed to the tagged
release.

## Frozen ownership boundary

The following are first-class Strut runtime facilities:

- HTTP client and server
- TLS and selected cryptographic primitives
- networking primitives
- bytes and streams
- cancellation
- files
- processes
- WebSockets
- PTYs
- the existing SQLite runtime integration

Native implementation dependencies remain hidden behind Strut APIs:

- libcurl implements outbound HTTP;
- OpenSSL implements inbound TLS and selected cryptographic primitives;
- operating-system APIs implement sockets, processes, PTYs and platform integration;
- the system/external SQLite library follows Strut's existing component model.

The following are package-level or deferred work and are not prerequisites for
the backend runtime:

- ZIP and other archive formats belong in future packages;
- multipart/form-data;
- server-sent events;
- HTTP/2;
- a reactor/event-loop backend, pending measured need;
- WebSocket compression and an outbound WebSocket client;
- a built-in authentication/session framework;
- an ORM;
- a reverse-proxy framework.

No archive dependency is part of this roadmap.

## CP1 baseline

Baseline source revisions before this work:

- Strut: `d107b9affc8ae0c9478fd2b2919a38e541e50584` on `main`, synchronized with `origin/main` and clean.
- Regression suite: `5cc7e6dbf97a165224fea014afc54ae8458f0e18` on `main`, synchronized with `origin/main` and clean.

Local certification toolchain:

- CMake 4.2.3
- GCC/G++ 15.2.0
- Python 3.14.4
- libcurl 8.18.0
- OpenSSL 3.5.5
- SQLite development library available to generated-program linking; the `sqlite3` CLI is not installed.

Repeatable validation commands:

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 dogfood/http_lifecycle_certification.py build/strut
python3 dogfood/backend_baseline_certification.py build/strut
python3 dogfood/http_framing_certification.py build/strut
python3 dogfood/http_worker_certification.py build/strut
python3 tests/ffi/run_native_link_tests.py build/strut
python3 ../strut-regression-suite/runner.py --compiler build/strut
```

`dogfood/backend_baseline_certification.py` records the current backend
contract without fixing it. It covers GET and POST routing, 404/405/413/431
responses, connection-limit rejection, many sequential requests, and repeated
server start/stop cycles. `dogfood/http_lifecycle_certification.py` separately
covers concurrent drain, explicit stop, SQLite in an HTTPS route, certificate
rejection, a trusted private CA, and the libcurl client/server path.

## Known baseline limitations

These are the remaining accepted baseline facts after CP8:

- server request bodies are completely buffered before dispatch;
- server responses are completely buffered before writing;
- successful and error responses use `Connection: close`;
- strict HTTP/1.0 and HTTP/1.1 request heads are supported; HTTP/2 is rejected;
- one case-insensitive `Content-Length` controls request framing; every duplicate is rejected;
- `Transfer-Encoding` is recognized and rejected because chunked request bodies remain unsupported;
- repeated fields are detected case-insensitively and rejected while the public request header map remains single-valued; accepted keys are lowercase;
- no protocol-upgrade or WebSocket path exists;
- handlers receive no request cancellation or disconnect signal;
- no streaming request-body or response-writer abstraction exists;
- admitted HTTP work uses a bounded reusable blocking worker pool; a reactor/event loop remains deferred;
- remaining light/full duplication is limited to foundational core/string/JSON/SQLite and unrelated filesystem/thread helpers; executor, TCP, HTTP client/helpers and the active HTTP server have one implementation owner;
- API knowledge is split among the registry, semantic analysis and code generation;
- libcurl responses are unbounded buffered strings and transfers cannot be cancelled;
- SQLite rows pass through JSON, lose exact large-integer semantics and do not expose BLOB values;
- process pipes retain their text APIs and now also expose the binary stream contract; process groups, timed waits and descendant cleanup remain absent.

## Initial concurrency decision

The critical path retains a bounded blocking/threaded architecture. HTTP/1.1,
streaming, cancellation, upgrades and moderate long-lived connection counts do
not intrinsically require epoll, kqueue or IOCP. Public ownership, stream and
cancellation contracts must not prevent a future event-driven backend.

## Checkpoint status

- CP1: baseline and scope freeze complete.
- CP2: canonical builtin API schema complete.
- CP3: generated runtime implementation boundaries complete.
- CP4: first-class owned bytes values complete.
- CP5: generic binary stream contracts complete.
- Review Gate 2 approved.
- CP6: unified cancellation primitives complete.
- CP7: cancellable blocking process handles complete.
- Review Gate 3 approved.
- CP8: strict HTTP/1 request parsing and framing complete.
- CP9: bounded HTTP worker and connection ownership complete.
- CP9A: HTTP foundation corrections complete.
- CP10: streaming response writer complete.
- CP11: streaming request bodies complete.
- CP12: HTTP/1 connection persistence and request-scoped cancellation complete.
- CP13: HTTP application request and response helpers complete.
- CP14: static file and single-range responses complete.
- CP15: cancellation-aware NDJSON streaming complete.
- Review Gate 5 approved.

## Production backend campaign

- P1: crypto and encoding primitives complete.
- P2: buffered outbound HTTP hardening complete.
- P3: outbound HTTP streaming and cancellation complete.
- P4: WebSocket upgrade ownership complete.
- P5: WebSocket server frame/message runtime complete.
- P6: process lifecycle hardening complete.
- P7: POSIX PTY primitive complete.
- P8: PTY lifecycle, resize, signals and cancellation complete.
- P9: Windows ConPTY complete.
- P10: bounded WebSocket and PTY application composition complete.

Do not renumber this history. In particular, the CP checkpoints above are the
older backend-runtime expansion sequence and the P checkpoints are the completed
production-backend campaign; identical numbers across those namespaces do not
refer to the same work.

P1 adds binary-first secure random bytes, SHA-256, HMAC-SHA-256, constant-time comparison, and strict RFC 4648 Base64/Base64url. Encoding is dependency-free; crypto is implemented by OpenSSL `libcrypto` and does not pull in `libssl`. SHA-1 remains internal to the WebSocket handshake and is not a public cryptographic primitive.

## P5 WebSocket frame/message result

- The P4 request-scoped upgrade handle now owns incremental RFC 6455 frame parsing and bounded text/binary reassembly over the sole HTTP worker transport. The public tagged `websocket_message`, convenience reads, text/bytes writes, ping and close APIs are registry-, sema-, LSP- and codegen-visible. Protocol/state/UTF-8 failures use `WebSocketError`; transport failures remain `NetworkError`.
- `http_server.websocket_limits(frame_bytes, message_bytes)` configures only the stopped server and defaults to 1 MiB/4 MiB. The frame value is a data-frame payload limit of at least 125 bytes; control frames retain RFC 6455's independent 125-byte ceiling. Client masking, RSV/opcodes, canonical extended lengths, 64-bit high-bit rejection, fragmentation transitions, close codes and incremental UTF-8 are enforced before delivery or oversized allocation.
- At P5, one reader was permitted, writes were serialized and synchronous, and TCP/TLS transport operations used a one-operation-at-a-time contract. P10 supersedes only that transport restriction with independently progressing read/write requests while retaining the P5 parser, framing, bounded mismatch slot, close handshake, invalidation and per-I/O byte-drip contracts.
- `http_websocket` composes HTTP server plus bytes without OpenSSL. Plain HTTP omits SHA-1 and all frame/message code; plaintext WebSocket adds no native library; TLS adds the existing server TLS stack.
- Deterministic local certification covers plaintext/TLS text and binary messages, accepted and emitted canonical 16/64-bit lengths, carry bytes, partial framing and partial disconnects, exact and over-limit boundaries, fragmented UTF-8 with interleaved ping, server-initiated close response waits, Ping after local Close, 1010 rejection, malformed frame/control/close/UTF-8 cases, concurrent writes, timeout-released blocked write/close, handler-return overlap with an admitted escaped reader, blocked-read invalidation, and 100 repeated handshakes. Resource checks sample the live generated server's RSS, descriptors/handles and worker count where the host exposes them. This is practical protocol/resource certification, not production soak or internet-scale long-lived-connection evidence; hosted platform CI remains required.

## P10 WebSocket and PTY composition result

- P10 adds no bridge API and no `http_websocket`/`pty` runtime dependency edge. A generated application owns two public-API pumps: binary WebSocket input and validated text controls drive one PTY, while a joined output task copies at most 4096 terminal bytes into each synchronous binary WebSocket write.
- WebSocket transport now permits one blocked reader and one serialized writer to progress concurrently. Plain TCP uses independent socket directions. TLS transfers the upgraded connection to one nonblocking OpenSSL owner with one pending request per direction, exact incomplete-write retries, absolute request deadlines and terminal timeout/failure handling.
- `dogfood/websocket_pty_session_certification.py` generates both the session server and its PTY child. It covers policy rejection before 101, coalesced upgrade input, plaintext/TLS, shell and executable launch, binary input/output, resize and lifecycle controls, natural exit after final-output drain, clean and abrupt disconnects, a 32 MiB slow-peer path, cancellation of blocked PTY input on shutdown, 1000 sequential sessions where unsanitized, and bounded concurrent sessions with native-resource drain and bounded reusable HTTP workers.

## Campaign closure handover

### Current architecture

- The inbound HTTP/1.0 and HTTP/1.1 server owns strict framing, bounded reusable workers, persistence, request-scoped cancellation, buffered and streaming request/response paths, application helpers, static/range responses, NDJSON, TLS and WebSocket upgrades.
- Inbound TLS uses OpenSSL with TLS 1.2 or newer. Certificate/key validation occurs before listen; there is no plaintext fallback or server-side mTLS surface.
- Buffered and streaming outbound HTTP share one libcurl engine, validation policy, redirect policy, limits, TLS verification, error mapping and lifecycle. Async calls use the bounded shared executor.
- Public crypto provides secure random bytes, SHA-256, HMAC-SHA-256 and constant-time equal; strict Base64/Base64url encoding is dependency-free. SHA-1 is internal to RFC 6455 only.
- WebSockets provide explicit policy-before-101 ownership, bounded RFC 6455 frames/messages, validated close/control/UTF-8 behavior and synchronous serialized writes.
- Cancellation has one public source/token model reused by HTTP, process and PTY facilities.
- Processes use argv-safe launch, Unicode `CreateProcessW` on Windows, process-group/Job ownership, bounded cleanup, concurrent pipe drains and cancellation-aware blocking pipe I/O.
- PTYs expose one binary API across Linux, macOS and Windows ConPTY, with resize, controls, cancellation, wait/status and bounded lifecycle ownership.
- One WebSocket reader and one serialized writer may progress concurrently. TCP uses independent socket directions; TLS gives one nonblocking owner thread sole access to `SSL*` with at most one bounded request per direction.
- WebSocket-to-PTY sessions are application composition. The application owns validated input/control and bounded output pumps, passes request cancellation to the PTY, closes every terminal path and joins its output task.
- SQLite remains the existing external/system-library integration and composes with the HTTP runtime without another database or networking stack.
- Typed-IR runtime-component resolution emits transitive slices once and drives native linking. WebSocket and PTY remain independent components.

### Critical invariants

- Do not introduce a duplicate HTTP client, HTTP server, networking or TLS stack.
- Keep one cancellation model; do not add subsystem-specific public tokens.
- Keep WebSocket and PTY independent in `runtime_components.cpp`.
- Do not add a built-in WebSocket/PTY bridge or terminal protocol without a separately approved architecture change.
- Never enter one upgraded connection's `SSL*` concurrently; the TLS owner remains the sole OpenSSL caller.
- Permit exactly one active WebSocket reader and serialize all writers; this still allows one read and one write to progress independently.
- Permit one PTY reader and one PTY writer; reject duplicate same-direction operations.
- Preserve synchronous bounded backpressure. Do not hide an unbounded queue between network, process or PTY endpoints.
- Prefer package/application policy for authentication, sessions and protocols. Add compiler/runtime capability only after a real workload proves a missing primitive.

### Accepted platform and API limits

- POSIX PTY foreground lookup and signal delivery cannot be atomic. Background or detached groups may escape leader-group cleanup; stronger containment requires an external supervisor or cgroup. External `SIGCHLD` reaping is unsupported.
- ConPTY is UTF-8/VT terminal traffic, not arbitrary binary transparency. Its interrupt, hangup, terminate and kill operations are Windows terminal/lifecycle approximations rather than POSIX signals.
- Windows before build 26100 may retain bounded OS-owned ConPTY process handles because of <https://github.com/microsoft/terminal/issues/17903>. Certification permits only the documented version-aware shape.
- WebSockets are server-only, with one reader, serialized writers, no extensions/compression, no outbound client and per-I/O frame-parser timeouts rather than a whole-frame deadline.
- The HTTP client has no public client-certificate, explicit-proxy or Unix-domain-socket options. HTTP/2 is absent. Outbound async work still uses blocking libcurl easy handles on the bounded executor.
- Process pipes have no public timeout, cancellation does not terminate the child, and POSIX descendants may escape their process group. `pipe_exec` remains a fixed two-stage helper.
- The latest release is `v0.0.3`; P1-P10 behavior is current-main/unreleased unless a later release explicitly includes it.

### Certification map

- `dogfood/crypto_certification.py`: P1 known answers, binary values, randomness lengths and strict malformed-input handling.
- `dogfood/http_client_certification.py`: P2 buffered HTTP policy, limits, redirects, methods, TLS and cleanup.
- `dogfood/http_client_streaming_certification.py`: P3 bounded upload/download, callbacks, cancellation, backpressure, async and repeated cleanup.
- `dogfood/http_websocket_certification.py`: P4 upgrade policy, subprotocols, ownership, TLS and stop races.
- `dogfood/websocket_runtime_certification.py`: P5 framing/messages, protocol failures, close behavior, TCP/TLS resources and stress.
- `dogfood/process_cancellation_certification.py` and `dogfood/process_lifecycle_certification.py`: P6 cancellable pipes, argv/platform behavior, process trees, pipelines and bounded cleanup.
- `dogfood/pty_certification.py`: P7 PTY spawn, binary/interactive I/O, ownership, cancellation and cleanup.
- `dogfood/pty_lifecycle_certification.py`: P8 resize, controls, wait/close/cancel races, descendants, churn and resources; it also executes the P9 ConPTY lifecycle paths on Windows.
- `dogfood/websocket_pty_session_certification.py`: P10 public-API composition, duplex transport, controls, slow peers, final drain, shutdown, 1000-session churn, bounded concurrency and resource return.
- `dogfood/http_*_certification.py` plus `dogfood/backend_baseline_certification.py`: the accepted strict HTTP server, streaming, persistence, helper, file, NDJSON, cancellation, worker and lifecycle foundations.
- `.github/workflows/cross-platform.yml`: Linux x64 GCC/Clang, Linux ARM64 GCC, macOS ARM64 AppleClang and Windows x64 MSVC hosted gate.
- `strut-regression-suite/.github/workflows/cross-platform.yml`: independent exact-ref black-box Linux/macOS/Windows gate.
- `tools/certify_docs.py`: canonical example compilation, API-registry requirements and website parity.

### Future work inventory, not an implementation roadmap

#### A. Runtime/compiler work genuinely still required

No new runtime primitive is currently proven necessary for the first Warden-shaped workload. Use that workload to test the accepted APIs before changing the compiler. Candidate capabilities to reconsider only with measured evidence include HTTP client certificates, explicit proxies, Unix-domain HTTP, HTTP/2, outbound WebSockets/compression, server mTLS, process timeouts, and event-loop/reactor scaling. Some containment and ConPTY limits are operating-system facts rather than compiler defects.

#### B. Standard/package ecosystem work

Session storage/cookies, CSRF policy, password authentication/hashing, basic JWT, OAuth/OIDC, SSE and multipart/form-data should default to packages. Password hashing may justify a narrowly reviewed native dependency, but not a general authentication framework in the runtime. Certificate automation, reverse-proxy policy and ORM work also remain package or deployment concerns by default.

#### C. Acceptance and dogfood applications

Build one Warden-shaped end-to-end acceptance application first. It should exercise HTTP/TLS, streaming, persistence, cookies, cancellation, SQLite, WebSockets, processes or PTYs where natural, clean shutdown and package ergonomics. Its purpose is to expose missing primitives and ownership friction, not to become a hidden runtime feature or to presuppose Warden's final product architecture.

#### D. Production certification and soak

After package/application acceptance, add long-duration soak, higher connection and session churn, slow and adversarial peers, deployment/restart/upgrade evidence, resource monitoring, proxy/TLS topology, operational failure recovery and repeatable performance baselines. Focused P1-P10 certification is not production-readiness evidence by itself.

#### E. Longer-term general systems-language work

Continue only from measured workloads: compiler diagnostics and optimisation, broader native-library ergonomics, cross compilation, package provenance, debugging/profiling, compatibility policy and eventual self-hosting decisions. Do not distort the language merely to reproduce framework conventions.

### Recommended next campaign structure

Recommendation only: let P1-P10 stand as the completed **backend runtime foundation** campaign. Start a separately named **application foundation** campaign with a Warden-shaped acceptance gate, then package-focused session/auth/CSRF/JWT work, then OAuth/OIDC plus SSE/multipart, and only then production certification. A new namespace makes the package/application default visible and avoids implying that an undefined P11 already exists. If maintainers later choose to continue P numbering, they must first freeze a P11 contract in this document; historical P1-P10 numbering must not change.

### Application & Package Foundation (APF) campaign

The APF campaign follows the backend runtime foundation. It is a separate namespace; its
checkpoints are not P-numbered and do not renumber the P1-P10 history.

- **APF-0 — Foundation correctness** (complete): namespace-scope aggregate/facade
  initialization, SemVer zero-major caret/tilde tightening, and the documentation
  certification baseline.
- APF-0A: struct literals, JSON literals, lambda values and embedded-asset values at
  module scope now lower through captureless lambdas (or capture by value inside
  function bodies) instead of an illegal namespace-scope capture-default. Equivalent
  function-scope semantics are preserved; runtime slicing is unchanged.
- APF-0B: the package resolver implements the documented SemVer contract, including
  zero-major tightening (`^0.2.3` excludes `0.3.0`; `^0.0.3` is exact patch range).
  Lockfile schema, exact versions, `*`, immutable locks and content checksums are
  unchanged.
- APF-0C: compiler CI now certifies the audited immutable website source
  `7903da7e88550f02117224678c4ff99e6c4fbc93`; `tools/certify_docs.py` no longer
  treats the historical P9 checkpoint `d9e6c47` as a required website needle and
  instead asserts stable semantic facts (current-development channel, `v0.0.3` as
  latest tagged release, full-duplex WebSockets, Windows ConPTY, WebSocket-to-PTY as
  application composition without a bridge API, canonical API examples). The compiler
  website SHA is intentionally immutable and must advance whenever a checkpoint
  changes canonical public API documentation.
- APF-0D: the Windows pre-24H2 and 2610+ PTY/WebSocket resource-retention checks
  reject only positive resource excess, not ambient decreases; a decreasing
  thread/handle count between samples is no longer reported as retention.

- **APF-H1 — Component closure + showcase certification** (complete): a builtin API
  brings along the standard modules its documented public types require, and the
  canonical showcase examples are certified.
- APF-H1 component closure: `embed_dir()` returns `map<string,string>` but callers
  previously received `requires standard module <map>` unless they manually added
  `include <map>`. The compiler now collects the standard modules referenced by every
  used builtin callable's parameters/results and builtin field types before analysis,
  so the API satisfies its own module requirement. Plain `map`/`set` declarations still
  require an explicit include when no builtin brings the module; runtime component
  slicing is unchanged.
- APF-H1 showcase certification: `examples/embed`, `examples/static-site` and
  `examples/one-binary-todo` are now certified by `tools/certify_showcase.py`
  (manifest-driven, compiled from their own directories so relative asset paths
  resolve) in the cross-platform matrix. They previously sat outside every
  certification path, which is how three showcase examples broke while the suite
  stayed green.
- APF-H1 API discoverability: `strut api` and `api --json` report the standard
  modules a callable's signature types require (`requires_modules` / "requires
  include <...>"), so agents can discover the required include without reading
  compiler source.

- **APF-1 — Struct encapsulation** (planned): private struct fields/methods and facade
  construction semantics. Excludes selective exports, method overloading, and the
  higher-order checked-error model.
- **APF-2 — Warden-shaped application validation** (planned): realistic backend +
  CLI/admin dogfood with early soak/adversarial testing.
- **APF-3 — Package visibility design** (planned): design only, using S0 as prior
  art/test specification.
- **APF-4 — Package-enablement correctness** (planned): individually gated work
  (higher-order checked errors, method error propagation, SQLite hardening, TTY
  detection, Unicode iteration, and any map-key/overloading work justified by APF-2).
- **APF-5 — Package ecosystem** (planned): sessions/auth/CSRF/JWT/SSE/multipart/
  OAuth/OIDC, biased to packages/application code.
- **APF-6 — Production/backend certification** (planned): soak, long-lived
  connections, slow peers, failure/restart, resources, deployment, performance, and
  justified runtime-hardening decisions.

Higher-order checked errors are a confirmed language soundness gap (function values do
not carry a checked-error set, allowing a callback whose creation error was handled to
cross an apparently no-throw boundary). It is deliberately deferred to APF-4a, not
addressed during APF-0.

### Documentation and publication

The website source is `strut-labs.github.io` branch `stage`; generated output is the nested `public/` repository on branch `main`. Edit source, run `nift build`, `nift status`, `python3 check_site.py`, compiler documentation certification and `git diff --check`, then commit both repositories. Publish generated `public/main` before source `stage` so source CI compares against the matching `SOURCE_DIGEST`. Pin current-development claims to the accepted compiler checkpoint and keep `v0.0.3` release claims separate.

### Repository safety

- Preserve `stash@{0}: On main: preserve interrupted P4 websocket scaffold before S0 revert`.
- Preserve the existing untracked `dogfood/__pycache__/` and `tools/__pycache__/` directories.
- Never delete or replace `.git` metadata.
- Do not use destructive cleanup on project repositories.
- Do not discard unrelated user or concurrent-agent changes.
- Do not tag or create a release as part of this closure.

## P1 crypto and encoding result

- `<crypto>` provides `secure_random_bytes`, `sha256`, `hmac_sha256` and `constant_time_equal`; `<encoding>` provides canonical padded Base64 and unpadded Base64url encoding and strict decoding. Cryptographic inputs and outputs remain binary-first, and OpenSSL failures use checked `CryptoError` values.
- Runtime slicing keeps encoding free of native dependencies and links crypto through OpenSSL 3 `libcrypto` without `libssl`. Mixed HTTP client, TLS server and crypto programs preserve static dependency order as `curl`, `ssl`, `crypto`.
- The canonical API registry drives semantic checks, runtime components, native-dependency metadata and LSP data. New builtin callable names are reserved against function-valued shadowing, while non-callable values with the same spelling do not select runtime code.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16; GCC ASan/UBSan passed CTest 16/16; all three builds passed known-answer, binary, random-length, strict-decoder and malformed-input certification; and the independent pinned regression suite passed 171/171.
- Final independent architecture/API, cryptographic security, generated-runtime/resource and test/portability reviews found no remaining actionable defect. Hosted CI remains the execution gate for Linux ARM64, macOS ARM64 AppleClang/Homebrew OpenSSL and Windows x64 MSVC/vcpkg.

## P2 buffered outbound HTTP result

- The existing libcurl client remains the sole outbound implementation. `http_get`, `http_request`, `http_get_ca` and the asynchronous wrappers share one bounded buffered engine; no streaming or public transfer-cancellation API was introduced.
- Requests validate absolute HTTP(S) URLs, methods, headers, strict JSON options and native ranges. Framing metadata remains transport-owned, explicit empty and embedded-NUL bodies retain exact lengths, TLS verification stays mandatory, HTTPS redirects cannot downgrade, and fully static libcurl links fail early rather than producing unresolved native dependencies.
- Responses enforce configurable body, cumulative header-byte and header-count limits. Only the final redirect block is exposed, names are lowercase, malformed status/header syntax, trailers and case-insensitive duplicates fail closed, and HTTP status errors remain ordinary responses. Native transport failures retain their `CURLcode` through `HttpError.code`.
- The process-lifetime libcurl initializer avoids cleanup racing asynchronous executor drain. Easy handles and request header lists use RAII, all native option/status results are checked, and allocation failures cannot unwind through libcurl callbacks.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16; GCC ASan/UBSan passed CTest 16/16 and the generated client certification; the deterministic peer suite covered binary bodies, strict metadata/options, limits, redirect state/method policy, timeout and async/shutdown behavior; TLS lifecycle covered rejection and an explicit CA path containing spaces; and the pinned independent suite passed 173/173.
- Final independent architecture/API, protocol-security, generated-runtime/resource and test/portability reviews found no remaining actionable defect. Hosted CI remains the execution gate for Linux ARM64, macOS ARM64, Windows x64, alternate supported libcurl versions and real proxy/TLS combinations.

## P3 outbound HTTP streaming result

- `http_request_stream` and `http_request_stream_async` exchange bounded owned `bytes` chunks through nullable upload/download callbacks, return final `http_response_head` metadata, and optionally bind the existing `cancellation_token`. No transfer handle or new reader/writer family escapes the call.
- Buffered and streaming calls use one libcurl request core, response-head parser, URL/header/options policy, TLS/proxy/protocol configuration, redirect rules, RAII and error mapping. The streaming runtime component composes the accepted P2 client with bytes and cancellation rather than introducing another engine.
- Uploads support deliberate known or unknown length without whole-body materialization. Producers are one-shot: 301/302/303 POST conversion is allowed, while 307/308 and other rewind requests fail deterministically. Downloads deliver only final-response binary chunks with synchronous callback backpressure; consumer early-stop is successful prefix consumption.
- Cancellation is cooperative through libcurl progress callbacks and reports `HttpError` code `-103`; callback failure uses `-104`, and replay refusal uses `-105`. No C++ exception crosses a libcurl callback. Async requests retain blocking easy handles on the shared executor, whose queue is now bounded with caller-runs saturation; a libcurl-multi reactor remains deferred.
- Deterministic certification covers 64 MiB upload/download transfers with unchanged RSS between 8 MiB and 56 MiB checkpoints, binary chunks, backpressure, limits, redirects, same/cross-origin credentials, callback failures, 20 cancellation-churn cycles, async behavior, proxy isolation, repeated completion, stable descriptors and TLS with an explicit CA. Unix-socket HTTP, client certificates and explicit proxy configuration remain unsupported rather than gaining streaming-only semantics.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16; GCC ASan/UBSan passed CTest 16/16; generated P1, P2 and P3 programs passed ASan/UBSan; P3 and HTTP worker concurrency passed TSan; the complete accepted HTTP/server, bytes, streams, cancellation, process, package and native-FFI chain passed; and the pinned independent suite passed 176/176. Final protocol/security, architecture/API and resource/portability reviews found no actionable Gate A defect. Hosted Linux ARM64, macOS ARM64 and Windows x64 remain required CI evidence.

## CP1 validation result

- CMake build: passed.
- CTest: 16/16 passed.
- HTTP lifecycle certification: passed.
- Backend baseline certification: passed; 40 sequential loopback requests completed in 0.018 seconds and 10 start/stop cycles completed.
- Native static/dynamic FFI linkage: passed.
- Independent regression suite: 155/155 passed.

## CP2 schema result

- The builtin registry now describes existing backend-relevant fields and methods for HTTP servers, streams, processes, sockets/listeners, threads, channels, mutexes, futures and SQLite.
- Source-level API names remain canonical. In particular, `http_server.static` remains the language member while code generation translates it to the native helper name `serve_static`.
- Semantic analysis uses registry signatures for builtin member return types, practical argument count/type checks and method checked errors. Existing container-specific and ownership-specific semantic rules remain local where they encode language behavior rather than API identity.
- LSP member completion and builtin field discovery consume the same registry metadata.
- Runtime component graph traversal now sizes state from the supplied graph rather than a fixed enum-sized array.
- CP2 adds no runtime capabilities and does not move generated runtime implementation boundaries; that remains CP3 work.
- Compiler-visible correction: registered builtin methods now reject wrong argument counts/types and require their declared checked errors. Specialized free functions retain their existing semantic rules.
- Validation: CMake build passed, CTest 16/16 passed, both HTTP certifications passed, native FFI linkage passed, and the independent regression suite passed 155/155.

## CP3 generated-runtime result

- `src/generated_runtime.cpp` and its private header now own component-specific emitted implementations for the executor, TCP socket/listener, HTTP client, HTTP request/response/query helpers and the active HTTP server lifecycle. `src/codegen.cpp` retains runtime selection and language lowering.
- Light and fallback generation compose those same emitters. Generated programs remain self-contained C++; the new files are compiler implementation files and are not headers consumed by generated programs.
- The obsolete `strut_http_server_legacy` implementation is no longer emitted. Only the lifecycle-aware server is present in generated server programs.
- Component slicing is preserved: hello programs omit networking, curl, OpenSSL, SQLite and process runtime; HTTP clients use curl without server or direct OpenSSL server TLS; plain servers omit curl and OpenSSL; TLS servers include OpenSSL without curl; SQLite-only programs omit HTTP.
- CP3 intentionally does not change bytes, streams, cancellation, HTTP semantics, WebSockets, PTYs, process behavior, SQLite behavior, crypto or archives.
- Code generation tests now check single emitted implementations and slicing, and compile representative hello, HTTP client, plain HTTP server, TLS HTTP server and SQLite programs.
- Validation: CMake build passed, CTest passed 16/16 including native compilation of the new representative fixtures, both HTTP certifications passed, native FFI linkage passed, and the independent regression suite passed 155/155.

## CP4 bytes result

- `bytes` is one owned, mutable binary value backed by contiguous `uint_8` storage. Assignment and parameter passing copy the value; equality compares contents.
- `bytes()` creates an empty value, `bytes(size)` creates a zero-filled value, and contextually typed literals accept values from 0 through 255.
- Integer indexing returns `uint_8` and supports mutation. Indexes and half-open `slice(begin, end)` ranges are runtime-bounds-checked; slices own a copy.
- `length()` returns `int_64`, `empty()` distinguishes zero length, and the addressable limit is the smaller of the native container limit and `int_64` maximum.
- `bytes.from_string` and `to_string` explicitly copy code units without UTF-8 validation. No implicit text/binary conversion exists.
- Filesystem `read_bytes`, `write_file` and `append_file` use the same bytes representation directly. The API registry owns bytes methods and filesystem overload metadata; sema retains contextual literal and indexed-mutation language rules.
- The bytes runtime component and implementation emitter add no native link dependency. Focused generated-code checks keep ordinary bytes programs free of networking, curl, OpenSSL, SQLite and process runtime.
- Validation: CMake build passed, CTest passed 16/16, both HTTP certifications passed, bytes certification passed, native FFI linkage passed, and the independent regression suite passed 157/157.

## CP5 stream result

- Existing `istream` and `ostream` are the generic binary contracts; no competing reader/writer family was introduced. Their text extraction/insertion operations remain available.
- Input streams add `read_bytes(max_bytes)`, `read_all_bytes(limit?)`, `eof()` and idempotent `close()`. Empty bytes indicate EOF only when `eof()` is true; zero-size reads do not change EOF; reads after close raise `StreamError`.
- Output streams add complete-write `write_bytes`, `flush` and idempotent `close`. Flush does not occur implicitly, native partial writes are completed internally, flush after close is a no-op, and writes after close raise `StreamError`.
- `ifstream` and `ofstream` implement the contract directly. Exact binary file behavior requires their existing `binary=true` mode. `sstream` implements the writer side and becomes closed after `close`.
- Process pipes expose structurally matching byte read/write methods using `ExecError`; they remain distinct move-only process handle types pending later process redesign.
- Stream implementations now have one owner in `generated_runtime.cpp`. The IO runtime component depends on bytes but does not introduce networking, process, curl, OpenSSL or SQLite dependencies.
- Blocking operations retain no public cancellation argument. Their stable read/write/close surface permits CP6-CP7 to wake or interrupt the native operation underneath without changing method signatures.
- Validation: CMake build passed, CTest passed 16/16, both HTTP certifications passed, bytes and stream certifications passed, native FFI linkage passed, and the independent regression suite passed 159/159.

## CP6 cancellation result

- `cancellation_source` and `cancellation_token` are copyable handles over shared reference-counted state. Source destruction does not imply cancellation, tokens outlive sources safely, and cancellation is one-way, thread-safe and idempotent.
- The public token supports non-blocking `cancelled()`, condition-variable-backed `wait()`, and checked `throw_if_cancelled()` using the subsystem-neutral `CancellationError`.
- CP6 exposes only explicit cancellation. The internal terminal-state boundary can add timeout, disconnect and shutdown reasons without replacing the public source/token types.
- Internal subscriptions close the registration/cancellation race under one state mutex. Cancellation extracts registrations before invoking callbacks, callbacks execute outside the state lock, and removal synchronizes with an in-flight callback.
- Cancellation is an independent runtime component with no networking, process, curl, OpenSSL or SQLite dependency. There is no process-global or thread-local current token.
- Validation: CMake build passed, CTest passed 16/16, both HTTP certifications passed, bytes and stream certifications passed, cancellation certification passed 50 repeated cycles with 32 waiters and 8 concurrent cancellers, native FFI linkage passed, and the independent regression suite passed 160/160.

## CP7 cancellable-handle result

- `process(program, args, token)` explicitly binds one copied cancellation token to the owning process-pipe context. The original two-argument constructor remains source compatible, and `read_bytes`, `read_all_bytes`, `write_bytes`, `eof`, `flush`, and `close` retain their Gate 2 signatures.
- Cancellation wakes blocked stdin writes and stdout/stderr reads and raises `ExecError` with message `process I/O cancelled` and code 125. It does not terminate or wait for the child. Independent process contexts use independent tokens.
- Completion and EOF win when observed in the same native wake cycle. A pre-cancelled token fails before I/O; cancellation wins over close or native failure when both are pending at interruption. Close alone remains distinct, peer close remains a checked I/O error, and process pipes do not yet expose timeout operations.
- Endpoint close and native-handle ownership are synchronized. Active operations retain the native handle until they finish, while close and cancellation wake them through operation-local objects. Concurrent operations on one endpoint are rejected; distinct endpoints and processes remain independent.
- Linux and macOS use nonblocking process descriptors with `poll` and an operation-local wake pipe. Existing thread-local SIGPIPE handling remains intact without holding the signal coordination mutex across blocking progress. Windows uses overlapped named pipes and operation-local events with `CancelIoEx`.
- Linux certification exercises 10 runs of 10 consecutive in-process cancellation cycles, plus pipe-capacity-blocked writes, independent tokens, close, EOF, normal completion and pre-cancellation. macOS and Windows paths are implemented but await their hosted CI runs.
- Validation: CMake build passed, CTest passed 16/16, both HTTP certifications passed, bytes, stream, CP6 cancellation and CP7 process-cancellation certifications passed, native FFI linkage passed, and the independent regression suite passed 161/161.

## CP8 strict HTTP framing result

- HTTP and HTTPS share one request-head parser for strict HTTP/1.0 and HTTP/1.1 request lines, origin-form targets, Host authority, field syntax and body framing. Public accepted header keys are lowercase and every repeated field is rejected while the public map remains single-valued.
- One unsigned decimal `Content-Length` is supported and bounded before allocation or body reads. Transfer-Encoding syntax is validated, every TE/CL combination fails with 400, malformed or non-final-chunked lists fail with 400, and valid unsupported chunked-final lists fail with 501.
- Request-head validation produces a framing result before exact bounded body acquisition. Premature EOF fails closed, bytes after the validated body cannot become another request, and malformed requests do not reach handlers. Buffered handlers and `Connection: close` remain unchanged.
- The black-box framing certification covers 51 plaintext syntax, limit and smuggling cases, HTTPS parser parity, and TLS head/body truncation. Abrupt TLS truncation may close without an HTTP response because the record channel is already broken. Cross-platform compiler CI runs the certification; no duplicate declarative regression fixture was added because that runner does not orchestrate raw sockets.
- Validation: CMake build passed, CTest passed 16/16, HTTP framing, lifecycle and backend baseline certifications passed, bytes, stream, CP6 cancellation and CP7 process-cancellation certifications passed, native FFI linkage passed, and the independent regression suite passed 161/161. The final adversarial security and portability review found no blocking defects; macOS, Windows and ARM64 execution remains delegated to hosted CI.

## CP9 bounded HTTP worker result

- Each listener generation owns a lazily grown reusable blocking worker set. The configured connection limit bounds queued plus running admissions, worker count and tracked sockets; completed work is reclaimed during service instead of being retained until shutdown.
- Admission is linearized against stop. Finite `max_requests` counts successfully admitted connections, including malformed requests and failed TLS handshakes, while saturation rejections do not consume the count. Plaintext saturation receives a bounded 503 path and pre-handshake TLS saturation closes.
- Listener and connected-socket native operations pin descriptor ownership. Listener close interrupts a short nonblocking `poll`/`WSAPoll` accept cycle, while forced connection close first wakes blocked I/O and waits for pinned operations before releasing the descriptor.
- Graceful stop uses one monotonic deadline across callers and the listener. Queued work is closed at expiry; running sockets are interrupted and closed. A non-returning handler is isolated in its retired generation, keeps `running()` true, and blocks restart or reconfiguration until completion. Synchronous and asynchronous handlers may initiate stop without waiting on themselves.
- TLS handshakes use nonblocking OpenSSL progress with `poll`/`WSAPoll` against the shortest configured read, write or idle timeout. Persistent workers clear the OpenSSL error queue between handshake attempts, and TLS reads, writes and shutdown participate in socket operation pinning.
- Black-box worker certification covers saturation and recovery, finite admission accounting, bounded shutdown, retired handlers, synchronous and asynchronous handler-initiated stop, silent TLS handshake expiry and worker reuse after timeout. Compiler CI runs it on Linux x64, Linux ARM64, macOS and Windows.
- Validation: GCC and Clang generated-code checks passed, CTest passed 16/16, worker, framing, lifecycle and backend baseline certifications passed, bytes, stream, CP6 cancellation and CP7 process-cancellation certifications passed, native FFI linkage passed, and the independent regression suite passed 161/161. Repeated adversarial concurrency, ownership and portability review found no blocking defects; hosted CI remains the execution gate for macOS, Windows and ARM64-specific paths.

## CP9A HTTP foundation corrections result

- Buffered HTTP responses are serialized through one validation boundary before any bytes are committed. Status codes are limited to 200 through 599, content types follow media-type syntax, field names follow HTTP token syntax, and field values reject response-splitting controls. Case-insensitive duplicate and transport-owned `Content-Type`, `Content-Length`, `Transfer-Encoding`, and `Connection` fields are rejected. Invalid metadata produces a fixed safe 500 response.
- Responses for 204, 205 and 304 reject non-empty bodies. The transport omits `Content-Length` for 204 and 304, emits zero length for 205, owns all framing fields, and retains the buffered `Connection: close` contract. Plain and TLS native writes cap each operation to the platform API's integer range.
- Server lifecycle transitions are explicit: stopped, starting, running, stopping and stopped. A pending generation is published before bind, startup completion is part of drain completion, and stop cannot publish stopped or permit restart while startup could still acquire or own a listener. Generation checks prevent stale startup and worker publication.
- Response certification covers valid custom metadata and 21 rejection cases with a healthy follow-up request after each rejection. Worker certification adds startup/stop races, concurrent stop callers, bounded shutdown timing, 600 sustained requests, native thread bounds, handler self-stop and TLS handshake expiry. The backend baseline covers 500 sequential requests and 20 start/stop generations that each serve a real request.
- Cross-platform compiler CI runs response and backend baseline certification in addition to the existing HTTP gates. Release packaging is gated per platform by CTest and the HTTP framing, response, worker, lifecycle, backend baseline, bytes, stream, cancellation and process-cancellation suites; POSIX packages additionally run native FFI linkage. Windows PowerShell steps fail immediately on native command errors.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the ASan/UBSan build passed CTest 16/16 and pointer/thread stress, all HTTP and non-HTTP runtime certifications passed, package and dogfood checks passed, native FFI linkage passed, and the independent regression suite passed 161/161. Repeated adversarial response-security, lifecycle, resource and release-gate reviews found no blocking defects; hosted CI remains the execution gate for macOS, Windows and ARM64-specific paths.

## CP10 streaming response result

- `http_response_writer` provides precommit status, content type, custom header and exact-length configuration plus incremental text/bytes writes, flush and idempotent finish. `get_stream` and `post_stream` add synchronous writer handlers without changing buffered handler signatures.
- Buffered, static, error and streaming responses share one uncommitted/committed/finished state machine and the CP9A metadata validator. Invalid precommit state can produce the fixed safe 500; committed failures close without a second response. Escaped writer copies are invalidated when their handler returns.
- Known-length output uses transport-owned Content-Length. Unknown-length HTTP/1.1 output uses internally generated chunk sizes and one terminal chunk; HTTP/1.0 output is close-delimited. Writes apply blocking socket backpressure without retaining the whole response, and plaintext/TLS use the same framing path.
- Response-stream certification covers many small writes, a 64 KiB NUL-bearing binary write, prompt flush, known/unknown lengths, HTTP/1.0, empty and forbidden bodies, invalid metadata, pre/postcommit failures, terminal-state operations, disconnect recovery, blocked-write shutdown and TLS parity. Cross-platform and release workflows run the certification.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the ASan/UBSan build passed CTest 16/16 and pointer/thread stress, all HTTP and non-HTTP runtime certifications passed, native FFI and package certification passed, and the independent regression suite passed 161/161. Repeated response-security and resource/lifecycle review found no remaining blocking CP10 defect; hosted CI remains the execution gate for macOS, Windows and ARM64-specific paths.

## CP11 streaming request result

- `http_request_body` provides binary `read_bytes`, bounded `read_all_bytes`, EOF and close operations. `get_request_stream` and `post_request_stream` supply it with the CP10 writer, while existing buffered and response-stream handlers retain their prior signatures and body behavior.
- CP8's request-head parser remains authoritative. Its validated fixed-length or sole HTTP/1.1 chunked framing result creates one body state; buffered `request.body` consumes that reader, so there is no parallel parser or decoder. TE/CL remains rejected and unsupported coding chains remain 501.
- Chunk decoding validates hexadecimal sizes, quoted/token extensions, exact CRLF, decoded-size overflow, aggregate framing overhead, the zero chunk and an empty trailer section. Non-empty trailers and `Expect` are rejected. Transport/TLS failures normalize to `NetworkError`.
- Reader copies share one consumption state, concurrent reads fail, and invalidation interrupts an escaped active read before transport destruction. Request-body reads and response commitment are serialized: commitment fails during an active or failed read, and later reads fail. A handler may leave a body unread only because CP11 closes after the response; CP12 must not reuse that connection without a terminal body.
- Request-stream certification covers fixed and chunked binary bodies, byte-fragmented framing, extensions, zero reads, active close, early response, buffered compatibility, escaped readers, unread bodies, caught framing failures, malformed/overflow/truncated chunks, payload and framing limits, TE/CL, unsupported coding, trailers, `Expect`, blocked-read shutdown and TLS parity. Cross-platform and release workflows run the certification.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the GCC ASan/UBSan build passed CTest 16/16 and pointer/thread stress, all HTTP and non-HTTP runtime certifications passed, native FFI and package certification passed, and the independent regression suite passed 161/161. Final request-framing/security and resource/lifecycle reviews found no remaining actionable CP11 defects. The local Clang sanitizer build could not link because its installed LLVM 21 toolchain lacks the ASan runtime archives; GCC sanitizer coverage passed, and hosted CI remains the execution gate for macOS, Windows and ARM64 paths.

## CP12 HTTP connection lifecycle result

- HTTP/1.1 connections persist by default and HTTP/1.0 persists only with `Connection: keep-alive`; token parsing is case-insensitive, explicit close is honored, and idle reuse is bounded by the configured idle timeout. Requests, including pipelined requests, are dispatched sequentially in wire order.
- The CP8 parser and CP11 body reader remain authoritative. Validated post-body carry bytes feed the next request head, and reuse requires body EOF plus a finished self-delimited CP10 response. Unread bodies, malformed framing, close-delimited output, transport failures and shutdown close the connection. HEAD reuses GET routing while suppressing body bytes, and upgrade detection is retained as a future ownership seam that currently responds 501.
- Every dispatched `http_request` exposes one read-only `cancellation_token` backed by a fresh internal source. Normal completion and every terminal request path cancel it, while active sources also participate in server shutdown. Handler-created Strut threads inherit execution context so joined nested work preserves handler-initiated stop semantics. Peer-disconnect notification remains cooperative for CPU-only handlers.
- Persistence certification covers HTTP/1.0 and HTTP/1.1 reuse, close policy, ordered pipelining, fixed and chunked carry-over, unread-body closure, HEAD, idle expiry and TLS parity. Request-cancellation certification covers normal completion, distinct keep-alive lifetimes, read/write disconnects, nested process I/O cancellation, repeated nested-thread stop, idle shutdown and TLS-relevant lifecycle paths. Cross-platform and release workflows run both suites.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the GCC ASan/UBSan build passed CTest 16/16 and pointer/thread stress, all HTTP and non-HTTP runtime certifications passed, package, dogfood and native FFI checks passed, concurrency stress passed 20/20, and the independent regression suite passed 161/161. The backend baseline passed in isolation with 500 sequential requests and 20 start/stop cycles after one timing failure while all validation suites ran concurrently. Final protocol/security, lifecycle and API/compiler reviews found no remaining actionable CP12 defects. Hosted CI remains the execution gate for macOS, Windows and ARM64 paths.

## CP13 HTTP application helper result

- `http_values` provides first-value, all-values-in-wire-order and presence lookup for decoded query parameters, request cookies and URL-encoded forms. `request.query` remains the raw last-value compatibility map. Strict percent decoding, form-style `+`, empty names/values, malformed escapes and encoded controls have explicit behavior. Query pair count follows the header-count limit and forms accept at most 1024 pairs to prevent separator-driven allocation amplification.
- Buffered requests add optional-limit `text`, `json` and `form` helpers over the CP11-acquired body. Request-stream handlers retain `http_request_body` as the only consumer and receive `HttpError` from buffered helpers. Existing no-argument `request.json()` remains source compatible and now normalizes parse failure to its registered checked error.
- Structured `http_cookie` values cover Path, Domain, Max-Age, Expires, Secure, HttpOnly and SameSite. Buffered responses and streaming writers pass ordered cookies to the common CP9A/CP10 serializer, which emits distinct Set-Cookie lines. Generic Set-Cookie remains reserved and arbitrary duplicate-header validation is unchanged. `http_redirect` adds only the standard 301, 302, 303, 307 and 308 statuses.
- Application-helper certification covers repeated and empty query/form values, percent and plus decoding, duplicate request cookie names, Warden-shaped cookie attributes, multiple Set-Cookie fields, invalid query/cookie input, generic Set-Cookie rejection, fixed/chunked bodies, JSON compatibility, helper/stream ownership conflict, redirects, persistent reuse and TLS parity.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the GCC ASan/UBSan build passed CTest 16/16 and pointer/thread stress, and the complete serial HTTP certification chain from framing through backend baseline passed, including application-helper plaintext and TLS coverage. Final architecture, security and API/compiler reviews found no remaining actionable CP13 defects. Hosted CI remains the execution gate for macOS, Windows and ARM64 paths.

## CP14 static file and range response result

- `http_serve_file(request, writer, path, content_type?)` opens one explicit binary file, fixes the representation to its initial size and streams bounded 64 KiB chunks through the CP10 response writer. It infers a conservative media type by extension unless one is supplied and reports filesystem versus transport failures through `FilesystemError` and `NetworkError`.
- GET supports one overflow-checked byte range in closed, open-ended or suffix form. Satisfiable ranges produce 206 and exact `Content-Range`/Content-Length metadata; valid unsatisfiable ranges produce empty 416 responses; malformed, unknown-unit and multipart ranges are ignored. HEAD reports the full representation without reading body bytes.
- File and range responses retain the CP10/CP12 response state, framing, backpressure, cancellation, persistence, TLS and postcommit-failure behavior. The helper does not map or authorize URL paths, enforce filesystem containment, or add a plaintext/TLS/sendfile path.
- Static-file certification covers binary and empty files, multi-chunk reads, inferred and explicit media types, range forms and boundaries, overflow, malformed/multipart fallback, 206/416 metadata, HEAD, persistence, missing files and TLS parity. Cross-platform and release workflows execute it.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the GCC ASan/UBSan build passed CTest 16/16 and pointer/thread stress, and the complete serial HTTP certification chain through backend baseline passed with the new static-file suite. Final protocol/security and architecture/API/resource reviews found no remaining actionable CP14 defects under the documented trusted immutable-path boundary. Hosted CI remains the execution gate for Windows UTF-8 path handling, macOS and ARM64 paths.

## CP15 NDJSON streaming result

- `http_write_ndjson(request, writer, value)` checks the request cancellation token, rejects non-finite numbers, invalid UTF-8, non-canonical exact numbers and more than 512 nested levels through `HttpError`, uses JSONIC's compact serializer for one complete value, appends exactly one LF and synchronously writes and flushes that record through the CP10 writer. The first record selects `application/x-ndjson`; each call retains only its own serialized record.
- JSON escaping prevents data from injecting record delimiters. HTTP/1.1 chunking and HTTP/1.0 close delimitation remain transport framing owned by CP10, independent of NDJSON records. Precommit failure can produce the safe 500; cancellation, disconnect or failure after a record commits closes without a synthetic terminal record or second response.
- The dedicated `http_ndjson` runtime capability composes JSON and the HTTP server only when referenced. Existing response writers and JSON output remain source compatible; applications migrate streaming JSON loops by replacing manual `dump + newline + write + flush` code with the helper, while empty streams set the NDJSON content type explicitly.
- NDJSON certification covers compact parseable records, escaped embedded newlines, invalid UTF-8 and non-finite rejection, exact LF framing, content type, prompt first-record delivery, HTTP/1.0 and HTTP/1.1 framing, persistent reuse, explicit shutdown cancellation, disconnect recovery and plaintext/TLS parity. Cross-platform and release workflows execute it, with generated ASan/UBSan execution on Linux GCC.
- Validation: warning-clean GCC and Clang builds passed CTest 16/16, the GCC ASan/UBSan build passed CTest 16/16 and pointer/thread stress, generated NDJSON plaintext/TLS execution passed ASan/UBSan, all serial HTTP certifications through backend baseline passed, and the independent regression suite passed 167/167. Final protocol/security, architecture/API/compiler, and resource/test/portability reviews found no remaining actionable CP15 defects. Hosted CI remains the execution gate for Windows, macOS, and ARM64 paths.

## Review Gate 5 result

- The integrated CP8-CP15 review found and closed request-target, parser-error, semantic-validation, cancellation, timeout, sanitizer and platform-fixture gaps without introducing a second HTTP pipeline. Percent-encoded controls now fail before dispatch, and recognized HEAD/HTTP versions survive parser and pre-parser failures, including oversized request lines.
- Canonical registry signatures now validate free HTTP helper arity and argument types. File responses check cancellation before setup as well as during bounded reads, and socket timeout installation fails closed.
- Windows certification uses native `SO_LINGER` layout and portable generated paths; macOS ARM64 jobs assert their runner architecture. TLS workers release OpenSSL thread-local state, and self-stopping certification handlers use explicit non-owning server references rather than shared-ownership cycles.
- Final validation passed warning-clean GCC and Clang CTest 16/16, GCC ASan/UBSan CTest 16/16 and pointer/thread stress, 167/167 independent regressions, the complete serial release-runtime/HTTP/FFI chain, generated HTTP ASan/UBSan coverage across framing, request/response streaming, files, NDJSON, persistence, cancellation and workers, and the generated worker suite under GCC ThreadSanitizer. Independent protocol/security, architecture/API/compiler, and resource/test/portability reviews reported no remaining actionable Gate 5 defects. Hosted CI remains the execution gate for Windows x64, macOS ARM64, and Linux ARM64.

## POSIX PTY P8 result

- The P7 binary PTY adds validated `resize`, fixed interrupt/terminate/kill/hangup methods, and cancellation-aware `wait` without adding another token or buffering system. Resize relies on kernel `SIGWINCH`; signals validate leader and terminal-session identity before trusting the kernel foreground PGID, including groups whose numeric leader PID has exited, and otherwise use the pinned leader group.
- Lifecycle ownership remains shared and single-reaper. One reader and one writer may overlap; duplicate directions reject; close, resize, signal, status, and wait races are synchronized; blocked I/O and wait wake on the bound token. Close publishes lifecycle intent before I/O shutdown, so close wins over later cancellation without cancellation turning bounded cleanup polling into a tight loop. Same-group descendants receive ordinary leader-group cleanup; other background/detached groups remain an explicit POSIX limitation.
- The runtime composes existing bytes, cancellation, and process helpers. Structural tests prove the slice excludes HTTP, WebSockets, TLS, and third-party terminal libraries.

## Windows ConPTY P9 result

- P9 keeps the P7/P8 `pty` surface unchanged and replaces the Windows stub with dynamically resolved native ConPTY on Windows 10 1809/build 17763 and Windows Server 2019 or newer. Older systems start normally and fail only `pty_spawn` with a deterministic `PtyError`.
- The backend creates non-inheritable synchronous ConPTY ends and overlapped host ends, attaches the opaque handle through `STARTUPINFOEXW`, assigns the suspended child to a kill-on-close Job, and resumes only after containment. A weak process monitor caches leader completion and terminates Job descendants without retaining PTY state or closing the pseudo-console. Pending reads independently initiate asynchronous pseudo-console closure while draining final output; exited-but-undrained PTYs do not accumulate closer threads. Explicit cleanup retires output before closure, handling pre-26100 output-flush behavior without a shared-state ownership cycle.
- Windows controls are documented terminal/lifecycle approximations: resize uses ConPTY, interrupt writes ETX, hangup closes input, terminate retires unread output and closes the pseudo-console, and kill terminates the Job with status 137. Natural leader exit also terminates remaining Job descendants. Dimensions are uniformly limited to 1 through 32767.
- Internal named pipes use system-random names, first-instance and remote-only rejection, and an owner/SYSTEM-only DACL. Executable Windows certification covers spawn overloads, argv/env/cwd/geometry, merged output, interactive input, cancellation, controls and races, descendant and last-owner cleanup, 300-cycle churn, and HANDLE/thread return. The PTY slice remains independent of HTTP, WebSockets, and terminal-emulation libraries.
