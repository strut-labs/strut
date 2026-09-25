# CP111 — Self-hosting feasibility review

## Current compiler shape

The bootstrap compiler is deliberately small enough to understand: the current `src/` plus public compiler headers are roughly 3.8k source lines across 33 C++/header files, excluding tests, JSONIC, generated/runtime support and documentation.

Its major subsystems are lexer, parser, source/diagnostics, semantic analysis, types/operators, typed IR, formatter, package/project build logic, C++ code generation, LSP and CLI orchestration.

## What Strut would need to self-host

A Strut compiler rewrite needs more than syntax parity. It needs a comfortable implementation story for:

- byte/string scanning and source spans;
- maps, arrays and graph-shaped compiler data;
- filesystem traversal and incremental metadata;
- deterministic diagnostics;
- JSON/package metadata;
- process/toolchain execution;
- formatter and LSP JSON-RPC workloads;
- native library and cross-platform path handling;
- efficient ownership patterns for AST/IR graphs;
- profiling-quality performance on large source inputs.

Most primitives now exist, but their ergonomics and performance have not yet been demonstrated on a compiler-sized Strut program.

## Backend consideration

The present bootstrap lowers typed Strut IR to C++ and invokes a host C++ toolchain. Rewriting the frontend in Strut would therefore not immediately remove the C++ toolchain dependency. A truly independent self-host would require either a direct native backend or another stable code-generation target.

That distinction matters: "compiler frontend written in Strut" and "Strut can bootstrap itself without a C++ compiler" are different milestones.

## Prototype decision

No additional self-hosting prototype is justified at this checkpoint. CP110's Nift metadata prototype and CP106/CP107 dogfood already expose more actionable language/runtime issues than a toy compiler-in-Strut would. A shallow parser rewrite solely to claim self-hosting would distort priorities without proving production readiness.

## Decision

Self-hosting is **feasible in principle but not currently a project goal**. Reconsider it after larger dogfood, profiler-driven optimisation and platform certification. Do not alter Strut syntax or runtime design merely to make self-hosting easier.
