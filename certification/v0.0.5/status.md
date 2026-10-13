# Strut v0.0.5 release-certification status

Persistent, tracked evidence log for the v0.0.5 release-certification campaign.
This document distinguishes **historical** results (obtained against an earlier
HEAD) from **candidate-SHA** results. Only results executed against the CANDIDATE
SHA listed below may satisfy release gates.

## Candidate

| Field | Value |
|---|---|
| Candidate SHA | production tree HEAD (see "Candidate lineage" below) |
| Explicit compiler | `/home/nick/relgcc/strut` (release candidate build, authoritative) |
| Branch | `main` |
| Remote | in sync with `origin/main` |
| Session start (UTC) | 2026-10-13 |

Candidate lineage: `0a27604` (previous verified HEAD) -> ... -> `8374423`
(hardened ws_drain_probes drivers + gitignore) -> `a1736fb` (this evidence log)
-> `38a1616` (ASan recording). The sanitizer campaign then discovered and fixed a
**production reactor runtime defect** (see "Defect found + fixed" section): the
candidate production SHA moves to the new fix commit once committed.

Certification infrastructure commits (8374423 harness hardening, a1736fb/b494b43
evidence log, 38a1616 recording) do not alter production runtime/compiler/ABI
behavior (except `src/generated_runtime.cpp`, which is a production fix described
in its own section). Functional gates are evidenced against the production tree,
and the final candidate SHA is the last production-touching commit once gates
complete. Each result row below quotes its own executed SHA.

## Recording format

Every result row records:

```
Candidate SHA   - short SHA of the tree under test
Test name       - gate/suite identifier
Compiler        - exact compiler path used to build the STRUT compiler used
Native CXX      - native compiler + flags consumed by the generated-program step
Config          - STRUT_HTTP_REACTOR / sanitizer env / options
Command         - exact command executed
Exit code       - 0/1/77/timeout
Result          - PASS / FAIL / BLOCKED / SKIPPED
Evidence        - path to detailed log (kept out of /tmp when persistent)
```

## Gate index

- [ ] ABI / compatibility
- [ ] Local walls (GCC -Werror, Clang -Werror, ASan/UBSan, regressions)
- [ ] FFI (FFI-9 historical, FFI-10 Linux-local final-candidate)
- [ ] Sanitized networking (ASan/UBSan generated servers)
- [ ] TSan
- [ ] PTY thread accounting
- [ ] Compiler validation (GCC + Clang warnings-as-errors, full CTest, regressions)
- [ ] Five-platform CI matrix (one candidate SHA)
- [ ] Release review (RELEASE_CHECKLIST.md reconciliation, READY/NOT READY)

## Results

### Checkpoint archive (harness hardening)

| Result | Test | SHA | Evidence |
|---|---|---|---|
| PASS | run_reactor_postbody.py (legacy + reactor) | 8374423 | this session, empty stderr, exit 0 |
| PASS | run_reactor_explicit_stop.py (legacy + reactor) | 8374423 | this session, empty stderr, exit 0 |

### Sanitized networking (ASan/UBSan, candidate SHA 8374423 production tree;
`STRUT_CXXFLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -O1`,
`STRUT_LDFLAGS=-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`)

Generated-server instrumentation independently confirmed: `nm` reports 32 `__asan`
symbols and `ldd` resolves `libasan.so.8` in an -O1-instrumented fixture built by the
candidate compiler with the same env flags (so the sanitizers reach the generated
C++ server, not only the compiler).

| Result | Test | Mode | Compiler | Exit | Evidence |
|---|---|---|---|---|---|
| PASS | run_reactor_postbody.py | legacy | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_postbody_20261013_163249.log |
| PASS | run_reactor_postbody.py | reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_postbody_20261013_163249.log |
| PASS | run_reactor_explicit_stop.py | legacy | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_explicit_stop_20261013_163344.log |
| PASS | run_reactor_explicit_stop.py | reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_explicit_stop_20261013_163344.log |

### Defect found + fixed (sanitizer campaign)

**Reactor WebSocket connection leak (deterministic, reference cycle).**
`conn->websocket_job` is a `std::function` stored ON the connection whose lambda
captures `ws_conn` (a `shared_ptr` to the connection itself) plus `run`
(`shared_ptr<run_state>`). The stream worker invoked the job but never cleared it
(src/generated_runtime.cpp:1825), so after `reactor_close` erased the connection
from `run->reactor_data->connections` the only surviving reference was the
connection's own self-capture -> the connection object AND the `run_state` were
never freed. Every reactor WebSocket connection leaked. Legacy mode was clean
(no stored lambda). Evidence:
logs/asan_ubsan_p5_reactor_detleaks_20261013_164525.log (RSS-return failure) and
logs/asan_ubsan_p5_reactor_noquar_20261013_164656.log (LSan: 1,182,464 bytes in
1,771 allocations, all indirect, 143 reactor_connection roots).

**Reactor TLS `SSL*` leak.** `reactor_close` never freed `conn->tls`, leaking the
OpenSSL session for every reactor TLS connection. Legacy path already wrapped
`SSL*` in `unique_ptr<SSL,SSL_free>`.

Fix (smallest demonstrated cause, src/generated_runtime.cpp):
- stream worker now clears `conn->websocket_job=nullptr` after invoking the job
  (breaks the self-capture; the worker is the job's sole invoker, assignment happens
  before the mutex-protected deque pop, so no race).
- `reactor_close` now calls `SSL_free(conn->tls); conn->tls=nullptr;`.

Verification after fix (same env flags as above but `detect_leaks=1` +
`quarantine_size_mb=0` to make LSan authoritative and isolate quarantine):
| Result | Test | Mode | Compiler | Exit | Evidence |
|---|---|---|---|---|---|
| PASS | websocket_runtime_certification P5 | legacy | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_p5_legacy_v2_*.log (100 cycles, TLS 20, sanitizer-empty) |
| PASS | websocket_runtime_certification P5 | reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_p5_reactor_v2_*.log (100 cycles, TLS 20, sanitizer-empty) |
| PASS | http_websocket_certification P4 | legacy | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_p4_legacy_v2_*.log |
| PASS | http_websocket_certification P4 | reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_p4_reactor_v2_*.log |
| PASS | run_reactor_postbody.py | legacy+reactor | /home/nick/relgcc/strut | 0 | 7 consecutive runs PASS 2026-10-13 |
| PASS | run_reactor_explicit_stop.py | legacy+reactor | /home/nick/relgcc/strut | 0 | 3 consecutive runs PASS 2026-10-13 |
| PASS | sanitized_http_smoke | legacy+reactor | /home/nick/relgcc/strut | 0 | mode=legacy/reactor 200/hello-asan exit 0 0 bytes |
| PASS | run_reactor_probe.py | legacy+reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_reactor_probe_v2_*.log |
| PASS | run_ws_close_probe.py | legacy+reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_ws_close_probe_v2_*.log |
| PASS | run_reactor_param_probe.py | legacy+reactor | /home/nick/relgcc/strut | 0 | logs/asan_ubsan_param_probe_v2_*.log |
| PASS | Default CTest (GCC -Werror) | - | relgcc build | 0 | 44/44 |
| PASS | Reactor CTest | - | relgcc build, STRUT_HTTP_REACTOR=1 | 0 | 44/44 |

NOTE on the harness `not stderr` assertion: empty stderr is required by the hardened
drivers. Under normal and sanitized execution the generated server emits nothing on
stderr, so a genuine sanitizer finding (which writes to stderr and usually aborts
with a non-zero exit) fails the test. No benign stderr noise was observed in any
passing run.

### Historical results (NOT candidate evidence)

The following were recorded in prior sessions/commits and must not be
automatically attributed to the candidate SHA:

| Result | Gate | Original SHA | Note |
|---|---|---|---|
| 44/44 | Default CTest | earlier | re-run required on candidate |
| 44/44 | Reactor CTest | earlier | re-run required on candidate |
| PASS | Legacy P4 | earlier | re-run required on candidate |
| PASS | Reactor P4 | earlier | re-run required on candidate |
| PASS | Legacy P5 | earlier | re-run required on candidate |
| PASS | Reactor P5 | earlier | re-run required on candidate |
| 25/25 | FFI-9 five-platform consumers | 38069818109 evidence | historical, platform CI |
| 4/4 | FFI-10 Linux-local interop | earlier | re-run required on candidate |
| PASS | ASan/UBSan legacy HTTP smoke | 0a27604 | codegen harness preserved at 8374423 |
| PASS | ASan/UBSan reactor HTTP smoke | 0a27604 | codegen harness preserved at 8374423 |

### TSan (thread sanitizer, generated servers via STRUT_CXXFLAGS/LDFLAGS)

**Defect found + fixed in reactor shutdown synchronization.** ThreadSanitizer on
reactor-mode WebSocket runtime reported a genuine data race on `run->workers_stopping`:
the event-loop thread wrote it without holding `run->mutex` (drain-stop site,
post-loop teardown, and worker-spawn catch path), while `reactor_worker_loop` and
`reactor_stream_worker_loop` read it inside their `condition_variable::wait`
predicates under `run->mutex`. Reactor mode had zero TSan coverage in CI (CI never
sets STRUT_HTTP_REACTOR), so this was previously unobserved. Fix: all three reactor
write sites now take `run->mutex` (src/generated_runtime.cpp).

Evidence: logs/tsan_websocket_runtime_reactor_*.log (pre-fix WARNING), logs/tsan_websocket_runtime_reactor_fixed2_*.log (PASS), logs/tsan_*.

TSan results (STRUT=/home/nick/relgcc/strut, -fsanitize=thread, TSAN_OPTIONS=halt_on_error=1):
| Result | Test | Mode | Exit | Evidence |
|---|---|---|---|---|
| PASS | http_worker_certification | legacy | 0 | logs/tsan_http_worker_v2_*.log |
| PASS | http_client_streaming_certification | legacy | 0 | logs/tsan_http_client_streaming_v2_*.log |
| PASS | websocket_runtime_certification P5 | legacy | 0 | logs/tsan_websocket_runtime_v2_*.log |
| PASS | websocket_runtime_certification P5 | reactor | 0 (x2) | logs/tsan_websocket_runtime_reactor_fixed2_*.log |
| PASS | http_websocket_certification P4 | reactor | 0 | logs/tsan_p4_reactor_v2_*.log |
| PASS | process_lifecycle_certification | legacy | 0 | logs/tsan_process_lifecycle_v2_*.log |
| PASS | pty_certification | legacy | 0 | logs/tsan_pty_cert_v2_*.log |
| PASS | pty_lifecycle_certification | legacy | 0 | logs/tsan_pty_lifecycle_v2_*.log |
| PASS | run_ws_close_probe | legacy+reactor | 0 | probe run |
| PASS | run_reactor_param_probe | legacy+reactor | 0 | probe run |
| PASS | run_reactor_postbody.py | legacy+reactor | 0 | driver run |
| PASS | run_reactor_explicit_stop.py | legacy+reactor | 0 | driver run |

Pre-existing reactor limitations observed (NOT regressions; fail identically
without sanitizers): reactor-mode http_worker_certification saturation reject and
websocket_pty_session_certification (see PTY gate). websocket_pty_session_certification
legacy under TSan timed out on a client recv during a heavy-host-load window (load ~11
from unrelated npm/python/ChatGPT jobs) with no TSan data-race report; re-run under the
PTY gate with normalized load.

### Open / in progress

| Result | Gate | SHA | Evidence / notes |
|---|---|---|---|
| PASS | ASan/UBSan partial-body + explicit-stop (legacy+reactor) | candidate | 4/4 + repeated runs (2026-10-13) |
| PASS | ASan/UBSan P4/P5 (legacy+reactor) | candidate | incl. TLS/WSS, detect_leaks=1 (2026-10-13) |
| PASS | ASan/UBSan smoke + probes (legacy+reactor) | candidate | sanitized_http_smoke, ws_close/param/probe |
| PASS | ASan/UBSan TLS/WSS lifecycle | candidate | covered by P4/P5 runs (plaintext+TLS) |
| PASS | TSan (legacy CI scope + reactor focus) | candidate | race fixed; see TSan section |
| PENDING | Full TLS/WSS standalone lifecycle gate | candidate | explicit dedicated row if required |
| PENDING | PTY thread-count discrepancy + ws-pty reactor timeout | candidate | |
| PASS | GCC -Werror full CTest (default + reactor) | candidate | 44/44 + 44/44 |
| PENDING | Clang -Werror full CTest | candidate | |
| PENDING | FFI final-candidate ABI/embedding | candidate | |
| PENDING | Five-platform CI matrix | candidate | |

## Residual limitations (preserved)

- Deterministic WebSocket scheduling probes exercise frozen generated-C++ snapshots.
  They demonstrate the original admission race and its correction but do not provide
  deterministic scheduling coverage of subsequent live-runtime changes. Current-runtime
  integration suites provide separate executable coverage. Not restarted for v0.0.5.
- Reactor HTTP/WebSocket has zero CI coverage (CI never sets `STRUT_HTTP_REACTOR`);
  local reactor coverage is the evidence source.