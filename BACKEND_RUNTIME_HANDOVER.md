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

These are accepted baseline facts, not certified desirable behavior:

- server request bodies are completely buffered before dispatch;
- server responses are completely buffered before writing;
- successful and error responses use `Connection: close`;
- only exact-case `Content-Length` controls request framing;
- `Transfer-Encoding` and chunked request bodies are unsupported;
- request headers are case-sensitive and duplicate values are overwritten;
- no protocol-upgrade or WebSocket path exists;
- handlers receive no request cancellation or disconnect signal;
- no streaming request-body or response-writer abstraction exists;
- one native thread is created per accepted connection;
- completed server workers and socket records are retained until listener shutdown;
- light and full generated runtimes duplicate networking, HTTP and executor implementations;
- API knowledge is split among the registry, semantic analysis and code generation;
- libcurl responses are unbounded buffered strings and transfers cannot be cancelled;
- SQLite rows pass through JSON, lose exact large-integer semantics and do not expose BLOB values;
- process pipes are string-oriented and process groups, timed waits and descendant cleanup are absent.

## Initial concurrency decision

The critical path retains a bounded blocking/threaded architecture. HTTP/1.1,
streaming, cancellation, upgrades and moderate long-lived connection counts do
not intrinsically require epoll, kqueue or IOCP. Public ownership, stream and
cancellation contracts must not prevent a future event-driven backend.

## Checkpoint status

- CP1: baseline and scope freeze complete.
- CP2: canonical builtin API schema complete.
- CP3: generated runtime implementation boundaries pending.
- Review Gate 1 follows CP3. Do not begin bytes or stream implementation before approval.

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
