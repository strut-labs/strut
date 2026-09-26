# CP21-32 completion summary

## CP21 Filesystem regressions
Expanded stdlib regressions cover bulk vector operations, wildcard `*`/`?`/`**` source operands, metadata/path APIs, whole-file I/O, and recursive/non-recursive deletion.

## CP22 Collection benchmarks
Added equivalent Strut/C++/Rust/Go benchmark sources for hash map, ordered map, hash set, queue, stack and priority queue; vector coverage already existed. Go's ordered-map case is explicitly documented as the nearest built-in map baseline rather than an ordered-container equivalent.

## CP23 File I/O benchmarks
Added `tools/profile_file_io.py` for 1 KiB/1 MiB/100 MiB cached whole-file reads across available Strut/C++/Rust/Go toolchains.

## CP24 Filesystem benchmarks
Added `tools/profile_filesystem.py` for controlled recursive traversal with environment caveats recorded in output.

## CP25 Module compile benchmarks
Added `tools/profile_modules.py` to record per-module compile time and emitted C++ size.

## CP26 Generated-code size gates
Added `tools/check_codegen_budget.py` as a broad accidental-runtime-bloat tripwire and wired it into `profile.sh`.

## CP27 Multi-module/incremental tests
Added local-include/stdlib aggregation coverage and an incremental isolation regression that proves touching a shared dependency only rebuilds the dependent object.

## CP28 Compatibility/migration
Documented `map`/`set` hash semantics, ordered variants, angle-bracket module includes, and the `prique` -> `priority_queue` migration. Old `prique<T>` now receives a Strut semantic diagnostic.

## CP29 Formatter/LSP
LSP completion includes stdlib collection/filesystem facilities. Formatter normalizes standard generic type spacing without rewriting unrelated commas.

## CP30 Documentation
Expanded `STANDARD_LIBRARY.md` with tuple syntax, include semantics, wildcard behavior, and complexity guarantees.

## CP31 Certification
13/13 compiler CTests pass. Focused stdlib regression set passes. The entire independent suite was attempted in this environment but exceeded the execution timeout, so a full-suite green claim is intentionally not made here.

## CP32 Convenience review
Requested conveniences accepted: tuple literals/indexing and filesystem wildcards. Additional convenience syntax is deferred until further dogfooding rather than expanding the surface speculatively.

## Additional requested work
- `tuple<T...>` added with literals such as `(2.0, 1)` and one-element `(7,)`.
- Tuple access uses compile-time literal indexing: `t[0]`.
- `include <tuple>;` is required.
- Filesystem wildcard source operands support `*`, `?`, and recursive `**`; destinations are never wildcard-expanded.
