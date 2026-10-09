# RESULTS

## Baseline identity
- Strut main commit: `a1036a1` (frozen at campaign start).
- Canonical workload: `/plaintext` "Hello, World!" + `/json` (frozen Strut HTTP benchmark).
- Local single-node screening (NUC-class, 20 cores, loopback, 50 conns, ~6s): ~55k requests/s
  (SCREENING ONLY; canonical retain/revert decisions require the two-node frozen run).

## C1 - TCP_NODELAY: RESOLVED (already fixed in R8.5)
Strut already disables Nagle on accepted sockets. `strut_socket_prepare` sets
`TCP_NODELAY` + `SO_NOSIGPIPE`, invoked from the `strut_tcp_socket(int)` constructor used by
BOTH the worker `accept()` and reactor `try_accept()` HTTP paths
(src/generated_runtime.cpp lines ~590-595, 698). R8.5 B8 found + fixed this (1 200 -> 9 023
rps, keep-alive stall 41ms -> 0.23ms per reused request; see
strut-benchmarks/http/results/raw/FINDING-tcp-nodelay.md). Campfire C does the same
(loop.c accept-site setsockopt). NO candidate needed. Campfire's second element - vectored
"Hyper-like" flush - is the C4/C5 question.

## C2 - request-path cost measurement (callgrind, single-node screening; ~5 854 requests)
PROGRAM TOTAL ~348.9M Ir => **~59.6k instructions / request**. Top cost centers
(percent of total Ir):
1. std::string::max_size 7.53%
2. std::string::size 6.87%
3. std::min<unsigned long> 6.18%
4. std::string::_M_data 3.75%
5. serve_connection 2.91%
6. std::string::_M_is_local 2.48%
7. std::string::_M_set_length 2.42%
8. std::string::operator[] (const) 2.05%
(+ assign/copy/_M_construct/_M_dispose/move/_M_append/dtor/_Alloc_hider ... all in the top 30)

plus: shared_ptr _M_release (~390/req), response_writer_state get/-> (refcount churn),
~300 mutex lock_guard / request, memcpy/~free (allocator), and the HTTP parser itself
(`strut_http_token_char`, `strut_http_target`, `strut_http_field_value` trim) which is only
~1-2% of total.

PRELIMINARY CONCLUSION: the parser is NOT the dominant cost; **std::string construction/
copy/append/grow across request+response + shared_ptr ownership + mutex** dominate. This points
at C4 (response/header representation - flattening/copying owned strings), C7 (per-keep-alive-
request reconstruction), and C6 (refcount multiplicities), not a parser rewrite (C3).

NEXT: canonical two-node baseline (Strut current vs Rust ref) to set the exact gap; then a
bounded C4/C7 candidate targeting per-request std::string/shared_ptr churn, measured in
isolation. Full syscall/malloc counts via the two-node harness profile.

## Canonical two-node baseline (frozen, on-node build, 96.126.107.155 server / .181 loadgen, 1 vCPU each, c=50, 15s)

Built ON NODE A (glibc 2.39 / gcc 13 / cmake Release) to avoid the local-glibc mismatch; exact
pinned baseline commit 793bc48. `wrk -t2 -c50 -d15s http://<A>:8080/plaintext`, 3 samples.

| impl | rps samples | median rps | p50 | server CPU% | RSS |
|---|---|---|---|---|---|
| Strut (baseline 793bc48) | 10203 / 10170 / 10160 | **~10 170** | ~4.96 ms | ~45% | ~5.8 MB |
| Rust (axum 0.7+tokio, LTO, release) | 21418 / 22627 / 23150 | **~22 630** | - | ~28% | ~4.7 MB |

Gap: Rust is ~2.22x Strut (absolute ~12.5k rps; Strut is ~55% behind). BOTH servers are
below CPU saturation on the 1-vCPU node (Strut ~45%, Rust ~28%), so neither is raw-CPU-bound at
c=50; Strut spends roughly 1.6x the CPU share to do ~half the rps => its per-request cost is
materially higher, consistent with callgrind (~59.6k Ir/req, string/refcount/lock dominated).

Toolchain (node A): Ubuntu 24.04, glibc 2.39, gcc 13.3, cmake build of strut@793bc48; Rust via
apt rustc/cargo, axum 0.7.9, tokio multi-thread (available_parallelism=1).

## Per-request COMPUTE (local callgrind, matched c=12, startup-negligible)
| mode | Ir / request | note |
|---|---|---|
| worker | ~124 900 | matched 8s run (90 53 req); earlier ~59.6k was under-sampled |
| reactor | ~111 700 | matched 8s run (9 957 req) |

Both modes burn a large, comparable compute budget dominated by std::string machinery
(local_data/_M_construct/_M_dispose/dtor/_Alloc_hider/_S_copy/length/capacity/substr/append),
plus shared_ptr<reactor_connection>::get, strut_parse_http_request_head, strut_http_token_char,
pthread_mutex lock/unlock, memcpy, malloc/free. Reactor's real throughput edge comes from lower
sync/scheduling (no worker handoff cv), not lower compute. => The remaining Rust gap (~1.34x on
reactor) is a COMPUTE-copy/materialization problem, not syscall volume (reactor: 3 sock ops + 1
eventfd + 0.55 epoll/req) and not writer-state locks (0/req). Rust per-request Ir is the next
comparative datum to obtain (locally, same methodology).

CORRECTED conclusion: the largest measured removable category is per-request STRING
MATERIALIZATION in request-head construction (method/path/header-name/value strings) and response
serialization, present identically in worker and reactor. Next: reactor ownership ledger counters
(cancellation/completion/reactor_connection) to separate the sync/ownership tax from the string
tax, then ONE candidate on the measured dominant cost.

## PARITY ASSESSMENT (after baseline): B - PARITY IS POSSIBLE BUT UNPROVEN

Evidence: (1) 2.2x gap, (2) Strut's per-request instruction count ~59.6k Ir dominated by
std::string machinery + ~390 shared_ptr releases + ~300 mutex lock_guards + memcpy/free, with the
parser only 1-2%, and (3) Strut uses more CPU% per request than Rust. These are exactly the
classes of cost where safe removal may still exist (string materialization, borrowed-owner
raw pointers instead of per-layer shared_ptr, keep-alive state reuse, mutex classification).
No fundamental parser/kernel/architecture blocker was measured. If the ~390 releases and ~300
locks are largely borrowable/removable, parity becomes realistic; if they are semantically
required by the safe runtime model, they are the architectural tax.

Budget categories to quantify next (C4/C7/C6):
- string/materialization churn: measure copies + constructs/req at each stage (request, headers,
  response ser) -> remove redundant materializations only.
- shared_ptr/refcount: classify the ~390 releases/req by type (connection, run_state,
  response_writer_state, request state, cancellation, reactor_connection, completion) and
  identify which are borrowed-only within a bounded synchronous section -> HTTP-C-borrowed-owner
  candidate.
- mutex: classify the ~300 lock_guards/req (owning-safe only).
- keep-alive reconstruction: what is destroyed/rebuilt per request on a persistent connection.
- response representation: how many times the head/body are copied/allocated (C4 iovec lesson):
  only after the ownership/string ledger.
- build/LTO/allocator: after structural wins (C8).

## Paired worker vs reactor vs Rust (same session, frozen nodes, `wrk -t2 -c50 -d15s`, 3 alternating samples)
| mode | rps samples | median | p50 | CPU% | RSS MB |
|---|---|---|---|---|---|
| worker (default; the R8.5 canonical mode) | 9677 / 10048 / 10064 | **~10 050** | ~5.0 ms | ~55 | ~5.7 |
| **reactor (STRUT_HTTP_REACTOR=1)** | 14900 / 15516 / 16252 | **~15 520** | ~3.1 ms | ~50 | ~4.6 |
| Rust (axum+tokio) | 20757 / 20268 / 21802 | **~20 770** | - | ~31 | ~4.4 |

Reactor is ~1.54x the worker path and ~1.34x behind Rust; worker is ~2.07x behind Rust.
The historical R8.5 canonical used the WORKER path (harness launches with no STRUT_HTTP_REACTOR);
its frozen Strut was ~17-18k and Rust >=35k (generator-limited) on these SAME Linodes. The current
session's absolute numbers are ~1.7x lower (Linode 1-vCPU shared-CPU quota drift); within-session
RELATIVE ratios are the trustworthy signal. Rust reference matches R8.5's axum+tokio source/LTO.

RECONCILIATION: current-vs-historical absolute differences are session (CPU-quota) drift, not a
code regression: both Strut and Rust dropped ~1.7x together in this session. Frozen canonical
baseline for the campaign = BOTH worker (matches R8.5 methodology) and reactor (best safe retained
opt-in configuration). Optimization targets reactor first (intended fast direction, furthest ahead
safely); worker stays a measured control.

## PARITY ASSESSMENT v2 (after paired baseline): B - PARITY IS POSSIBLE BUT UNPROVEN
With the best retained (reactor) configuration the gap to Rust is ~1.34x (15.5k vs 20.8k), not
~2x. The per-request profile (59.6k Ir/req; writer-state accessors = shared_ptr copy + uncontended
lock per call; parser 1-2%) is unchanged and can plausibly cover ~1.34x. Reactor also lowers RSS
(4.6 vs 5.7 MB) and p50. Parity more credible than the worker-only read suggested, still unproven
until an end-to-end candidate A/B lands.

## Pending
C3, C4, C5, C6, C7, C8, C9, C10 - each one hypothesis, isolated, benchmarked, retained/reverted.
