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
      linkage is the separate outstanding item (a test-only emit_probe tool reusing the public
      CppBackend API now exists and compiles; the fixture in probe.p uses strut_string.to_int(),
      which is not bound by this compiler, so live generation is scaffolded-not-yet-wired).
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
- [ ] TLS WSS and reactor admission-drain parity: legacy TCP proven; TLS/reactor not yet
      separately reproduced (Codex-dependent; reactor reading/waiting_body interruption unchanged).
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