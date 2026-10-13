# Strut v0.0.5 release-certification checklist

Final gate list. All items must be evidenced from the FINAL implementation SHA; no item may be
inferred from an earlier state or a different platform's results.

## ABI / compatibility
- [ ] `include/strut/embed.h` compiles standalone as C11 and C++20 (source-tree AND staged-prefix).
- [ ] `strut_embed_value` layout measured and verified against the five-platform C# mirror
      (size 72 / align 8 / kind@0 i@8 s@32 retained@48 cb@56 on x86-64 Linux).
- [ ] Exported symbols present and unmangled in the installed library (Linux; macOS/Windows via
      consumer evidence + symbol lists where practical).
- [ ] No retrospective v0.0.4 binary-compat guarantee implied; future ABI versioning policy
      documented.
- [ ] `FFI_ABI_AUDIT.md` / `FFI_HANDOVER.md` consistent and accurate.

## Local walls
- [ ] GCC -Werror: full CTest PASS (enumerate count; current baseline 42/42).
- [ ] Clang -Werror: full CTest PASS.
- [ ] ASan/UBSan: applicable native CTest PASS (interpreter-hosted consumers excluded, documented).
- [ ] Regressions: default 307/307 AND reactor (`STRUT_HTTP_REACTOR=1`) 307/307.

## FFI
- [ ] FFI-9 five-platform consumer execution 25/25 (run 38069818109 evidence preserved).
- [ ] FFI-10 configured Linux-local 4/4 with the Nift C ABI 1.3 SDK; skips not counted as PASS.

## Sanitized networking (ASan/UBSan, instrumented GENERATED servers via STRUT_CXXFLAGS/LDFLAGS)
- [x] Reusable harness dogfood/sanitized_http_smoke.py (dynamic port, temp fixture, signal-import
      cleanup, EOF-vs-timeout, no pkill).
- [x] Legacy HTTP smoke: PASS (200/hello-asan, exit 0, 0 sanitizer bytes).
- [x] Reactor HTTP smoke:  PASS (200/hello-asan, exit 0, 0 sanitizer bytes).
- [ ] Partial-body regression under sanitizers: PENDING (execution sessions interrupted; not
      observed as pass).
- [ ] Explicit-stop regression under sanitizers: PENDING (interrupted; not observed).
- [ ] Legacy/Reactor P4 and P5 under sanitizers: PENDING (not executed in-session).
- [ ] Applicable TSan: PENDING (separate).

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
- [x] Full default CTest 42/42; full reactor CTest 42/42.
- [ ] Sanitized certification PASS on the affected platform: deterministic worker-first probe
      PASS locally under ASan+UBSan; CI sanitized dogfood step still pending.
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
- [ ] REACTOR WEBSOCKET CLOSE LIFECYCLE (CORRECTED finding): with a PROTOCOL-CORRECT client
      (masked frames reusing the certification client_frame semantics; leftover bytes from the
      handshake recv fed into the buffered frame parser -- the earlier "missing close frame"
      claim was a client-buffering artifact), the /explicit-close route shows BOTH modes send
      the server CLOSE frame (op 8, code 1001) and answer the masked Ping with Pong; the true
      divergence is the final TCP EOF: legacy closes the socket after the close handshake
      (recv returns b''), reactor leaves it open (recv times out), matching
      websocket_runtime_certification exercise_server_closing's recv(1) timeout.
- [x] REACTOR FINAL-CLOSE FIXED (Codex reactor_arm_streams close-scheduling patch, integrated at
      7cb4c87): the residual strict-probe reactor timeout was a STALE-COMPILER artifact --
      run_ws_close_probe.py defaulted to repo-root build/strut (dated Oct 7, pre-fix); with the
      freshly rebuilt compiler (and Codex's known-good compiler) the strict regression PASSES in
      BOTH modes (reactor eof, rc=0), matching Codex's demonstration; only the Oct-7 binary
      reproduced the timeout. Harness now selects the most recent candidate (env STRUT
      authoritative). Full websocket_runtime_certification passes legacy + reactor;
      wall 42/42 both.
      Codex's high-priority finite-admission reactor request_stop + parameter
      propagation also remain; CI reactor coverage still absent.
- [x] REACTOR PARAMETER PROPAGATION FIXED + end-to-end VERIFIED (c11a9d2 fix; permanent driver
      dogfood/ws_drain_probes/run_reactor_param_probe.py): the handler accepts only when
      request.params["id"]=="abc123" (no stdout oracle); legacy-positive 101 / legacy-negative
      no-101 / reactor-positive 101 / reactor-negative no-101 all PASS -- correct captured values
      reach reactor WebSocket handlers in both modes.
- [ ] REACTOR P4 remaining divergences (isolated, current runtime):
      (a) HANDLER-DECLINE: when a WebSocket handler returns without accepting, legacy answers
      HTTP/1.1 403 Forbidden but reactor closes the connection with NO bytes (EMPTY/closed) -
      confirmed on a /ws/:id route in both modes; the hardened run_reactor_param_probe.py
      negative control now expects 403 and FAILS on reactor (the regression for this defect).
      (b) MALFORMED-EXTENSION: malformed Sec-WebSocket-Extensions (permessage-deflate; =bad)
      returns 400 in BOTH modes on isolated static and /ws/:id single-route fixtures - the
      cert's 405 only manifests in the multi-route P4 server (composition-dependent); needs the
      multi-route server repro to pin the 405 branch.
      Do not blanket-convert 405->400.
      Previously: reactor ws-upgrade 404 (pre-fix), now 101.
      Separate from the admission race (not admission-related). CI never sets STRUT_HTTP_REACTOR
      (no activation found in scripts/workflow); reactor HTTP/WebSocket has zero CI coverage.
- [ ] Explicit-stop validation (stop during drain, partial headers, keepalive, active WS,
      bounded deadlines): not yet separately exercised.
- [ ] Sanitized certification PASS on the affected platform: deterministic worker-first probe
      PASS locally under ASan+UBSan (frozen snapshot); CI sanitized dogfood step still pending.
  SEPARATE RELEASE ISSUE (not the WebSocket race): PTY thread-count certification failures
      observed historically at CLI runs (e.g. 38034787150/38069818109 x64 jobs) must be assessed
      independently before overall release approval.

## Five-platform release certification (one final SHA)
- [ ] Linux x64 GCC / Linux x64 Clang / Linux ARM64 GCC / macOS ARM64 AppleClang /
      Windows x64 MSVC: Build PASS, CTest PASS, dogfood certification PASS (each reported
      separately; a platform is NOT fully certified if any mandatory stage failed).

## Release artifacts
- [ ] Staged install verified (header + lib + bin + share/strut/jsonic) and relocated-prefix
      consumers run without developer paths.
- [ ] README/CHANGELOG/install docs consistent; `FFI_HANDOVER.md`, `FFI_ABI_AUDIT.md`,
      `WEBSOCKET_INVESTIGATION_HANDOFF.md` current.
- [ ] v0.0.4 untouched; v0.0.5 tag/release issued only after explicit approval.