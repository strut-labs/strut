# Strut v0.0.5 release-certification status

Persistent, tracked evidence log for the v0.0.5 release-certification campaign.
This document distinguishes **historical** results (obtained against an earlier
HEAD) from **candidate-SHA** results. Only results executed against the CANDIDATE
SHA listed below may satisfy release gates.

## Candidate

| Field | Value |
|---|---|
| Candidate SHA | `83744237627893b9ee13a37df7429ed4cddddd27` |
| Explicit compiler | `/home/nick/relgcc/strut` (release candidate build, authoritative) |
| Branch | `main` |
| Remote | in sync with `origin/main` |
| Session start (UTC) | 2026-10-13 |

Candidate promoted lineage: `0a27604` (previous verified HEAD) ->
`2904d68` -> ... -> `8374423` (hardened ws_drain_probes drivers + gitignore).

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

### Open / in progress

| Result | Gate | SHA | Evidence / notes |
|---|---|---|---|
| PENDING | ASan/UBSan partial-body + explicit-stop (legacy+reactor) | candidate | |
| PENDING | ASan/UBSan P4/P5 (legacy+reactor) | candidate | |
| PENDING | ASan/UBSan TLS/WSS lifecycle | candidate | where supported |
| PENDING | TSan | candidate | |
| PENDING | PTY thread-count discrepancy | candidate | |
| PENDING | GCC/Clang -Werror full CTest | candidate | |
| PENDING | FFI final-candidate ABI/embedding | candidate | |
| PENDING | Five-platform CI matrix | candidate | |

## Residual limitations (preserved)

- Deterministic WebSocket scheduling probes exercise frozen generated-C++ snapshots.
  They demonstrate the original admission race and its correction but do not provide
  deterministic scheduling coverage of subsequent live-runtime changes. Current-runtime
  integration suites provide separate executable coverage. Not restarted for v0.0.5.
- Reactor HTTP/WebSocket has zero CI coverage (CI never sets `STRUT_HTTP_REACTOR`);
  local reactor coverage is the evidence source.