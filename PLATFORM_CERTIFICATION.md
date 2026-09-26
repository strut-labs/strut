# Platform certification status

## Local Linux development host

The current source has been built/tested locally with warnings-as-errors using both GCC and Clang. Final local audit after CP114:

- GCC internal CTest: 13/13 passing;
- Clang internal CTest: 13/13 passing;
- deterministic frontend fuzz test included in those runs;
- sanitizer-backed ptr/weak_ptr/thread stress: passing;
- concurrency stress: 20/20 passing;
- native Linux x64 cross-target smoke has been exercised previously.

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
- CP115: the complete release-candidate regression, package, generated-code, dogfood, and documentation gates are green.
- CP116 remains the explicit `v0.0.1` tag and release action.
