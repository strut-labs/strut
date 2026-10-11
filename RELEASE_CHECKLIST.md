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

## WebSocket dogfood (release blocker)
- [ ] Root cause identified and attributed (scenario + server evidence) OR independent review
      (WebSocket investigation now owned by Codex from SHA 04d8675) establishes a demonstrated
      defect + correction.
- [ ] Ordinary certification PASS; sanitized certification PASS on the affected platform.
- [ ] No retries added solely to conceal the failure; gate intact.

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