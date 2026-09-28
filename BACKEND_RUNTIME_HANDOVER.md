# Backend runtime expansion handover

This document records the backend/runtime expansion that starts after Strut
0.0.3. It is deliberately separate from the compiler bootstrap checkpoint
history in `IMPLEMENTATION_HANDOVER.md`.

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
