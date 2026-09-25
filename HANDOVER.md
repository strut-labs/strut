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
2. `IMPLEMENTATION_HANDOVER.md` — ordered checkpoint roadmap;
3. `REGRESSION_HANDOVER.md` — regression-suite contract;
4. `WEBSITE_HANDOVER.md` — website/docs requirements and maintenance contract.

When a design decision changes, update these documents before or alongside implementation so later work does not silently revive superseded syntax.

## Current status

The compiler implementation has not started. The first checkpoints are intentionally project/bootstrap work: lock the initial design contract, establish builds/tests, establish the independent regression suite, and turn the barebones website into the public Strut documentation site before large amounts of implementation make documentation catch-up expensive.

## Major acceptance target

The first major usefulness milestone is not a toy expression evaluator. Strut should be able to build a production-ish HTTP service that uses JSON, async/concurrency, an official SQLite package, and embedded frontend assets, and compile the entire application into one native executable.

A later major dogfood target is to investigate and, if practical, rewrite Nift in Strut.

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

The compiler/runtime bootstrap targets portable C++17. JSONIC is verified header-only C++17. libcurl does not require Strut itself to use C++20. Do not raise the bootstrap language standard without a demonstrated need.

## Embedded third-party dependency policy

Only JSONIC and libcurl are pre-approved for embedding in the Strut toolchain/runtime. Prefer the standard library and Strut-owned code otherwise. Do not vendor or embed any additional third-party library without discussing it with the maintainer first. Packages may depend on external system libraries later, but that is separate from silently embedding another dependency into Strut itself.
