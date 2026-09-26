# Strut 0.0.1

Strut 0.0.1 is the first packaged compiler release. It establishes the current
pre-1.0 language and toolchain baseline across Linux, macOS, and Windows.

## Highlights

- Executables use `function main() -> int` or
  `function main(string cmd, string[] args) -> int`; the returned value becomes
  the process exit status, `cmd` receives native `argv[0]`, and `args` contains
  only user-supplied arguments.
- Dynamic arrays (`T[]`) are a core language facility and no longer require
  `include <vector>;`; `vector<T>` remains available as a compatibility spelling.
- Filesystem `copy`, `move`, and `remove` operations accept compatible iterable
  path collections. Pairwise copy/move mappings require ordered collections and
  equal lengths.
- Safe owning pointers use `T* owner := new(...)`; raw pointer extraction is an
  explicit unsafe operation written `ptr<T> raw := ptr(owner)`.
- Typed and inferred lambdas compose with generic functions, containers, async
  work, and operator definitions.
- Nested generic types are parsed and canonicalized recursively, including safe
  pointer and reference types inside generic containers.
- Cross-feature runtime behavior is more reliable across collections, JSON,
  filesystem, processes, networking, concurrency, FFI, and SQLite workloads.
- Diagnostics, source spans, formatting, and documentation were tightened around
  the supported language contract.
- Regression cases run in isolation, preventing one failure from hiding or
  contaminating another.
- Generated C++ is certified with strict warnings under GCC, Clang/AppleClang,
  and MSVC, with dogfood programs exercised on Linux, macOS, and Windows.

Strut remains pre-1.0. See `COMPATIBILITY.md` for the compatibility policy and
`BUILDING.md` for source and installed-layout instructions.

## Development after 0.0.1

- Generic calls now use destination, assignment, return, parameter, and nested
  `TypeId` context, including empty literals, nullable values, pointers, and
  recursively nested containers.
- Applications can declare nominal structured checked errors with
  `error Name { ... }`; contracts, throws, typed catches, generic functions,
  async functions, modules, diagnostics, and editor metadata preserve identity.
- `atomic<int>` and `atomic<bool>` provide sequentially consistent load/store,
  exchange, compare-exchange, and integer fetch-add/fetch-sub operations.
- Executable documentation now certifies every enabled operator-overload family.
