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

### PTY thread accounting + P10 WebSocket-PTY

Historical CI finding (run 38069818109, linux-x64-gcc, "Certify sanitized generated HTTP
runtime" step, SHA 37dd55a): `P10 Linux threads did not return: {rss 321792, threads 11} ->
{... threads 12}` in the abrupt-session phase of websocket_pty_session_certification under
ASan. The current candidate does NOT reproduce it across repeated runs:

| Result | Test | Mode/Flags | Threads | Exit | Evidence |
|---|---|---|---|---|---|
| PASS | pty_certification | plain | n/a | 0 | ptycert_asan_candidate_*.log (ASan) |
| PASS | pty_lifecycle_certification | plain (x3), ASan, TSan | 1->1 after (allowance 0) | 0 | ptylc_plain_1_candidate_*.log, tsan_pty_lifecycle_v2_*.log |
| PASS | websocket_pty_session (P10) | plain, 1000 seq/32 conc | 34->34 | 0 | p10_plain_candidate_*.log |
| PASS | websocket_pty_session (P10) | ASan, 50 seq/8 conc (x8) | 5->5, 10->10 | 0 | p10_asan_*.log |
| PASS | websocket_pty_session (P10) | TSan, 50 seq/8 conc | 6->6, 11->11 | 0 | p10_tsan_candidate_*.log |

Assessment: the historical 11->12 was a single transient thread during an abrupt-
disconnect phase on a busy shared runner at an older SHA; fd=4, children/zombies=0
returned exactly in the same failing sample (RSS within ASan allowance), so it is
best explained as a timing-sensitive test-accounting transient rather than a permanent
thread leak. Current candidate returns threads to baseline exactly across plain/ASan/TSan.
The PTY worker-thread join model (worker_loop joins on drain, close_timer joins in
destructor, P10 output thread joins in handler) shows no permanent retention.

Pre-existing reactor limitation (documented, NOT a regression): websocket_pty_session
cert times out in reactor mode without sanitizers (server never delivers a frame in a
session phase), matching the documented reactor WebSocket close/final-EOF divergence.
Reactor mode has zero CI coverage; P10 is only required in legacy mode where it passes.

### Compiler validation (final candidate after reactor fixes)

| Result | Gate | Compiler | Exit | Evidence |
|---|---|---|---|---|
| PASS | GCC -Werror full CTest | /home/nick/relgcc/strut | 0 | 44/44 |
| PASS | GCC -Werror full CTest (reactor) | /home/nick/relgcc/strut | 0 | 44/44 |
| PASS | Clang -Werror full CTest | /tmp/opencode/clangbuild/strut (clang++ 21) | 0 | 40/40 (Nift-FFI 4 tests absent: libnift_c not in Clang config) |
| PASS | Clang -Werror full CTest (reactor) | /tmp/opencode/clangbuild/strut | 0 | 40/40 |
| PASS | Independent regression suite default | /home/nick/relgcc/strut | 0 | 307/307 |
| PASS | Independent regression suite reactor | /home/nick/relgcc/strut | 0 | 307/307 |
| PASS | Independent regression suite (Clang) | /tmp/opencode/clangbuild/strut | 0 | 307/307 |

### FFI / embedding (final candidate after reactor fixes)

- `include/strut/embed.h` compiles standalone as C11 and C++20 (source tree).
- `strut_embed_value` layout measured: sizeof=72, alignof=8, kind@0 i@8 s@32 retained@48
  cb_fn@56 cb_ctx@64 — matches FFI_ABI_AUDIT.md / FFI-9 C# mirror.
- Exported symbols present and unmangled in libstrut_embed.so (9 strut_embed_* exports).
- FFI/embed CTest suite 28/28 PASS (final GCC candidate), including ffi_abi_identity,
  aggregate, pointer, callback, error, retained, node/csharp/python/go consumers, embed_host.
- FFI-10 Nift interop 4/4 PASS (strut_ffi_nift_strut, from_strut, to_strut, bytes_rt).
- FFI-9 five-platform 25/25 remains historical evidence (run 38069818109, SHA 37dd55a),
  to be re-confirmed by the final five-platform CI matrix on the candidate SHA.

### Open / in progress

| Result | Gate | SHA | Evidence / notes |
|---|---|---|---|
| PASS | ASan/UBSan partial-body + explicit-stop (legacy+reactor) | candidate | 4/4 + repeated runs (2026-10-13) |
| PASS | ASan/UBSan P4/P5 (legacy+reactor) | candidate | incl. TLS/WSS, detect_leaks=1 (2026-10-13) |
| PASS | ASan/UBSan smoke + probes (legacy+reactor) | candidate | sanitized_http_smoke, ws_close/param/probe |
| PASS | ASan/UBSan TLS/WSS lifecycle | candidate | covered by P4/P5 runs (plaintext+TLS) |
| PASS | TSan (legacy CI scope + reactor focus) | candidate | race fixed; see TSan section |
| PASS | PTY thread accounting / P10 (plain+ASan+TSan) | candidate | see PTY section |
| PASS | GCC -Werror CTest (default+reactor) | candidate | 44/44 + 44/44 (final) |
| PASS | Clang -Werror CTest (default+reactor) | candidate | 40/40 + 40/40 (final) |
| PASS | Regression suite (default+reactor+Clang) | candidate | 307/307 x3 |
| PASS | FFI-10 Nift Linux-local (final candidate) | candidate | 4/4 + FFI/embed 28/28, ABI 72/8 verified |
| PENDING | Full TLS/WSS standalone lifecycle gate | candidate | explicit dedicated row if required |
| PENDING | FFI-9 five-platform matrix on candidate SHA | candidate | re-confirm via final CI matrix |
| PENDING | Five-platform CI matrix | candidate | |

## Residual limitations (preserved)

- Deterministic WebSocket scheduling probes exercise frozen generated-C++ snapshots.
  They demonstrate the original admission race and its correction but do not provide
  deterministic scheduling coverage of subsequent live-runtime changes. Current-runtime
  integration suites provide separate executable coverage. Not restarted for v0.0.5.
- Reactor HTTP/WebSocket has zero CI coverage (CI never sets `STRUT_HTTP_REACTOR`);
  local reactor coverage is the evidence source.