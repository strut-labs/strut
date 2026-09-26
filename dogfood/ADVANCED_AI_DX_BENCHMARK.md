# Advanced AI-DX benchmark

This is the controlled follow-up to `AI_DX_BENCHMARK.md`. The preserved blind
baseline used compiler `eaa4418` and only public documentation, CLI help,
diagnostics, and task-owned files. Compiler source, generated C++, regression
fixtures, and previous solutions were excluded. The post-fix run additionally
used the public `strut api --json` index.

The twelve tasks cover a multi-module HTTP server, SQLite-backed routes, JSON
request/response handling, a TLS client, nested generics, empty generic literals,
pointer containers, async filesystem/process/network composition, multi-module
checked errors, local project discovery, fresh project initialization, and an
isolated missing-package-cache failure. Network-facing programs are compile-only;
the remaining programs and CLI workflows have exact outcome checks.

## Results

| Metric | Before | After |
| --- | ---: | ---: |
| First-attempt compile/workflow rate | 7/12 (58.3%) | 12/12 (100%) |
| First-attempt correct-run/outcome rate | 6/12 (50.0%) | 12/12 (100%) |
| Median correction cycles | 0.5 | 0 |
| Total correction cycles | 6 | 0 |
| Documentation consultations | 12 | 4 |
| CLI-help consultations | 2 | 1 |
| Machine-readable help consultations | unavailable | 1 index for all tasks |
| Compiler-source inspections | 0 | 0 |
| Environment/setup failures | 0 | 0 |

Native libraries were present in this environment, so success does not imply
they are universally installed. Native-failure guidance is separately covered by
unit tests and now gives reliable platform suggestions for libcurl and SQLite.

## Baseline findings

| Task | Category | Finding |
| --- | --- | --- |
| SQLite service | Compiler/runtime bug | A SQLite handle captured by an HTTP route became const, but `query`/`exec` were not const-safe. |
| Nested generics | Code-generation bug | `map[key] = value` emitted bounds-checked `.at()` and crashed instead of inserting. |
| Empty literal | Diagnostic gap | `identity([])` leaked native C++ template deduction output instead of explaining missing element evidence. |
| Async backend | API discovery gap | Mixed string/integer concatenation required explicit `to_string`, but the composition itself worked. |
| Multi-module errors | Documentation gap | The docs did not say that user-declared custom error types are not implemented. |
| Project initialization | CLI/project gap | `strut init` created build configuration but not the manifest required by package commands. |

The isolated missing-cache task already produced a direct package-and-requirement
diagnostic. TLS client compilation also correctly selected libcurl. Server-side
TLS remains explicitly unsupported rather than being represented as a working API.

## Implemented improvements

- SQLite `query` and `exec` can be used from captured route state.
- Map index assignment uses insertion semantics while reads remain checked.
- Empty arrays in inference-only generic positions receive a source-level error
  and explicit-type help.
- `strut init` creates both build configuration and a minimal manifest.
- `strut project [--json]` exposes root, configuration, manifest, and cache paths.
- `strut api --json` exposes built-in signatures, methods, checked errors,
  modules, runtime components, native dependencies, CLI commands, and key syntax
  notes in a versioned schema.
- Native dependency failures provide conservative platform-specific package
  guidance where the host platform is known.
- Public docs now cover API discovery, project/cache inspection, empty generic
  literals, the current custom-error boundary, and a certified SQLite HTTP route.

Reproduce both runs with:

```sh
python3 dogfood/benchmark.py /path/to/eaa4418/strut dogfood/attempts/0.0.1-advanced-blind
python3 dogfood/benchmark.py build/strut dogfood/attempts/0.0.1-advanced-after
```
