# Platform certification status

## Local Linux development host

The `v0.0.3` source has been built/tested locally with warnings-as-errors using both GCC and Clang:

- GCC internal CTest: 16/16 passing;
- Clang internal CTest: 16/16 passing;
- deterministic frontend fuzz test included in those runs;
- sanitizer-backed ptr/weak_ptr/thread stress: passing;
- concurrency stress: 20/20 passing;
- native Linux x64 cross-target smoke has been exercised previously.
- CP6 cancellation certification passed locally on Linux for 50 process cycles, each with 32 waiting observers and 8 concurrent cancellation requests. CP6 adds only standard C++ synchronization primitives; macOS and Windows execution remains pending their CI jobs.

This local evidence is supplemented by the completed supported-platform CI matrix below.

## GitHub Actions

`.github/workflows/cross-platform.yml` provides compiler build/CTest jobs for:

- Linux x64 — GCC;
- Linux x64 — Clang;
- Linux arm64 — GCC on the GitHub arm64 runner;
- macOS arm64 — AppleClang;
- Windows x64 — MSVC.

The independent regression repository has its own manually-triggered cross-platform workflow for Linux x64, macOS arm64, and Windows x64. It accepts a compiler git ref so a specific candidate can be certified without silently testing a different revision.

## Completed release-candidate certification

- CP98: Linux x64 GCC/Clang and Linux ARM64 GCC compiler certification is green.
- CP99: macOS ARM64 AppleClang compiler and regression certification is green.
- CP100: Windows x64 MSVC compiler and regression certification is green.
- CP115: the complete 155-case regression, package, generated-code, dogfood, documentation, sanitizer, and performance gates are green.
- The published `v0.0.2` tag remains immutable at `ed7c5a7cb9227c37cd737dbaedcfaa3624fbdf11`.
- `v0.0.3` is published and immutable at `cf75cd6389373f727c628b85afdf978a0afd3af5`; its four package jobs and publish job passed, and all archives match the published `SHA256SUMS`.
