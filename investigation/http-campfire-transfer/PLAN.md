# PLAN (HTTP-C*)

Reference campfire-c/g: keep-alive `/plaintext` + `/json`. Gap to Rust: compute-efficient path,
not transport. Two-node methodology identical to the completed Strut HTTP campaign.

| C# | Campfire mechanic | Strut analogue | Prior R8.5 overlap | Status |
|---|---|---|---|---|
| C1 | TCP_NODELAY on accepted sockets | accepted sockets (worker+reactor) | FIXED in R8.5/B8 (1200->9023) | RESOLVED: already applied |
| C2 | span parser; measure parser cost | strut_http parser (std::string fields) | partial (header flat) | MEASURED: string churn dominates ~60k Ir/req |
| C3 | span-first parser candidate | parser materialization | - | pending (only if C2 justifies) |
| C4 | vectored/iovec response segments | response serialization representation | head construction retained | pending: response representation audit |
| C5 | bounded output candidate (representation/copying, NOT EPOLLOUT policy) | - | EPOLLOUT eliminated | pending |
| C6 | per-request operation accounting | allocator/syscall/futex counts | cancellation alloc work | pending: syscall + allocator counts |
| C7 | fixed/bounded keep-alive state | connection/request/response reconstruction | - | pending |
| C8 | LTO/-march/allocator build | generated-server flags | - | pending |
| C9 | generated C++ inspection | compiled server source | - | pending (callgrind gave first data) |
| C10 | matched profile shape | Strut vs Rust vs C | - | pending |

Ordering starts C1 (done/resolved) -> C2 (measured) -> C4/C6/C7, then C8/C9/C10. Profiling may
reorder. Stop when Strut reaches practical Rust parity, or all strong hypotheses measured with
an attributable remaining cost, or remaining candidates are weak/noisy.
