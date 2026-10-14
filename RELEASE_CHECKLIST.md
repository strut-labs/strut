# Strut v0.0.5 release-certification checklist

Final gate list. All items must be evidenced from the FINAL implementation SHA; no item may be
inferred from an earlier state or a different platform's results.

Final implementation SHA: 2c9c7e8. Final workflow run: 38529518056 (five-platform PASS on the
first attempt — no reruns needed).

## ABI / compatibility
- [x] `include/strut/embed.h` compiles standalone as C11 and C++20 (source-tree; staged-prefix).
- [x] `strut_embed_value` layout measured and verified: size 72 / align 8 / kind@0 i@8 s@32
      retained@48 cb@56 on x86-64 Linux (matches the five-platform C# mirror).
- [x] Exported symbols present and unmangled in the installed library (Linux: 9 strut_embed_*
      exports; macOS/Windows via consumer evidence + symbol lists where practical).
- [x] No retrospective v0.0.4 binary-compat guarantee implied; future ABI versioning policy
      documented.
- [x] `FFI_ABI_AUDIT.md` / `FFI_HANDOVER.md` consistent and accurate.

## Local walls
- [x] GCC -Werror: full CTest PASS 43/43 (default and reactor) on the final candidate
      (earlier 44/44 counts in this log were provisional; the test set changed during the
      campaign). Final CI totals: linux-x64-gcc / linux-x64-clang / linux-arm64-gcc /
      windows-x64-msvc 43/43; macos-arm64-appleclang 42/42 (Go FFI consumer not provisioned).
- [x] Clang -Werror: full CTest PASS default and reactor on the final candidate (a few
      Nift-FFI tests are not configured in the local Clang build: libnift_c absent; CI Clang
      runs 43/43 and GCC covers the Nift-FFI tests).
- [x] ASan/UBSan: applicable native CTest PASS (interpreter-hosted consumers excluded, documented);
      sanitized generated-server matrix PASS (see Sanitized networking section).
- [x] Regressions: default 307/307 AND reactor 307/307 (GCC); 307/307 with Clang.

## FFI
- [x] FFI-9 five-platform consumer matrix: historical 25/25 run 38069818109 (SHA 37dd55a)
      preserved as historical evidence; final-candidate CI run 38373411911 (SHA e082b1a)
      executed the FFI/embed CTests per platform — 24/24 PASS on linux-x64-gcc,
      linux-arm64-gcc, linux-x64-clang, windows-x64-msvc; 23/24 on macos-arm64-appleclang
      (Go consumer not provisioned on the macOS path; all other FFI/embed PASS).
- [x] FFI-10 configured Linux-local 4/4 with the Nift C ABI 1.3 SDK (linux-x64-gcc only;
      skips not counted as PASS; not a five-platform claim).

## Sanitized networking (ASan/UBSan, instrumented GENERATED servers via STRUT_CXXFLAGS/LDFLAGS)
- [x] Reusable harness dogfood/sanitized_http_smoke.py (dynamic port, temp fixture, signal-import
      cleanup, EOF-vs-timeout, no pkill).
- [x] Legacy HTTP smoke: PASS (200/hello-asan, exit 0, 0 sanitizer bytes).
- [x] Reactor HTTP smoke:  PASS (200/hello-asan, exit 0, 0 sanitizer bytes).
- [x] Partial-body regression under sanitizers: PASS both modes (7 consecutive driver runs,
      empty stderr; evidence certification/v0.0.5/logs; also under detect_leaks=1).
- [x] Explicit-stop regression under sanitizers: PASS under `ASAN_OPTIONS=detect_leaks=0`
      (both modes, repeated) AND resolved under `detect_leaks=1`. The exit-time retention was
      Defect C: a route handler strongly capturing its own `http_server` formed the documented
      reference-counting cycle. Root cause was a compiler gap (the focused/minimal+light
      runtime slices did not emit `strut_weak`/`strut_raw`/shared_ptr-vs-null helpers), fixed at
      1b0560b. The explicit-stop fixture now uses explicit weak capture and passes strict
      `detect_leaks=1` in legacy and reactor (process exit 0, no report); the strong-cycle
      negative control is retained as dogfood/ws_drain_probes/run_strong_cycle_control.py and
      asserts a recognizable LeakSanitizer report (not merely a nonzero exit). The non-blocking
      CI exception was removed; strict explicit-stop LSan is now a mandatory gate.
- [x] Legacy/Reactor P4 and P5 under sanitizers: PASS both modes, detect_leaks=1 + quarantine
      disabled (evidence logs/asan_ubsan_p{4,5}_{legacy,reactor}_v2_*.log).
- [x] TLS/WSS lifecycle under sanitizers: PASS (covered by P4/P5 plaintext+TLS runs).
- [x] DEFECT FOUND + FIXED during this campaign (commit 4b5e171): reactor WebSocket connection
      reference cycle (conn->websocket_job self-captured ws_conn + run, never cleared) leaked
      every reactor WebSocket connection + run_state; reactor_close also never freed conn->tls
      (leaked OpenSSL session per reactor TLS connection). Legacy unaffected. Fixed by clearing
      websocket_job after invocation and SSL_free in reactor_close. Verified leak-free under
      detect_leaks=1; GCC -Werror CTest 44/44 default + 44/44 reactor.
- [x] Applicable TSan: PASS. Legacy CI scope (http_worker, http_client_streaming,
      websocket_runtime, process_lifecycle, pty, pty_lifecycle, websocket_pty_session) PASS
      under -fsanitize=thread, plus reactor-mode focus (P4/P5 + drain probes). TSan found a
      reactor `workers_stopping` data race fixed at 65eefb1.

## WebSocket dogfood (admission-drain fix implemented; validation ongoing)
- [x] Root cause (independent Codex review): finite-admission shutdown interrupts an admitted
      FIRST request before the HTTP 101 is sent (`accepting` conflated admission exhaustion with
      explicit stop). Fixed by separating `cancellation_requested` from `accepting` (request_stop
      drains first requests on admission exhaustion; explicit stop retains cancellation);
      idle/served_request transition moved under run->mutex (no data race). Fix SHA: 02b50f8.
- [x] Deterministic worker-first regression (permanent driver
      dogfood/websocket_admission_drain_regression.py, commits 421f6fa/64b28a7 + negative
      control): forced worker-first AND stop-first orderings both yield valid HTTP/1.1 101 with
      the correct Sec-WebSocket-Accept key and exit 0. Historical negative control (strengthened):
      the pre-fix original_worker_first control must reproduce the ORIGINAL signature (TCP
      connected, server exited 0, zero bytes received -- no crash/timeout counted as a pass) and
      does. This proves the HARNESS distinguishes original from corrected FROZEN implementations;
      it does NOT by itself protect future changes to src/generated_runtime.cpp. LIVE-runtime
      linkage is the separate outstanding item: the ad-hoc emit_probe (test-only CppBackend tool)
      was ABANDONED after bounded diagnosis -- the production CLI compiles the committed probe.p
      cleanly (exit 0), so the emitter's spurious parse diagnostics are a hand-assembled
      front-end divergence, not a fixture defect; the scaffold was removed (no dead experiment
      infra). A checked transformation of src/generated_runtime.cpp is the sanctioned alternative
      but is NOT yet implemented.
      Worker-first under ASan+UBSan:
      valid 101. NOTE: probes are pinned runtime snapshots (the compiler currently has no C++
      emission path to auto-rebuild them from src/generated_runtime.cpp); the live
      http_websocket_certification.py covers the CURRENT emitted runtime - reconnection to the
      live emission path remains the documented residual gap.
- [x] Existing WebSocket certification post-fix: 45/45 consecutive runs (previously intermittent
      ~1/10); reuse-first/reuse-second finite-limit-one scenarios covered by the cert.
- [x] Full default CTest 43/43; full reactor CTest 43/43 (final candidate; see Local walls).
- [x] Sanitized certification PASS on the affected platform: partial-body/explicit-stop/P4/P5
      PASS under ASan+UBSan in both legacy and reactor modes (2026-10-13 campaign, candidate
      SHA 4b5e171). CI sanitized dogfood step now PASSes (run 38373411911 on e082b1a).
- [x] No retries added solely to conceal the failure; gate intact.
TRANSPORT PARITY (current runtime):
- [x] TLS/WSS finite-admission parity: http_websocket_certification tls-ws uses listen_tls(...,1)
      (finite admission) and asserts a valid HTTP/1.1 101 WSS upgrade with the acceptance key over
      TLS; part of the 45/45 current-runtime runs (tests/fixtures/tls certs exist).
- [x] Legacy TCP finite-admission WebSocket (foundation-accept + reuse-first/reuse-second limit 1)
      passes 45/45 current-runtime; websocket_runtime_certification passes in legacy mode.
- [x] REACTOR ROUTING FIXED (Codex diagnosis + fix integrated): root cause was a C++ preprocessor
      ordering defect - strut_route_has_websocket was emitted BEFORE #define STRUT_USE_WEBSOCKET,
      so its #ifdef permanently selected `return false` and reactor discarded every websocket
      route (legacy unaffected). Replaced with compile-time member detection:
      `if constexpr(requires { r.websocket; }) return (bool)r.websocket; else return false;`.
      VERIFIED process-safe matrix (dogfood/ws_drain_probes/run_reactor_probe.py, fresh server
      per (mode,request) row, isolated ports, current runtime), matrix now PASSES in BOTH modes:
          legacy:  /plain 200 | /ws-upgrade 101+accept-key | /missing 404 | /ws-no-upgrade 400
          reactor: /plain 200 | /ws-upgrade 101+accept-key | /missing 404 | /ws-no-upgrade 400
      A no-websocket-route fixture (/plain only) compiles and runs cleanly, exercising the
      constexpr else path.
- [x] REACTOR WEBSOCKET CLOSE LIFECYCLE (CORRECTED finding): with a PROTOCOL-CORRECT client
      (masked frames reusing the certification client_frame semantics; leftover bytes from the
      handshake recv fed into the buffered frame parser -- the earlier "missing close frame"
      claim was a client-buffering artifact), the /explicit-close route shows BOTH modes send
      the server CLOSE frame (op 8, code 1001) and answer the masked Ping with Pong. RESOLVED
      on candidate: run_ws_close_probe now shows reactor eof_after_close=eof (final TCP EOF
      matches legacy; "reactor leaves it open/recv timeout" divergence is gone) in plain,
      ASan/UBSan (v2) and TSan runs.
- [x] REACTOR FINAL-CLOSE FIXED (Codex reactor_arm_streams close-scheduling patch, integrated at
      7cb4c87): the residual strict-probe reactor timeout was a STALE-COMPILER artifact --
      run_ws_close_probe.py defaulted to repo-root build/strut (dated Oct 7, pre-fix); with the
      freshly rebuilt compiler (and Codex's known-good compiler) the strict regression PASSES in
      BOTH modes (reactor eof, rc=0), matching Codex's demonstration; only the Oct-7 binary
      reproduced the timeout. Harness now selects the most recent candidate (env STRUT
      authoritative). Full websocket_runtime_certification passes legacy + reactor;
      wall 43/43 both.
      Reactor CI coverage is now MANDATORY on linux-x64-gcc (plain: P10 flood + worker cert +
      saturation + P4/P5; sanitized: P10+P4/P5 under ASan/UBSan detect_leaks=1; TSan:
      P10+worker+P4/P5). Finite-admission reactor request_stop and parameter propagation are
      fixed and verified (c11a9d2; run_reactor_param_probe).
- [x] REACTOR PARAMETER PROPAGATION FIXED + end-to-end VERIFIED (c11a9d2 fix; permanent driver
      dogfood/ws_drain_probes/run_reactor_param_probe.py): the handler accepts only when
      request.params["id"]=="abc123" (no stdout oracle); legacy-positive 101 / legacy-negative
      no-101 / reactor-positive 101 / reactor-negative no-101 all PASS -- correct captured values
      reach reactor WebSocket handlers in both modes.
- [x] REACTOR P4 remaining divergences RESOLVED on candidate (http_websocket_certification
      reactor-mode PASSes in plain, ASan/UBSan and TSan runs; run_reactor_param_probe
      reactor-negative returns 403 PASS matching legacy):
      (a) HANDLER-DECLINE: reactor now answers HTTP/1.1 403 Forbidden (was: closes with NO
      bytes) - reactor-negative expects 403 and PASSES (the previous regression is gone).
      (b) MALFORMED-EXTENSION: malformed Sec-WebSocket-Extensions return 400 in BOTH modes on
      the multi-route P4 server (P4 cert asserts all 19 invalid openings -> 400 and PASSES in
      reactor). No 405->400 blanket conversion was needed.
      Previously: reactor ws-upgrade 404 (pre-fix), now 101 with full P4 parity.
      Separate from the admission race. CI now sets STRUT_HTTP_REACTOR in the mandatory reactor
      stages (reactor coverage is no longer local-only).
- [x] Explicit-stop validation (stop during drain, incomplete POST body, bounded deadlines):
      PASS both modes; permanent driver run_reactor_explicit_stop.py. Verified plainly, under
      ASan/UBSan (repeated) and TSan. The stop-during-drain path is separately exercised by
      http_websocket_certification stop-race scenarios.
- [x] Sanitized certification PASS on the affected platform maintained AND CI sanitized dogfood
      step now PASSes (workflow run 38373411911, linux-x64-gcc "Certify sanitized generated
      HTTP runtime" comprising http_websocket, websocket_runtime, http_worker, pty, P10 etc.
      under ASan/UBSan).
  PTY THREAD-COUNT RELEASE ISSUE (separate from the WebSocket race): ASSESSED. Historical
      P10 "threads did not return 11 -> 12" (runs 38034787150/38069818109, SHA 37dd55a) is
      CONFIRMED as a historical failure, but its root cause is NOT CONCLUSIVELY ESTABLISHED
      (measurement transient vs short-lived leak vs test accounting all remain possible).
      Current candidate: NOT OBSERVED (P10 plain 34->34, ASan 10->10 x8, TSan 11->11; P7/P8
      threads return exactly, allowance 0). Assertions not weakened. Permanent reactor/legacy
      CI now re-runs P10 under ASan/TSan on every push, which will provide the ongoing evidence
      to accept or retire the residual risk.
  PTY P8 INTERRUPT/ETX RESIDUAL (tracked follow-up; NOT release-blocking): an intermittent
      Windows-only failure in the P8 PTY resource program's interrupt/ETX check
      (`pty_lifecycle_certification.py` -> `certify_resources` return 4;
      exit=4 seen_done=False stdout=['RESOURCE_BASELINE']). exit=4 is a normal program return
      (the emitted `main` wrapper maps uncaught errors to exit 70 + stderr; stderr was empty),
      i.e. the interrupt helper did not report readiness/`caught=INT` before termination. Root
      cause NOT conclusively established; classification: intermittent ConPTY Ctrl+C delivery
      timing. The assertion was left intact (not weakened); the last clean candidate passed and
      reproduced no failure. The macOS P8 owner-shutdown failure was a separate, DEMONSTRATED
      fixture race (a fixed `sleep_ms(100)` before requiring the descendant PID file) corrected
      at 2c9c7e8. Follow-up: investigate the Windows interrupt/ETX path after v0.0.5 with
      server-side evidence; do not reduce it to a dismissed test flake.

## Five-platform release certification (one final SHA)
- [x] Linux x64 GCC / Linux x64 Clang / Linux ARM64 GCC / macOS ARM64 AppleClang /
      Windows x64 MSVC: Build PASS, CTest PASS, dogfood certification PASS. Final candidate
      SHA 2c9c7e8, workflow run 38529518056 — five-platform PASS on the FIRST attempt (no
      reruns). CTest 43/43 (macos-arm64-appleclang 42/42, Go consumer not provisioned);
      linux-x64-gcc also PASS on the sanitized reactor HTTP runtime (weak explicit-stop +
      strong-cycle control + weak-ownership certification under detect_leaks=1) and the
      reactor TSan stage. Earlier run 38373411911 on e082b1a is historical evidence only.

## Release artifacts
- [x] Staged install verified (header + lib + bin + share/strut/jsonic) at /tmp/strut-prefix:
      relocated-prefix C consumer compiles and runs add(2,3)=5 without developer paths
      (STRUT_JSONIC_INCLUDE_DIR + LD_LIBRARY_PATH are the documented JSON runtime-support
      hook, set identically in the CTest consumers); staged strut --version = 0.0.5.
- [x] Ownership docs current: `MEMORY_SAFETY.md` / `MEMORY_HARDENING.md` / `LANGUAGE_HANDOVER.md`
      describe `weak_ptr<T>` (construct `weak(ptr)`, promote with `.lock()`, query with
      `.expired()`) and the reference-cycle guidance; this now matches the implementation for
      ordinary structs, native `http_server` handles, and structs containing native handles
      (fix 1b0560b).
- [ ] Packaged README/CHANGELOG live in the separate website repository; the in-repo
      `FFI_HANDOVER.md` / `FFI_ABI_AUDIT.md` / `WEBSOCKET_INVESTIGATION_HANDOFF.md` remain
      current. Final release-note prose is to be written at publish time.
- [ ] v0.0.4 untouched; v0.0.5 tag/release issued only after explicit approval.