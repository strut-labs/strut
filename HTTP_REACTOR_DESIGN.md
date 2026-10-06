# HTTP Server Runtime: Reactor + Fixed Worker Pool — Architecture Review

Status: APPROVED WITH AMENDMENT (R0). Design only; no runtime implementation.
Scope: replace the thread-per-connection HTTP execution model in the generated
runtime (`src/generated_runtime.cpp`, `emit_server`) with a reactor + bounded
CPU-scaled application worker pool.

This document reconstructs the current model from source, confirms the measured
defect, compares candidate architectures, recommends one, and proposes a
reversible checkpoint campaign.

## Amendment (R0) — worker/I/O boundary

Option A is approved, **but the fixed worker pool must not ultimately own
blocking socket I/O for an entire request.** A CPU-sized pool that blocks inside
ordinary network I/O can be starved by a single slow upload, slow reader, SSE
stream, large file, WebSocket, or blocking handler-side network operation — on
1 vCPU, one pathological connection could occupy the only application worker.
That would trade thread-per-connection contention for worker starvation /
head-of-line blocking.

Required principle: **separate transport progress from application execution.**

- Reactor/I/O engine owns: accept, idle connections, socket readiness,
  incremental request-head reads, ordinary request-body transport, partial
  response writes, output readiness, idle deadlines, disconnect/error readiness.
- Application worker pool (CPU-scaled) owns: routing/callback execution, CPU
  work, response construction, explicit blocking application work.

**Ordinary buffered path (target for the benchmark):** reactor incrementally
reads head/body → complete request becomes runnable → worker runs route/handler
→ worker produces response → response handed back to reactor → reactor drains it
via writable readiness → connection returns to keep-alive idle.

**Streaming/long-lived path (resolved at R7):** either (A) reactor-driven
streaming through bounded per-connection adapters/queues, or (B) a separate,
clearly-bounded transitional long-lived pool. Bounded adapters are preferred;
a separate pool is acceptable as a transitional checkpoint but must not scale
native threads with long-lived connections as the final model.

**Bounded queues only.** Request bodies bounded by `max_body_bytes`; per-
connection output bounded; optional global output bound. When a producer
outruns the network it must block/yield/fail per documented semantics — never
grow memory without bound.

The blocking-looking Strut streaming API is preserved; a bounded
producer/consumer bridge is acceptable. No new async syntax, continuations,
public handler APIs, or compiler changes.

Workers ≈ available CPU parallelism (documented cap). On 1 vCPU this is 1
application worker plus reactor/control thread(s) — but only meaningful once the
worker is *not* sitting in ordinary blocking socket waits.

---

## 1. Current server architecture (reconstructed from source)

All references are `src/generated_runtime.cpp` (generated server runtime).

### 1.1 Entry points

- `strut_http_server::listen(host,port,max_requests)` (1033) calls
  `run_server(host,port,max_requests, process, reject)` where
  `process = serve_connection(s, worker_run_, worker_connection_, socket)`
  and `reject` writes `503`.
- `listen_tls(...)` (1105) opens an `SSL_CTX`, and in the accept callback does a
  *blocking* `SSL_accept` with `poll` slices (`tls_accept_until`, 1085), then
  wraps the fd in `tls_socket` and calls the same `serve_connection`.

### 1.2 Accept + worker spawning (`run_server`, 1148)

- The calling thread runs a single accept loop.
- Per accepted socket it builds `connection` (shared_ptr, 1124) and, under
  `run->mutex`, lazily spawns a worker **to match in-flight connections**:

  ```cpp
  if (workers.size() <= run->in_flight && workers.size() < max_connections)
      workers.emplace_back([s, run, process]{ worker_loop(s, run, process); });
  ```

- It pushes the connection onto `run->queue` and notifies `work_cv`.
- `max_connections` defaults to 1024 (1126) → **up to ~1024 OS threads**.

### 1.3 Worker semantics (`worker_loop`, 1147)

Each worker:
1. blocks on `run->work_cv` for a queued connection,
2. sets thread-locals `worker_run_` / `worker_connection_`,
3. calls `process(connection->socket)`.

`process` = `serve_connection` (1184), which runs the **entire keep-alive
lifecycle of that one connection synchronously** and only then returns the
worker to the pool. So a worker is occupied for the whole life of a keep-alive
connection, not per request.

**This is thread-per-connection (lazily grown, pooled).** Measured: 51 threads
at c=50.

### 1.4 Connection loop (`serve_connection`, 1184–1217)

Per iteration:
- `begin_idle` / `set_idle` (state under `run->mutex`),
- `set_socket_timeouts(idle)` → blocking `socket.read(4096)` until `\r\n\r\n`,
- `set_socket_timeouts(read)`, parse head (`strut_parse_http_request_head`, 872),
- dispatch route; buffered handlers read the whole body (`read_buffered_body`),
  streaming handlers pull the body; the handler runs on this worker,
- write response via `response_writer` (head then body),
- decide keep-alive reuse from `reusable()` / `release(next)`; loop.

### 1.5 I/O layer

- `strut_tcp_socket` (607): `write` is a **blocking `send` loop** (625); `read`
  is a **blocking `recv`** (638) relying on `SO_RCVTIMEO` on POSIX; Windows read
  uses `WSAPoll` 100 ms slices checking `interrupted`.
- `strut_socket_state` (589): refcounted `operations`, `closing`, `interrupted`;
  cancellation works by `shutdown()` (POSIX) / `SD_BOTH` (Windows).
- `strut_tcp_listener::accept` (661): non-blocking accept with a 50 ms poll.
- Timeouts are per-phase `SO_RCVTIMEO`/`SO_SNDTIMEO` (`set_socket_timeouts`,
  1149) plus explicit poll on Windows.

### 1.6 Streaming and body

- `response_writer` (815): synchronous; the `send` lambda writes directly to the
  socket (1158). Chunked framing, `flush`, `account_head_body` for HEAD.
- `request_body` (1166): synchronous pull reader with `read`/`interrupt`.
- No output queue exists. A slow client blocks the owning worker in `send`.

### 1.7 Async runtime

- `strut_executor` (535): a **blocking-submit thread pool**
  (`hardware_concurrency` clamped to [2,32], `max_queue = n*8`, runs inline if
  called from the same executor or the queue is full).
- `strut_async` submits; `strut_await` calls `future.get()` and blocks.
- The HTTP server does **not** use this executor. `get_async`/`post_async`
  (1019–1020) wrap the future-returning handler and `strut_await` it **on the
  server worker**, so an async handler still occupies an OS worker thread for
  its whole duration.
- There is **no coroutine/continuation mechanism**. The async runtime is
  thread-based, not I/O-async.

### 1.8 Shutdown / cancellation

- `request_stop` (1143): clears `accepting`, cancels all request cancellation
  sources, `shutdown_io()` on idle/websocket connections, closes the listener,
  notifies `work_cv`.
- `wait_for_drain` (1145): waits until `in_flight == 0` or the shutdown
  deadline; then `force_shutdown_locked` (1144) closes everything.
- Workers are joined if drained, else detached.
- Per-request cancellation: `request_scope` (1142) creates a
  `strut_cancellation_source`; client disconnect is observed as `recv == 0`.

### 1.9 Ownership today

| object | owner |
|---|---|
| native fd | `strut_socket_state` (shared_ptr) via `strut_socket_operation` refcount |
| connection | `std::shared_ptr<connection>` in `run->queue`/`run->connections` |
| parser buffer | local `std::string raw` in `serve_connection` (reused across keep-alive) |
| input accumulation | `raw` / `next` move |
| output | none (direct blocking writes) |
| request/response | stack locals in `serve_connection` |
| cancellation | `shared_ptr<strut_cancellation_source>` in `request_scope` |

---

## 2. Confirmed measured defect

Per-request CPU cost (single vCPU, all three pinned to CPU 0, Go `GOMAXPROCS=1`,
Rust `available_parallelism=1`):

| load | Strut | Go | Rust |
|---|---|---|---|
| c=1  | ~80 µs | ~63 µs | ~25 µs |
| c=50 | ~106 µs | ~39 µs | ~25 µs |

Strut's per-request cost **increases** with concurrency; Go/Rust do not. This is
the signature of contention, not raw instruction count:

- 51 OS threads at c=50 (Go 4, Rust 2);
- ~6.5 syscalls/request (Go/Rust ~3.0);
- ~34× Go's context-switch count (direct switch cost ≈ 3.5% CPU).

The extra cost is scheduler pressure, cache churn, allocator contention, lock
contention and reduced locality from time-slicing dozens of connection threads
on one core. This is now an architectural finding, not speculation. P4 (redundant
`setsockopt`) and P5 (head/body coalescing, <2%) do not explain the deficit.

**Target of the redesign:** make c=50 cost approach the c=1 cost (remove the
concurrency inflation), keeping the public API and streaming semantics.

---

## 3. Candidate architectures

### Option A — One reactor thread + fixed CPU-sized worker pool

One (or one-per-CPU) reactor owns all fds and readiness; a bounded worker pool
sized to CPU parallelism runs request logic.

- Complexity: **medium**.
- Synchronization: reactor mutex + per-connection state; worker handoff.
- Locality: good (workers = CPUs; idle connections cost no thread).
- Scalability: threads ≈ CPUs regardless of connections.
- Streaming: depends on sub-design (see §4).
- Cancellation/shutdown: reactor wakeup; in-worker I/O unchanged.
- Platforms: epoll / kqueue / (Windows readiness) — see §6.
- Memory: bounded; no per-connection thread stack.
- Latency: +1 handoff per request vs today.
- Throughput: removes contention inflation.
- API compat: **unchanged** (internal only).

### Option B — Per-CPU reactor + worker

Each CPU has a reactor and its own workers; connections are sharded across CPUs.

- Complexity: **medium-high**.
- Synchronization: lower cross-CPU contention (great for N vCPU).
- Locality: best at N>1; adds sharding/accept distribution (SO_REUSEPORT or
  accepted-fd handoff) complexity.
- Single-vCPU behavior: indistinguishable from A (one shard).
- Cancellation/shutdown: per-shard coordination needed.
- Recommendation: **the natural R2** of Option A, not the first step. It exists
  to make the multi-vCPU scaling test (post-single-core) strong.

### Option C — Reactor + existing `strut_executor`

Reuse `strut_executor` as the HTTP worker pool.

- The existing executor is a **blocking-submit** pool; `strut_await` blocks the
  calling thread. It provides no continuations, so it cannot drive non-blocking
  I/O. Reusing it for HTTP yields: reactor detects readiness → submits a job →
  the job does blocking I/O → still occupies a thread per in-flight request; and
  `max_queue`/inline-run semantics interact badly with backpressure.
- It also entangles HTTP lifecycle/shutdown with a global executor used by
  unrelated user `strut_async` calls.
- Recommendation: **do not reuse as-is.** Keep the HTTP pool distinct. Optionally
  extract a shared *lower-level* executor abstraction later; not now.

### Option D — Completion-based engine (IOCP-style) end to end

One portable "connection engine" whose backend is completion-based where
available.

- Complexity: **very high**; IOCP semantics differ fundamentally from
  epoll/kqueue; risks awkward artificial abstraction on Unix.
- Not justified for the first campaign.

**Recommended: Option A now, structured so Option B is a contained follow-up.**

---

## 4. Recommended design — reactor-owned transport + CPU-scaled application pool

The key idea (amended): the reactor owns ordinary transport waits and
incremental framing; a complete buffered request is dispatched to a CPU-scaled
application worker; the worker produces a response that is handed back to the
reactor, which drains it via writable readiness. The reactor never executes user
code. Application workers do not block on ordinary socket I/O.

The synchronous-looking Strut API is preserved behind a bounded
producer/consumer bridge. Streaming/long-lived surfaces are handled by a
reactor-driven bounded adapter (preferred) or a bounded transitional pool
(R7 decision).

### 4.1 Lifecycle (ordinary buffered request)

```
                              accept (reactor)
                                   |
                                   v
   +------------------------------ reactor (1 thread) ------------------------------+
   | owns: listener, epoll/kqueue/WSAPoll, ALL idle/keep-alive connections,          |
   |       incremental request-head/body reads, per-connection output queues,        |
   |       writable-readiness draining, idle deadlines, wakeup (eventfd/pipe)        |
   +---------------+------------------------------------------------+----------------+
                   | complete request assembled                     ^ response ready
                   | -> dispatch job (bounded queue)                | (reactor drains
                   v                                                |  via POLLOUT)
         +-----------------------------------+                     |
         | application worker pool (N=CPUs)   |---------------------+
         |  route / handler execution         |
         |  CPU work, response construction   |
         |  (no ordinary blocking socket I/O) |
         +-----------------------------------+
                   |
                   | long-lived/streaming surface -> bounded adapter
                   | (R7: reactor-driven queue, or bounded transitional pool)
                   v
```

Connection ownership is explicit: a connection is in exactly one of the reactor
set, a worker job, or the closing list at any instant. For the buffered path the
reactor retains ownership of the socket; workers receive a request object and
return a response object (no socket ownership).

### 4.2 Connection state machine

`accepting → reading_head → reading_body → dispatching → writing_response →
keep_alive_idle → (reading_head | closing)`, plus `streaming` (request or
response), `websocket`, `closing`, `shutdown`.

Each transition has one owner at a time (reactor XOR worker). Ownership transfer
is explicit (a connection is in exactly one of: reactor set, worker job, or
closing).

### 4.3 Worker scheduling

- Pool size = `available_cpu_parallelism` (documented formula:
  `max(1, hardware_concurrency)`; no artificial minimum of 2 like the current
  executor; cap at a documented bound). On the 1-vCPU benchmark: **1 application
  worker + reactor/control thread(s)**, versus 51 today — valid only because the
  worker does not block on ordinary socket I/O.
- Workers are dispatched a *complete runnable request*; they never own the
  socket for ordinary traffic and never block on network reads/writes.
- Pipelining: the reactor can assemble multiple buffered requests from one read
  and dispatch them in order; a worker may process a runnable request without a
  reactor round-trip, but response bytes always flow through the reactor output
  queue.
- A separate transitional long-lived/streaming pool (if chosen at R7) is capped
  and documented; it must not be the final model.

### 4.4 Backpressure and streaming (hard requirement)

- **Bounded queues only.** Request bodies bounded by `max_body_bytes`;
  per-connection output queue bounded; optional global output bound. A producer
  that outruns the network blocks/yields/fails per documented semantics rather
  than growing memory.
- **Buffered path:** the worker returns a complete response; the reactor drains
  it via writable readiness with partial-write handling. Slow receivers cost
  reactor queue capacity, not application workers.
- **Streaming/long-lived surfaces** (`response_writer`, chunked framing,
  `flush`, NDJSON, `serve_file`, request streaming, SSE, WebSocket) are bridged
  by a bounded adapter: `write(...)` feeds a bounded per-connection queue and
  blocks when full; the reactor drains on write readiness. Preferred over a
  separate pool, but a bounded transitional pool is acceptable at R7.
- **Documented trade-off:** until the full reactor bridge lands (R7), some
  long-lived surfaces may be served by the bounded transitional pool. Ordinary
  buffered HTTP performance must not regress because streams/WebSockets are
  open.

### 4.5 Timeouts

- **Idle/keep-alive**: reactor-owned deadlines (min-heap/timer wheel), not
  repeated `SO_RCVTIMEO`. Idle connections are parked in the reactor; on expiry
  the reactor closes them.
- **Read/write during a request**: unchanged in-worker `SO_RCVTIMEO`/
  `SO_SNDTIMEO` (already set per phase). Externally visible timeout categories
  (read / write / idle / shutdown) are preserved.
- Any change to *when* a peer observes a timeout must be proven equivalent by
  tests; otherwise keep the current in-worker mechanism.

### 4.6 Cancellation and shutdown

- Client disconnect: during worker processing, the blocking read returns 0 /
  errors and the existing path closes; while parked in the reactor, `POLLHUP`/
  `POLLERR`/EOF unregisters and closes.
- `stop()`: set `accepting=false`, wake the reactor via `eventfd`/pipe, close
  the listener, cancel in-flight sources, `shutdown_io()` connections parked
  idle; `wait_for_drain` until `in_flight == 0` or deadline; then force-close.
- Reactor wakeup during shutdown is mandatory (the reactor blocks in `wait`).
- Invariants: no callback after connection destruction; no use-after-free; a
  connection is closed only by its current owner; workers never touch a released
  connection (shared_ptr + explicit state transfer).

### 4.7 Ownership model

| object | owner | notes |
|---|---|---|
| native fd | `strut_socket_state` shared_ptr (existing) | keep refcounted `operation` guard |
| connection | `shared_ptr<connection>` | exactly one of: reactor set / worker job / closing list |
| reactor interest | reactor | register/unregister on readiness changes |
| idle deadline | reactor timer | per connection |
| input buffer (`raw`/`next`) | reactor | reuse, bounded by `max_header_bytes`/`max_body_bytes` |
| output queue | reactor, per connection | bounded; drained on write readiness |
| assembled request | handed to worker | owned by worker job; not the socket |
| request/response | worker job | response returned to reactor; socket stays reactor-owned |
| cancellation | `shared_ptr<strut_cancellation_source>` (existing) | cancelled by reactor/shutdown |

Prefer RAII and existing shared_ptr/refcount mechanisms; avoid new raw-pointer
lifetime coupling.

---

## 5. Compatibility implications

- Public Strut API unchanged: `app.get/post/...`, `get_stream`, `get_async`,
  `serve_static`, `timeouts`, `limits`, `stop`, `listen`, `listen_tls`,
  WebSocket handlers, streaming helpers, TLS. This is an internal runtime
  change.
- `strut_http_response_writer` / `strut_http_request_body` observable behavior
  preserved (streaming, chunked, HEAD accounting, cancellation).
- The async boundary stays: `get_async` still `strut_await`s on the worker.
  (An eventual future could move async handlers off the worker pool, but that is
  out of scope here.)

---

## 6. Platform mapping

Narrow internal abstraction (not platform structs leaking into HTTP code):

```
reactor:
  register(fd, Interest)
  modify(fd, Interest)
  unregister(fd)
  wait(events_out, timeout)      // readiness
  wake()                          // cross-thread wakeup
  schedule(deadline, callback)    // connection timers
Interest: readable | writable | error | closed | wakeup
```

- **Linux**: `epoll` (`EPOLLIN/EPOLLOUT/EPOLLRDHUP/EPOLLERR/EPOLLHUP`),
  wakeup via `eventfd`.
- **macOS/BSD**: `kqueue` (`EVFILT_READ/EVFILT_WRITE`, `EV_EOF/EV_ERROR`),
  wakeup via `EVFILT_USER` or a `socketpair`.
- **Windows**: IOCP is *completion*-based and must not be forced into the
  readiness shape. First cut: a readiness reactor over `WSAPoll` /
  `WSAEventSelect` + `WSAEnumNetworkEvents`, bounded by the documented
  connection limit, using the existing `strut_socket_poll*` primitives. A
  scalable completion-based engine (IOCP) should be a **separate future
  backend**, evaluated on its own; it does not block R1–R11.

The connection state machine (§4.2) is platform-independent; only the I/O
engine under it varies. This keeps epoll/kqueue/WSAPoll differences out of HTTP
logic.

---

## 7. Risks

1. **Streaming concurrency cap** (biggest behavioral risk): worker pool size
   bounds concurrent long-lived streams. Mitigation path in §4.4; must be
   measured and documented, not hand-waved.
2. **Platform divergence**: three readiness backends; Windows is the weakest
   (WSAPoll limits). Certify per platform at R8/R9.
3. **Cancellation/shutdown races**: handoff + wakeup must be race-free; heavy
   sanitizer/stress testing required (R10).
4. **Timeout semantic drift**: verify read/write/idle/shutdown timing
   equivalence in tests.
5. **TLS handshake**: currently blocking with poll slices; keep it in the worker
   after first readiness (as today), or add a reactor TLS-handshake sub-state.
6. **WebSocket**: currently one dedicated thread per upgraded connection
   (`websocket_worker_`, 1059/1079). It would occupy a worker in the handoff
   model. Decide explicitly at R7 (separate pool vs reactor write path).
7. **Regression surface**: the generated runtime is large; every checkpoint must
   keep the full certification wall green.

---

## 8. Expected performance benefit (with uncertainty)

The redesign removes the concurrency inflation (80 → 106 µs) and most of the
scheduler/cache/allocator contention. A plausible outcome at c=50 is roughly the
c=1 per-request cost, i.e. throughput from ~9k toward ~11–13k rps on one vCPU
(~1.3–1.5×). It also removes per-connection `poll`/`setsockopt` churn, which
should *reduce* the ~6.5 syscalls/request somewhat.

This is **not** expected, by itself, to reach Go (~24k) or Rust (~35k+), whose
per-request userspace cost is materially lower. Reaching "competitive with /
beating Go" likely also needs per-request cost work (parser, allocations,
response serialization) beyond the concurrency model. The reactor is the
necessary first step (it removes the *inflating* term); further per-request work
is a follow-up. **No exact RPS is promised.**

Post-redesign, multi-vCPU scaling (R12) and a stronger generator (node B ≥2
vCPU) are required before publication-quality ratios; the current ~35k Rust
number is a lower bound (generator saturates ~37k).

---

## 9. Checkpoint campaign (reversible)

R0  Freeze revised architecture/invariants; preserve benchmark baseline; commit
    design only (no runtime code).
R1  Reactor abstraction + Linux `epoll` backend + wakeup + registration
    lifecycle; focused unit tests.
R2  Reactor-owned accept; connection objects; connection state machine; idle/read
    readiness; no public API change.
R3  Incremental request-head/readiness path; buffered request-body transport;
    dispatch complete requests to fixed CPU-sized worker pool.
R4  Worker completion path; reactor-owned nonblocking/partial response writes;
    bounded output; backpressure.
R5  Keep-alive reuse; pipelining; idle deadlines; timeout equivalence.
R6  Cancellation; disconnect; `stop()`; graceful drain; forced shutdown;
    ownership/race hardening.
R7  Streaming request bodies; streaming responses; NDJSON; `serve_file`; SSE
    behavior; WebSocket strategy; bounded streaming backpressure.
R8  TLS integration/hardening; slow-handshake behavior.
R9  `kqueue` backend + macOS certification.
R10 Windows readiness backend + Windows certification.
R11 Sanitizers; race/concurrency stress; slow-client/slow-reader/slow-writer
    tests; resource-bound tests.
R12 Linode single-vCPU rebenchmark.
R13 Profile remaining per-request cost; pursue general parser/allocation/runtime
    wins as justified.
R14 Stronger generator (node B >= 2 vCPU); final authoritative Strut/Go/Rust
    matrix.
R15 1/2/4-vCPU scaling; evaluate whether Option B/per-CPU reactors are
    warranted.

Checkpoint boundaries may be adjusted if implementation reality makes another
split cleaner.

Every checkpoint: focused tests, existing behavior preserved, `git diff --check`
clean, independently reviewable and revertible, commit at checkpoint end. Run
broader/full certification at sensible integration gates rather than rerunning
five-platform CI after every internal helper, but do not let cross-platform
divergence accumulate to the end.

**Early architecture-validation gate (after R4/R5, before spending R6–R15):**
run a Linux Linode smoke at c=1/c=10/c=50 for `/plaintext` and `/json` and
record threads, context switches, syscalls/request, CPU, RSS, p50/p95/p99, RPS.
Primary question: did per-request CPU stop inflating with concurrency (old 80 →
106 µs)? If it does **not** materially improve, STOP, profile, and reassess
before continuing.

Benchmark controls stay frozen (Go `net/http`, Rust `axum`+`tokio`); no history
rewrite; keep baseline, post-TCP_NODELAY, P4/P5 evidence, reactor baseline/final.

## 9.1 Implementation gate

Proceed through checkpoints without re-approval. STOP and report only if:
public API change appears necessary; semantics must change; memory
safety/ownership cannot be made clear; streaming/WebSocket compatibility appears
fundamentally incompatible; Windows/macOS requires a materially different
public/runtime architecture; benchmark performance worsens enough to invalidate
the premise; or scope expands beyond the contained HTTP/generated-runtime
redesign.

---

## 10. Size estimate

**LARGE runtime change** (not medium, not very large).

- It replaces the server concurrency core and adds a cross-platform I/O engine
  and a connection state machine, with streaming/cancellation/shutdown/TLS/
  WebSocket interactions.
- It is contained to the generated runtime (no compiler, type-system, or public
  API changes), and is decomposable into independently revertible checkpoints,
  which keeps it below "very large".

---

## 11. Decision

APPROVED WITH AMENDMENT (R0):

- Option A approved: reactor + bounded CPU-scaled application worker pool.
- Amendment: reactor owns ordinary transport waits; workers execute application
  work and do not block on ordinary socket I/O (buffered path). Streaming/
  long-lived surfaces use a bounded reactor-driven adapter (preferred) or a
  bounded transitional pool, resolved at R7.
- Option B (per-CPU reactors/sharding) reserved as the multi-vCPU evolution;
  design the connection state machine so sharding does not require rewriting it.
- Option C rejected as-is.
- Option D / full completion abstraction (IOCP) deferred; Windows uses a
  correctness-capable readiness backend first, classified as such.
- `strut_executor` not reused as the HTTP scheduler.
- Size: LARGE runtime change.

R0 is a design-only checkpoint. Implementation proceeds per §9 (R1 onward)
without further re-approval except at the §9.1 gate.
