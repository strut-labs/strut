# Strut Handover

This repository contains the Strut programming language.

Strut is intended to be a concise, memory-safe, natively compiled general-purpose language with strong C++ and Nift familiarity, deterministic reference-counted memory management, real concurrency, agent-friendly syntax, and simple deployment.

The language is deliberately willing to borrow good ideas from C++, Nift, Go, Rust, Zig, and elsewhere, but it should not inherit complexity merely for compatibility. The design priorities are:

1. memory safety by default;
2. native performance;
3. brevity and low ceremony;
4. readable, unsurprising syntax;
5. deterministic behaviour and memory management;
6. real multithreading and explicit low-level control when wanted;
7. strong web/backend capability without becoming a web-only language;
8. simple single-binary deployment;
9. strong AI/agent DX;
10. a small core/stdlib with a curated package ecosystem;
11. first-class static and dynamic linking with executable size treated as a performance concern;
12. familiar stream/process/system programming facilities without inheriting C++ declarator ambiguity.

Strut has **no tracing garbage collector**. Normal owning pointers are safe and reference-counted. Raw unmanaged pointers require `unsafe`.

## Workspace repositories

The expected development workspace currently contains three sibling repositories:

- `strut/` — compiler, runtime, stdlib, package tooling, documentation handovers;
- `strut-regression-suite/` — independent language/compiler regression and compatibility suite;
- `strut-labs.github.io/` — public website and multi-page documentation, built with Nift.

Treat the three repositories as one coordinated project. A language feature is not complete merely because compiler code exists: its tests, regression coverage, documentation, examples, and website should move with it.

## Start here

Before implementing or changing language behaviour, read:

1. `LANGUAGE_HANDOVER.md` — current language design contract;
2. `IMPLEMENTATION_HANDOVER.md` — ordered checkpoint roadmap; its "Extended roadmap" section records the authoritative long-term sequencing (HTTP/runtime performance => FFI/Nift => language maturity => releases => low-level systems readiness) and why that ordering was chosen;
3. `REGRESSION_HANDOVER.md` — regression-suite contract;
4. `WEBSITE_HANDOVER.md` — website/docs requirements and maintenance contract;
5. `FFI_HANDOVER.md` — two-way FFI / embedding baseline, ABI contract, and checkpoint plan (active major roadmap item for 0.0.5).

When a design decision changes, update these documents before or alongside implementation so later work does not silently revive superseded syntax.

## Current status

Strut `v0.0.3` was published historically from commit `cf75cd6389373f727c628b85afdf978a0afd3af5`. The current release is **`v0.0.4`, PUBLISHED** (annotated tag, immutable commit `ce593b4927a05531f2ccab81f4204c8bea05c8b5`) — a large HTTP/backend, runtime, and performance release (opt-in reactor HTTP server via `STRUT_HTTP_REACTOR=1`, reactor-native TLS, request/response streaming, WebSockets, indexed routing, request/response hot-path and runtime building-block work). The tag-triggered release matrix is green on Linux x64/ARM64, macOS ARM64, and Windows x64, and the `main` cross-platform compiler certification is green. Published assets: `strut-0.0.4-linux-x64.tar.gz`, `strut-0.0.4-linux-arm64.tar.gz`, `strut-0.0.4-macos-arm64.tar.gz`, `strut-0.0.4-windows-x64.zip`, `SHA256SUMS`. Current development line: **0.0.5** (unreleased; no tag) — the active major roadmap item is **two-way FFI / embedding** (stable C ABI, native->Strut calls, callbacks, ownership/lifetime, checked-error propagation, cross-platform ABI correctness from day one). Release-certification blockers fixed en route: response-serializer `-Werror` unused-but-set; generated `extern "C"` `-Wreturn-type-c-linkage` (AppleClang, narrow suppression); portable unhandled-`std::exception` reporting in generated `main` via `std::cerr` (Windows); `check_release.py` CMake-version default.

The authoritative architecture now includes dependency-driven runtime components, structured `TypeId`, a canonical API registry shared by semantic analysis/code generation/editor tooling, LSP integration, reproducible Git packages with immutable locks and verified offline caches, official package shorthand, custom checked errors, atomics, distinct prefix/postfix operator identities, HTTP/TLS lifecycle handling, executable documentation, controlled AI-DX benchmarks, and focused string/thread/atomic runtime slicing. These are implemented foundations, not roadmap items.

## Post-0.0.3 performance baseline

The reproducible evidence and raw-artifact routing are documented in the sibling `strut-benchmarks/PERFORMANCE_0.0.2.md`; despite the filename, it records both the immutable `0.0.2` comparison baseline and the final `0.0.3` candidate results.

- direct A/B Hello World compile: about 315 ms (`1,659 -> 315 ms`);
- direct A/B plain-thread compile: about 795 ms (`2,112 -> 795 ms`);
- focused atomic fixture compile: about 656 ms (`1,824 -> 656 ms`);
- frontend phases: sub-millisecond for the measured tiny programs; native C++ compilation dominates their build time;
- sustained reference-count workload: about `1.21x` equivalent C++ (`52.20 ms` versus `43.08 ms`). Assembly review found the same `std::shared_ptr` ownership sequence plus Strut's required null-safe dereference path, so no safety-preserving micro-fix was justified for `0.0.3`.

## Post-0.0.3 certification baseline

- 155/155 independent black-box regressions;
- 16/16 GCC and 16/16 Clang unit/integration suites;
- ASan/UBSan 16/16 with leak detection disabled only where the sandbox prevents LeakSanitizer ptrace operation;
- Linux x64/ARM64, macOS ARM64, and Windows x64 hosted compiler certification, plus synchronized regression matrices;
- structural code-generation budgets for 13 representative configurations;
- reproducible package graphs, immutable locks, checksum verification, offline cache, repair, concurrency, and update tests;
- HTTP/TLS lifecycle, generated-code/dogfood, LSP stdio, executable-docs, AI-DX, and cross-language performance comparisons.

ThreadSanitizer remains environment-dependent: the local Swift-Clang runtime is unusable because of its libdispatch linkage, so TSAN is used only where the host toolchain provides a working runtime.

## Known limitations after 0.0.3

- package discovery is Git-backed official shorthand plus explicit Git sources; there is no centralized searchable registry;
- the website provides POSIX lifecycle scripts, but no first-party Windows PowerShell installer/update automation;
- HTTP/2, WebSockets, and ACME/certificate automation are not implemented;
- ownership-heavy sustained workloads retain the measured reference-count overhead above;
- conservative generic/type inference still intentionally rejects unresolved or unsafe edge cases rather than guessing;
- editor integration is an LSP server and protocol surface, not yet polished native packaging for major editors;
- native debugging/profiling maps generated C++ well enough for current use, but richer Strut-frame metadata and source-level debugger integration remain open;
- GitHub Actions currently emits upstream Node-runtime and runner-image migration warnings; they do not fail certification but should be cleared as action releases permit.

## Ranked next-phase candidates

1. **Editor/LSP packaging and debugging workflow** — highest near-term adoption and AI-DX leverage, moderate risk; turn the existing protocol foundation into an easy, observable daily experience.
2. **Package ecosystem and registry UX** — high user/adoption leverage, moderate architectural and supply-chain risk; add discovery and publishing ergonomics without weakening immutable Git locks.
3. **Runtime/reference-count profiling and optimization** — focused performance value and architectural leverage, medium-high risk because ownership semantics and safety must remain unchanged.
4. **Backend/server capability** — HTTP/2, WebSockets, operational TLS tooling, and deployment polish offer high backend value but carry substantial protocol/security risk.
5. **Native application/GUI feasibility** — potentially differentiating and high adoption upside, but highest scope and platform risk; begin with a narrow evidence/prototype phase rather than framework implementation.

The recommended next development phase is **editor/LSP packaging plus debugging/profiling integration**. It builds on already-authoritative metadata and LSP architecture, improves human and agent feedback loops, and makes the existing broad language surface easier to adopt without adding another major semantic subsystem. Package UX should follow closely. Do not begin either phase without a separately approved implementation brief.

## Major acceptance target

The first major usefulness milestone is not a toy expression evaluator. Strut should be able to build a production-ish HTTP service that uses JSON, async/concurrency, an official SQLite package, and embedded frontend assets, and compile the entire application into one native executable.

Nift rewrite feasibility has been investigated. A rewrite is technically plausible but deliberately deferred until larger dogfood, cross-platform certification, profiling, and measurable maintenance/performance/safety benefits justify it.

## Working rules

- Keep semicolons mandatory.
- Use the full `function` and `operator` keywords.
- Generic/template declarations use `[T]`; instantiated types continue to use `<T>`.
- Keep operator precedence/fixity language-defined; overload the fixed operator set rather than extending the parser.
- Keep `exec` and fundamental process control in the stdlib.
- Treat both static and dynamic linking as first-class; single-binary deployment is an option, not a mandate.
- Prefer `snake_case` for APIs and library naming.
- Do not add aliases/syntax forms merely because C++ accepts them. Strut should normally have one canonical spelling.
- Safe/common operations should get the shortest syntax; dangerous/uncommon operations should be explicit.
- Do not introduce a tracing GC.
- Do not recreate Rust's syntax or ceremony merely to obtain Rust-like safety properties.
- Prefer compile-time errors to runtime surprises.
- Preserve deterministic destruction where the ownership model permits it.
- Keep the compiler frontend/backend boundary clean enough that the code-generation backend can evolve.
- Every accepted language feature needs positive tests, negative diagnostics tests where applicable, and regression-suite coverage.
- Keep the public website and examples in sync with the implemented language rather than documenting speculative syntax as if shipped.

## Cross-platform requirement

Strut is cross-platform by design. Linux is the initial development host, but compiler architecture, runtime boundaries, filesystem/process abstractions, generated code, packages, and CI/release planning must preserve Windows and macOS as first-class targets rather than porting them as an afterthought.

## Deferred

Server-side/Nift-style templating has been removed from the active roadmap for now. Revisit it later only if the core language and web/backend story make it clearly worthwhile.

## Performance implementation rule

Prefer allocation-light standard-library conversion primitives such as `std::from_chars` / `std::to_chars` where they fit, following the performance lessons from JSONIC, rather than stream-based or exception-heavy conversions. Preserve correctness and useful diagnostics first, then benchmark.

## Bootstrap implementation standard

The compiler/runtime bootstrap targets portable C++20 because of the pre-approved libcurl-backed networking layer and current backend integration. JSONIC itself remains independently verified C++17-compatible; do not regress that compatibility.

## Embedded third-party dependency policy


## Approved third-party foundations

Only JSONIC, libcurl, and OpenSSL are pre-approved for vendoring/embedding in Strut itself. Prefer the standard library, operating-system APIs, and Strut-owned code otherwise; any additional embedded third-party dependency must be approved first. SQLite may be consumed as an external/system library by the official package without being vendored into the compiler/runtime.

## Performance/profiling workflow

The compiler supports `--timings` for coarse frontend/native-backend phase timing and `--emit-cpp <path>` to emit the bootstrap C++ without invoking the native compiler. The sibling `strut-benchmarks` repository contains reproducible compile, incremental-build, perf, assembly and runtime comparison tooling. Preserve raw before/after results when optimising.

## Standard-library collections/filesystem pass (CP1-CP10)

- Standard modules added: `vector`, `deque`, `list`, `map`, `set`, `ordered_map`, `ordered_set`, `queue`, `stack`, `priority_queue`, `filesystem` via `include <name>;`.
- CLI compilation enforces explicit standard-module includes. Direct internal parser/sema unit construction remains compatibility-friendly for existing tests.
- `map<K,V>` now lowers to `std::unordered_map`; `set<T>` to `std::unordered_set`; ordered variants lower to `std::map`/`std::set`.
- `vector<T>` is a named spelling; `T[]` remains shorthand.
- Queue/stack/priority queue (`priority_queue`) native mappings and basic methods are supported.
- Minimal codegen is feature-granular for numeric collection programs and emits only the corresponding STL headers.
- Filesystem calls require `<filesystem>` in CLI compilation.
- Bulk `copy(vector,dest)`, `move(vector,dest)`, and `remove(vector)` are implemented. Bulk copy/move require an existing destination directory.
- Rich CLI diagnostics now include an ANSI syntax-highlighted source line plus caret/range.
- See `STANDARD_LIBRARY.md` for current public semantics.

- CP11-20: added deque/list, renamed prique to priority_queue with `priority_queue<T,min>`, safe remove/remove_all split, filesystem predicates/metadata/traversal/path/cwd APIs, optimized whole-file text/bytes reads and writes, and optimized compiler source loading.
- CP21-32 pass: added first-class `tuple<T...>` / `(x,...)` literals including `(x,)`, compile-time tuple indexing, `include <tuple>`, wildcard filesystem source operands (`*`, `?`, `**`), expanded stdlib regression/profiling tools, module/codegen size profiling, and LSP stdlib completion entries.
