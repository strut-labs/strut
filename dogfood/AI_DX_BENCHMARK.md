# Controlled AI-DX benchmark

This benchmark follows `FIRST_ATTEMPT.md`. The baseline was authored against
compiler `e5fc2c1` and website source `d6dceba` using only `strut --help`, public
documentation, compiler diagnostics and task-owned files. Compiler source,
generated C++, regression fixtures, old dogfood solutions and prior solutions were
excluded until the HTTP task reached a recorded blocked state.

The twelve tasks cover CLI arguments, JSON transformation, recursive filesystem
work, process execution, HTTP, channels and threads, async, SQLite, local multi-file
composition, pointers, generics/containers and checked errors. HTTP is compile-only
to keep the benchmark deterministic; every other task executes with exact output
and exit-status checks.

## Results

| Metric | Before | After |
| --- | ---: | ---: |
| First-attempt compile rate | 8/12 (66.7%) | 12/12 (100%) |
| First-attempt correct-run rate | 8/12 (66.7%) | 12/12 (100%) |
| Median correction cycles | 0 | 0 |
| Total correction cycles | 5 | 0 |
| Tasks recovered from diagnostics without implementation source | 3 | 0 needed |
| Tasks requiring docs | 12 | 12 |
| Tasks requiring compiler-source inspection to resolve | 1 | 0 |

The median remains zero because most tasks already succeeded immediately; total
cycles captures the improvement among failures more clearly. Exact source,
compiler output and per-task metadata live under `dogfood/attempts/`.

## Baseline failures

| Task | Cycles | Category | Finding |
| --- | ---: | --- | --- |
| Process execution | 1 | Documentation gap | The example omitted the required `ExecError` contract; the compiler diagnostic was sufficient. |
| HTTP JSON | 2 | Compiler bug + documentation gap | A JSON-returning builtin lost its type in IR, generated `.at()`, and leaked C++; package docs then incorrectly suggested `include <http>`. |
| SQLite | 1 | Documentation gap | The page omitted the checked `SqliteError` spelling; the compiler diagnostic was sufficient. |
| Generic array call | 1 | Diagnostic/codegen gap | An array literal passed to a generic call became an untyped C++ initializer list and leaked template deduction output. |

The pointer task compiled, but public pages disagreed between `ptr<T>` and
`raw_ptr<T>`. The terminology was standardized even though it did not cost a
compile cycle. The post-fix clean run also exposed `await async_call()` as a native
reference-binding failure; accepting temporary futures removed an unnecessary
extra-variable requirement.

## Implemented improvements

- Builtin return types such as `http_get_json` now survive IR lowering.
- Array literals used as call arguments carry a concrete vector type into C++.
- `await` accepts both named futures and direct async-call results.
- Python/Rust-style `for (x in xs)` remains invalid, but now directly recommends
  canonical `for (x : xs)` syntax.
- `strut --help` now exposes entry-point forms, compile/run workflow, range-loop
  syntax, checked-error placement and the public docs URL.
- HTTP, SQLite, process, core-array and raw-pointer documentation now reflects the
  implemented contracts without false includes or stale terminology.

Reproduce either run with:

```sh
python3 dogfood/benchmark.py build/strut dogfood/attempts/0.0.1-e5fc2c1-blind
python3 dogfood/benchmark.py build/strut dogfood/attempts/0.0.1-ai-dx-after
```
