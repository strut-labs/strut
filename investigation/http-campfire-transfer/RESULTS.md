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

## Pending
C3, C4, C5, C6, C7, C8, C9, C10 - each one hypothesis, isolated, benchmarked, retained/reverted.
