# C6/C7 ownership & synchronization ledger (per trivial /plaintext request)

Source of truth: generated server C++ (`strut --emit-cpp`), worker path (default; reactor code
present but not executed). Callgrind attribution: ~59.6k Ir/req; ~390 shared_ptr `_M_release`;
~300 `std::lock_guard<std::mutex>`; parser 1-2%.

## Dominant classified cost: response/request-body writer-state accessors
`strut_http_response_writer_state` and `strut_http_request_body_state` are `std::shared_ptr`
globals created once per response/request. EVERY accessor does:
`auto s = require(); std::lock_guard<std::mutex> lock(s->mutex); ...` i.e. ONE shared_ptr copy
(increment) + ONE uncontended mutex lock per call.

Accessors hit per plaintext request (each = 1 refcount inc + 1 lock):
- `require()`/`committed`/`reusable`/`account_head_body`/`prepare_stream_content_type`/`mutate`
  xN /`begin_response`/`abort`/`invalidate` - the response-writer state lock spin.
- request-body `eof`/`failed`/`begin_response` lock spins.
- response writer `write`/`flush` path

These are all called by ONE worker thread within `serve_connection()`'s synchronous section; the
mutex is uncontended and the shared_ptr ownership is already held by the enclosing worker-owned
objects. This is the mechanistic explanation for the ~300 locks and a large fraction of the
~390 releases.

## Other per-request owners (worker path)
- `std::shared_ptr<connection>`: created per accepted connection; copied into
  `run->connections`/`queue` and to each worker pop (a few transitions per request, legitimate
  cross-thread handoff -> keep shared).
- `run_state`: one per server run (worker_loop captures once) - negligible per request.
- `strut_socket_state` (via `std::shared_ptr<strut_tcp_socket>`): `acquire()`/`release()` per
  I/O op (read + write) = 2 refcount inc/dec + 2 socket-state mutex ops per request; needed for
  cross-thread socket ownership on cancellation/close. Candidate to review: per-op acquire on a
  single-worker sync read/write.
- `strut_cancellation_source`: created once and registered per request; cancel registration
  + request_cancellation lookup lock per request even when cancellation never fires.
- `strut_http_response_writer_state` -> passed to handler and to completion; also copied at
  `http_text` return / commit.

## Ledger table (types)

| type | per-req owner copies (est.) | shared_ptr inc+dec | accepts mutex | crosses thread/async? | borrowed-raw candidate? |
|---|---|---|---|---|---|
| response_writer_state | held by response + re-require per accessor | many (each accessor) | yes (uncontended) | only at handoff | YES in sync section |
| request_body_state | eof/begin_response per request | few | yes | at handoff | YES in sync section |
| connection | 1 create + ~2-4 queue/worker transitions | ~4-8 | worker queue | yes (worker+queue, legit) | keep shared |
| socket_state | 1 per socket; acquire/release per read+write | ~4+ | yes | cancellation/close | partial (per-op) |
| cancellation_source | 1 per request; register/unregister locks | ~2-4 | yes | cancel from other thread (escape) | keep shared |

## HTTP-C-borrowed-owner candidate (highest value)
Within `serve_connection()`'s synchronous, single-worker section, the response-body/request-body
writer STATE is owned by objects the worker holds for the whole request. Introduce a borrowed
`state*` (from the existing owner) into the commit/write/accessor path for the synchronous
portion, dropping the per-accessor shared_ptr copy + uncontended lock -- WITHOUT changing
ownership at the async boundaries (reactor/queue handoff, cancellation, close). Ownership at
every point where another thread can observe the object stays `shared_ptr`.

Estimated effect: remove most of the ~300 locks/req and a large share of the ~390 releases/req.

## EXACT COUNTS (compile-time-gated instrumentation, node A, frozen build; loadgen 50 conns ~14s from B)

Per-request (count / requests served; REQUEST count from the loadgen):

| counter | reactor | worker |
|---|---|---|
| response_writer require + lock | **0** | 6.0 |
| request_body require + lock | **0** | 2.0 |
| socket_state acquire / release | 3.0 / 3.0 | 6.0 / 6.0 |
| epoll_wait | 0.55 | 0 (worker path) |
| eventfd write (reactor wake) | **1.0** | 0 |

CORRECTION (measure, don't infer): the response/request-body writer-state accessors are NOT the
bulk of reactor cost -- they are ~8/req on the WORKER path and ZERO on the reactor path (the
reactor uses a separate response/completion path). The earlier ~300 locks / ~390 shared_ptr
releases/req came from the WORKER-path callgrind profile and do NOT transfer to the reactor.
The reactor's own per-request syscall pattern is already lean (1 eventfd wake + 3 socket ops +
~0.55 epoll_wait), so its ~locks/refcounts must live in cancellation, completion, and
reactor_connection ownership/alloc -- NOT yet instrumented. Candidate A is therefore RE-TARGETED:
measure reactor cancellation/completion/reactor_connection shared_ptr+alloc counts before any
raw-pointer change. eventfd_write = 1.0/req is also a candidate to attack (one wake syscall per
request) once ownership/alloc are attributed.

## Keep-alive reconstruction (C7)
Response-writer + request-body states are created/destroyed per response even though the
connection and its buffers persist. Serialized head buffer and header container capacity are
rebuilt each response for the trivial endpoint. Candidate (later): clear-and-reuse capacity
across keep-alive requests where semantically clean.

RERUN note: reactor-mode callgrind on the 1-vCPU Linode was startup-dominated (ld.so strcmp /
dl machinery) because valgrind slowness yielded very few requests -- NOT request-representative.
The compile-time-gated instrumentation counters are therefore the authoritative per-request
attribution (table above), and the instrumentation has been REMOVED from the committed runtime
(production builds emit nothing; strut_codegen_tests snapshot stays intact at 30/30/regressions).

RE-TARGETED CANDIDATE A (reactor primary): the writer/request-body accessor hypothesis does NOT
transfer to the reactor path (0 require/req). Reactor per-request costs to attribute next via the
same gated-counters method: cancellation source/token (alloc/register/lock per request),
completion state, reactor_connection / run_state ownership, string materialization in the reactor
parse+response path, and the eventfd_write = 1.0/req (one wake syscall per request) as a
separate candidate. Only then implement HTTP-C-borrowed-owner-1 (or an eventfd-reduction
candidate) on the measured dominant cost.

## Rule
Only borrow where an enclosing owner provably spans the synchronous use; never at async
boundaries; ASan/UBSan + lifecycle/shutdown/disconnect/cancel dogfood stay green.
