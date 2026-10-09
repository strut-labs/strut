# HTTP throughput transfer from Campfire C

Objective: learn from `basecamp/once-campfire-c` (and Rust/verification) which hot-path
mechanics can close Strut's remaining HTTP/1.1 request-throughput gap, testing each transferable
hypothesis independently. Read-only references; BOUNDED campaign; then return to FFI-7.

## References (read-only, cloned under /tmp/opencode)
- basecamp/once-campfire-c (loop.c/output.c/request.c/http_internal.h, CHANGELOG.md, bench/)
- basecamp/once-campfire-rust
- basecamp/once-campfire-verification/docs/performance-review.md

Key published numbers (AMD Ryzen AI MAX+ 395, 4 cores/app, 16 clients, gzip):
C reads ~137-152k rps, Rust ~103-121k rps; posts ~7.5k vs ~8.0k. C's read lead is driven by
complete-body caching + app architecture, NOT raw language speed; we chase only transferable
hot-path mechanics.

## Discipline
- One hypothesis per candidate, isolated; canonical two-node frozen workload for retain/revert;
  local single-node only for screening/profiling/debugging.
- Preserve R8.5 line: serializer, buffered request move, range-guided head construction,
  indexed router, flat headers, cancellation allocation reduction are RETAINED. Immediate
  reactor writes (-0.46%), direct writable arm (-10.5), direct reactor callback (+6.7%
  diagnostic, unsafe) are ESTABLISHED ABSENT / REVERTED. Do not re-run those without a new
  mechanism.
- Baseline identity frozen at commit (see RESULTS.md).
- No destructive git ops; Campfire repos are never modified/pushed.
