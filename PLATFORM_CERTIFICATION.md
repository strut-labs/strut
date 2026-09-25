# Platform certification status

## Local Linux development host

The current source has been built/tested locally with warnings-as-errors using both GCC and Clang. Final local audit after CP114:

- GCC internal CTest: 13/13 passing;
- Clang internal CTest: 13/13 passing;
- deterministic frontend fuzz test included in those runs;
- sanitizer-backed ptr/weak_ptr/thread stress: passing;
- concurrency stress: 20/20 passing;
- native Linux x64 cross-target smoke has been exercised previously.

This is useful Linux-x64 evidence but does **not** close CP98 because the full independent regression suite and Linux arm64 CI certification still belong in the release matrix.

## GitHub Actions

`.github/workflows/cross-platform.yml` provides compiler build/CTest jobs for:

- Linux x64 — GCC;
- Linux x64 — Clang;
- Linux arm64 — GCC on the GitHub arm64 runner;
- macOS arm64 — AppleClang;
- Windows x64 — MSVC.

The independent regression repository has its own manually-triggered cross-platform workflow for Linux x64, macOS arm64, and Windows x64. It accepts a compiler git ref so a specific candidate can be certified without silently testing a different revision.

## Pending checkpoints

- CP98 remains open until Linux CI/full regressions are recorded green (including arm64 where practical).
- CP99 remains open until macOS arm64 CI/regressions are recorded green.
- CP100 remains open until Windows x64 CI/regressions are recorded green.
- CP115 is blocked on that supported-platform matrix plus the complete release-candidate regression/package/docs gates.
- CP116 is an explicit release action and must not be performed merely because implementation is locally complete.
