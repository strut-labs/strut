# WebSocket certification failure — independent investigation handoff

Concise, self-contained context for an independent (read-only) reviewer. Strut repo, branch
`main`, starting SHA **`04d8675`** (the SHA that created this handoff). Do NOT modify the runtime
or the certification harness while investigating.

## Observed failure
Exactly one intermittent failure signature, in the **dogfood certification stage** (run AFTER the
CTest suite):

    RuntimeError: unexpected WebSocket upgrade response: b''

i.e. the certification client connected to a generated Strut HTTP/WebSocket server and the
connection closed **before delivering even the first byte of an HTTP upgrade response**.

## Known evidence
- **Historical CI runs** (all failing at the `Certify sanitized generated HTTP runtime` /
  `Certify generated C++ and dogfood` dogfood stage, Linux jobs; the same job's CTest passed 38/38
  immediately before):
  - run `38034787150` linux-x64-gcc (PTY/HTTP sanitizer cert stage)
  - run `38053208025` linux-arm64-gcc (`RuntimeError: unexpected WebSocket upgrade response: b''`)
  - run `38060567145` and `38069818109` linux-x64-gcc (same message)
  - Later runs green; the failure is intermittent across many runs, never on macOS/Windows.
- **Local reproduction**: reproduced once (~1-in-10) in a rapid plain-mode stress loop
  (`for i in $(seq 1 10); do python3 dogfood/http_websocket_certification.py build/strut ...`)
  before instrumentation; not reproduced since, despite a 24-run `STRUT_WEBSOCKET_DIAG=1` campaign.
  Frequency and cause unestablished.
- The failing server runs **unsanitized** in the local reproduction; all observed CI failures are
  in the **sanitized** ("Certify sanitized generated HTTP runtime") step, which is ASan/UBSan-built.
  Whether sanitizer startup/termination changes behaviour is an open question.
- The certificate also executed in ordinary (non-sanitized) CI steps historically without failure.

## Relevant source files
- `dogfood/http_websocket_certification.py` — the Python certification harness:
  - `exchange()` (line ~54): client connect/send/read loop; `retry`/`retry_empty` semantics;
    does NOT establish full server-app readiness (it retries TCP connect only).
  - `require_switch()`: asserts an HTTP/1.1 101 response; on empty/other responses raises
    `unexpected WebSocket upgrade response: b'' [scenario=...]`.
  - `main()`: compiles several generated servers from embedded Strut `.p` sources
    (server / shutdown-server / stop-server / accept-race-server / TLS server), runs them as
    subprocesses, and drives the scenarios sequentially on the SAME long-lived `server` process
    (plus per-scenario servers for shutdown/stop/TLS). Scenario labels and server-process
    association are already wired at all 11 `require_switch` call sites.
- `src/generated_runtime.cpp` — the **generated** HTTP/WebSocket runtime:
  - `strut_serialize_websocket_switching_protocols()` — constructs the 101 response
    (`HTTP/1.1 101 Switching Protocols ... Sec-WebSocket-Accept`) AFTER the connection is accepted.
  - the WebSocket `accept(protocol)` path: a `commit` state machine (offered-protocol check,
    `commitment: committing -> committed`); intentional pre-101 closes exist (e.g. handler returns
    without accept → 403, `/reject` route → 403, subprotocol-not-offered → error).
  - `strut_websocket_sha1/base64` — `Sec-WebSocket-Accept` computation.
- `src/` HTTP server runtime: connection acceptance, request parse/read, upgrade validation,
  worker dispatch, server stop/cancellation, socket lifetime.
- `.github/workflows/cross-platform.yml` — the dogfood steps that run the certification, including
  the sanitized build for `Certify sanitized generated HTTP runtime` (Linux x64 GCC).

## Reproduction commands
    # ordinary (unsanitized) build + full cert
    cmake --build build -j4
    python3 dogfood/http_websocket_certification.py build/strut
    # diagnostic self-check (does not need a server)
    python3 dogfood/http_websocket_certification.py --self-check
    # optional server-side diagnostics on failure
    env STRUT_WEBSOCKET_DIAG=1 python3 dogfood/http_websocket_certification.py build/strut
    # rapid stress (may take many iterations before/without reproducing)
    for i in $(seq 1 10); do python3 dogfood/http_websocket_certification.py build/strut || break; done

Recommended process: perform your OWN initial inspection of the runtime and harness BEFORE
relying on this document, and treat the hypotheses below skeptically - they are options to test,
not conclusions to confirm.

## Current hypotheses (all UNPROVEN; do not treat any as established)
1. **Harness readiness/teardown race** — `exchange(retry=True)` retries TCP connect only; a
   connect can succeed while the server/app is not yet able to serve; a previous scenario's
   shutdown may interfere with the next (all primary scenarios share one long-lived server
   process; next scenario's request may race the previous connection's cleanup).
2. **Request-read / partial-read close** — the accepted connection fails or times out before the
   request is fully read, and the server closes without sending 101/403. Is there a read timeout
   mis-handled as EOF?
3. **Accepted-socket closed before headers** — a genuine server path that closes an accepted
   socket before writing the upgrade response (beyond the known intentional 403 paths).
4. **Sanitizer process termination / startup** — the recurring CI failures are all in the
   sanitized step; server process exit due to a sanitizer-detected condition (or harness cleanup)
   would manifest as an empty response.
5. **Worker/accept dispatch race on stop** — `app.stop()` / cancellation racing an accept that
   has not yet dispatched a worker (relevant to `/shutdown`, `/wait`, `/race` scenarios).
6. **Previous-scenario interference** — a leftover connection or an invalidation from a prior
   scenario closing a socket that the current scenario touches.

Separate **demonstrated facts** from the above: the failure signature, its intermittency, its
Linux-only occurrence in CI, and its (single, un-attributed) local reproduction.

## Diagnostic changes already attempted (instrumentation ONLY; no runtime change)
- Scenario labels at all 11 `require_switch()` call sites; explicit `scenario=` arg.
- Server-process association (primary server, tls-server, stop-server, race-server) into
  `require_switch` for server-state/stderr on failure (gated by `STRUT_WEBSOCKET_DIAG=1`,
  best-effort/nonblocking).
- `--self-check` CLI regression for the failure path.
- No retries added; no assertions weakened; the certification gate is intact.

## Constraints
- Prefer a read-only review in a separate checkout/worktree at `f00a82b`.
- Any proposed runtime/harness correction should be reported back before implementation.