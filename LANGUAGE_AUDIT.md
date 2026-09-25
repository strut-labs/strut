# CP112 — Language design audit

The language handover has been reviewed against the implemented compiler rather than the early design discussion.

## Frozen core spellings

- inferred declaration `name := value;`, typed declaration `T name := value;`, assignment `name = value;`;
- mutable by default, explicit `const`;
- mandatory semicolons;
- full `function` and `operator` keywords;
- generic declarations use `[T]`, instantiated types use `<T>`;
- `.p` source and optional `.h` declaration/header files;
- `T[]` dynamic arrays and `T[n]` fixed arrays;
- keyed `[...]` map literals and `{...}` JSON literals;
- `ptr<T>`, `ref<T>`, `weak_ptr<T>`, and unsafe `raw_ptr<T>`; no tracing GC;
- checked errors use `-> R : E` or `-> R : (E1, E2)` plus `throw`/`try`/`catch`;
- local `include "file.h";` and package `include <package>;`;
- fixed language-defined operator set and precedence; no parser-extension/custom-precedence mechanism;
- one data-bearing base plus abstract contract bases;
- `in`, `out`, `err`, streams, argv-safe process APIs, and explicit shell execution;
- static, dynamic, and mixed native linking are first-class.

## Removed/deferred rather than frozen

- server-side Nift templating;
- arbitrary operator spelling/precedence;
- generic constraint syntax without a demonstrated need;
- algebraic/payload enums beyond the current enum model;
- self-hosting as a release requirement.

## Audit result

No core syntax needs redesign before broader dogfooding. Remaining changes should be driven by real programs, profiling, cross-platform certification, compatibility policy, or concrete safety issues rather than aesthetic churn.
