# Strut Implementation Handover — Checkpoint Game Plan

This is the ordered implementation roadmap. Checkpoints are intentionally concrete and should be marked complete only when code, tests, regressions, docs, and relevant commits are done.

Do not treat later checkpoint numbering as a reason to preserve a bad early decision. If implementation exposes a design problem, update the design docs and roadmap deliberately.

Current implementation progress: **CP0–CP97 and CP101 complete; CP98–CP100 await platform certification; CP102 is next for local work.**

## Phase 0 — Project contract and repo foundations

### CP0 — Freeze the initial written design baseline
- [x] Review `HANDOVER.md`.
- [x] Review `LANGUAGE_HANDOVER.md` against the latest design discussion.
- [x] Resolve any immediately blocking syntax contradictions before compiler code starts.
- [x] Record remaining provisional decisions explicitly rather than guessing during implementation.
- [x] Commit the handover baseline in `strut/`.

### CP1 — Choose bootstrap implementation/toolchain
- [x] Choose the implementation language/toolchain for the first Strut compiler/runtime.
- [x] Document compiler/frontend/backend boundaries.
- [x] Choose the initial native code-generation strategy/backend.
- [x] Confirm Linux-first developer workflow while preserving portable architecture from day one: no POSIX-only assumptions in compiler core, runtime abstractions for OS services, and Windows/macOS build paths kept viable.
- [x] Add reproducible build instructions.

### CP2 — Establish `strut/` repository structure
- [x] Add compiler/runtime/stdlib/test directories.
- [x] Add build configuration.
- [x] Add formatter/lint rules for compiler source where appropriate.
- [x] Add a minimal compiler executable with `--help` and `--version`.
- [x] Add unit-test harness.
- [x] Commit a clean bootstrap checkpoint.

## Phase 1 — Establish independent regression suite immediately

### CP3 — Bootstrap `strut-regression-suite/`
- [x] Add README and suite handover/readme describing its independence from compiler tests.
- [x] Add fixture directory structure.
- [x] Add a harness capable of accepting/locating a local Strut compiler binary.
- [x] Add initial placeholder/smoke cases.
- [x] Commit in the regression-suite repository.

### CP4 — Certify the regression harness itself
- [x] Assert successful compile exit status.
- [x] Assert program stdout/stderr/exit status.
- [x] Assert expected compiler failure.
- [x] Assert diagnostic text and, where practical, source locations.
- [x] Ensure a deliberately wrong expectation makes the suite fail.
- [x] Keep the harness dependency-light and agent-readable.

## Phase 2 — Build the public website/docs early

### CP5 — Turn the barebones Nift site into the Strut website shell
- [x] Inspect existing Nift config/tracking/template structure.
- [x] Create a distinctive minimalist dark-mode visual system similar in discipline to `nift.dev` without cloning it.
- [x] Avoid a blue-dominated palette.
- [x] Add header/navigation and responsive layout.
- [x] Add a hamburger on mobile.
- [x] Make the hamburger show/hide a full-screen docs/navigation menu.
- [x] Ensure menu state/scroll behaviour works correctly on mobile.
- [x] Add a Strut favicon/brand asset.
- [x] Run `nift build` and fix all errors.
- [x] Commit website changes.

### CP6 — Establish multi-page docs architecture
- [x] Add landing/about/install/getting-started pages.
- [x] Add initial language/docs sections for syntax, types, functions, collections, JSON, structs, memory, errors, concurrency, async, packages, CLI, examples.
- [x] Wire internal navigation using Nift project-aware linking.
- [x] Add previous/next or equivalent docs navigation if useful.
- [x] Ensure desktop and mobile docs navigation scale beyond a handful of pages.
- [x] Build and inspect generated output.
- [x] Commit website changes.

### CP7 — Add branded 404 and docs maintenance contract
- [x] Add a custom dark 404 page.
- [x] Keep 404 centered/minimal with no normal header/footer, similar in spirit to `nift.dev`.
- [x] Add a clear path back to useful content.
- [x] Confirm favicon/theme consistency.
- [x] Document implemented vs planned language features clearly.
- [x] Commit website changes.

## Phase 3 — Lexer, parser, AST, diagnostics

### CP8 — Source files and lexical skeleton
- [x] Support provisional `.p` Strut source files.
- [x] Support optional `.h` declaration/header files semantically, without C preprocessor text substitution.
- [x] Implement source locations/spans from day one.
- [x] Implement comments, whitespace, identifiers, keywords, punctuation.
- [x] Add unit tests and independent regressions.

### CP9 — Literals and primitive tokens
- [x] Integer literals.
- [x] Floating literals.
- [x] String literals and escapes.
- [x] Boolean literals.
- [x] `null`.
- [x] Error diagnostics for malformed literals.
- [x] Tests + regression fixtures.

### CP10 — Declaration and assignment grammar
- [x] `x := value;`.
- [x] `Type x := value;`.
- [x] `x = value;`.
- [x] `const x := value;` and typed const form.
- [x] Mandatory semicolons.
- [x] Reject duplicate/ambiguous C/C++-style declarator spellings.
- [x] Tests + regressions + syntax docs update.

### CP11 — Expressions and operators
- [x] Arithmetic.
- [x] comparison/equality.
- [x] logical operators.
- [x] precedence/associativity.
- [x] member/index access.
- [x] increment/decrement if retained after design review.
- [x] compound assignment if retained.
- [x] Tests + regressions.

### CP12 — Control-flow parser
- [x] `if` / `else`.
- [x] `while`.
- [x] C-style `for`.
- [x] Nift-style `for (item : items)`.
- [x] `break` / `continue`.
- [x] block scopes.
- [x] Tests + regressions + docs.

### CP13 — Function parser
- [x] `function name(args) -> Type { ... }`.
- [x] explicit `-> void`.
- [x] `return value;`.
- [x] bare `return;` for `void`.
- [x] function declarations without bodies.
- [x] `function Struct::method(...) -> Type { ... }` out-of-struct definitions.
- [x] generic declaration form `function name[T](...) -> Type`.
- [x] require generic/template parameter identifiers to use uppercase identifiers.
- [x] Tests + regressions.

### CP14 — Lambda/function-type parser
- [x] `(x) => expr`.
- [x] `(x, y) => { ... }`.
- [x] `async (...) => ...` syntax parsing.
- [x] `function<(A, B) -> R>` function types.
- [x] `function<() -> void>` zero-argument form.
- [x] `function[T]<(T, T) -> T>` explicit generic function-value type form.
- [x] infer undeclared uppercase template identifiers from typed lambda parameters; reject misspelled lowercase/mixed-case types instead of treating them as templates.
- [x] Tests + regressions.

### CP15 — Diagnostic foundation
- [x] Stable error reporting format.
- [x] file/line/column spans.
- [x] useful parser recovery where practical.
- [x] no cascades of nonsense after one obvious parse failure.
- [x] snapshot/select diagnostics in the regression suite.

## Phase 4 — Semantic analysis, types, aliases, first codegen

### CP16 — Symbol tables and scopes
- [x] lexical scopes.
- [x] symbol declarations/lookups.
- [x] duplicate definitions.
- [x] shadowing policy.
- [x] function/struct namespaces.
- [x] Tests + regressions.

### CP17 — Numeric type family
- [x] implement `int_8/int_16/int_32/int_64`.
- [x] implement `uint_8/uint_16/uint_32/uint_64`.
- [x] implement `double_32/double_64`.
- [x] define literal inference/conversion rules.
- [x] define overflow/narrowing diagnostics.
- [x] Tests + regressions.

### CP18 — Type aliases
- [x] settle alias declaration syntax.
- [x] implement general aliases.
- [x] define `int := int_32`, `uint := uint_32`, `double := double_32` as ordinary aliases in the language/runtime definition.
- [x] support aliases of compound/function types.
- [x] cycle/invalid-alias diagnostics.
- [x] Docs + regressions.

### CP19 — Core semantic typing
- [x] `bool`, numeric types, `string`, `void`, `null` foundations.
- [x] type inference for `:=`.
- [x] explicit annotation checking.
- [x] assignment compatibility.
- [x] const enforcement.
- [x] Tests + regressions.

### CP20 — Typed IR
- [x] define stable typed IR distinct from parser AST.
- [x] lower declarations/expressions/control flow/functions.
- [x] preserve source mapping for diagnostics/debugging.
- [x] document backend contract.

### CP21 — First native code generation
- [x] compile integer/string hello-world style programs.
- [x] function calls and returns.
- [x] basic control flow.
- [x] produce a native executable.
- [x] verify compiler tests and independent regressions.

### CP22 — Initial CLI compile workflow
- [x] support `strut file.p` as the preferred single-file compile form.
- [x] define deterministic default output naming.
- [x] add explicit output option.
- [x] decide whether `strut compile file.p` exists as an alias/explicit form.
- [x] do not add `strut run` by inertia.
- [x] docs + regressions.

## Phase 5 — Collections, strings, JSON, structs

### CP23 — Dynamic arrays `T[]`
- [x] runtime representation.
- [x] literals `[1, 2, 3]`.
- [x] indexing/bounds behaviour.
- [x] length/empty.
- [x] push/pop.
- [x] iteration.
- [x] owned runtime storage; `T*` reference-count integration is intentionally completed with the memory model in CP35.
- [x] tests + regressions.

### CP24 — Fixed arrays `T[n]`
- [x] fixed-size type semantics.
- [x] layout/storage rules.
- [x] initialization rules.
- [x] nested fixed/dynamic arrays.
- [x] bounds diagnostics/runtime safety.
- [x] tests + regressions.

### CP25 — Strings
- [x] owned string representation.
- [x] UTF-8 policy.
- [x] indexing/slicing policy.
- [x] starts_with/ends_with/contains/trim/replace/split/join/substr or equivalent curated API.
- [x] formatting/conversion basics.
- [x] tests + regressions.

### CP26 — Maps
- [x] `map<K, V>`.
- [x] keyed `[...]` literal syntax.
- [x] lookup/insert/remove/contains.
- [x] deterministic iteration policy documented.
- [x] equality semantics.
- [x] tests + regressions.

### CP27 — Built-in JSON type
- [x] JSON value representation.
- [x] `{...}` literal syntax always means JSON/object data rather than map inference.
- [x] nested array/object values.
- [x] indexing/navigation.
- [x] equality.
- [x] tests + regressions.

### CP28 — JSON stdlib
- [x] `json.parse`.
- [x] `json.stringify`.
- [x] pretty output.
- [x] typed encode/decode direction fixed: `json.encode(value)` is available now; typed `json.decode<T>` is reserved for the post-struct generic conversion work.
- [x] useful parse/stringify diagnostics now include JSONIC line/column data; checked-error integration will adopt the Strut error model when that phase lands.
- [x] tests + regressions + docs.

### CP29 — Structs
- [x] fields.
- [x] `Type { field: value }` construction.
- [x] inline methods.
- [x] out-of-struct definitions.
- [x] visibility/access policy.
- [x] layout rules.
- [x] tests + regressions.

### CP30 — Nullability
- [x] `T?`.
- [x] nullable assignment rules.
- [x] `?.`.
- [x] `??`.
- [x] flow narrowing after null checks.
- [x] safe dereference diagnostics.
- [x] tests + regressions.

## Phase 6 — First-class functions and collection ergonomics

### CP31 — First-class named functions
- [x] assign functions to variables.
- [x] pass functions as arguments.
- [x] return functions where safe.
- [x] function identity semantics.
- [x] tests + regressions.

### CP32 — Lambdas and closures
- [x] inferred lambda parameter/return types where context permits.
- [x] explicit function-type assignments.
- [x] capture semantics.
- [x] escaping closure lifetime safety.
- [x] mutable capture semantics documented.
- [x] tests + regressions.

### CP33 — Higher-order collection functions
- [x] `map`.
- [x] `filter`.
- [x] `reduce`.
- [x] `any` / `all`.
- [x] `find` / `count`.
- [x] `sort` callback.
- [x] tests + regressions.

### CP34 — Extended collection helpers
- [x] `count_by`.
- [x] `index_by` with duplicate-key policy.
- [x] `partition`.
- [x] `pick` / `omit` where applicable.
- [x] `merge_deep` with conservative explicit semantics.
- [x] tests + regressions.

## Phase 7 — Safe memory model, reference counting, weak refs, unsafe

### CP35 — Prototype `T*` runtime ownership
- [x] implement safe reference-counted owning pointer prototype.
- [x] copy increments count.
- [x] release decrements count.
- [x] zero count destroys deterministically.
- [x] define nullability interaction.
- [x] benchmark baseline overhead before optimizing.

### CP36 — Const pointer semantics
- [x] `T*`.
- [x] `T* const`.
- [x] `const T*`.
- [x] `const T* const`.
- [x] enforce referent vs binding const independently.
- [x] tests + regressions.

### CP37 — `T&` safe borrows
- [x] `T&` non-null/non-owning.
- [x] `T& const`.
- [x] reference binding is non-reassignable.
- [x] establish lifetime validation sufficient to prevent dangling refs.
- [x] pass-by-ref without refcount churn.
- [x] tests + compile-fail regressions.

### CP38 — `weak_ptr<T>` and cycle strategy
- [x] weak-control-block/runtime mechanics.
- [x] safe upgrade/lock semantics.
- [x] destruction behaviour.
- [x] obvious reference-cycle warning prototype where practical.
- [x] parent/child back-reference fixtures.
- [x] tests + regressions.

### CP39 — `unsafe` and `ptr<T>`
- [x] `unsafe { ... }`.
- [x] `ptr<T>` creation/use restrictions.
- [x] pointer arithmetic policy.
- [x] conversion rules between safe/weak/raw pointers.
- [x] compiler prevents raw operations outside unsafe contexts.
- [x] tests + regressions.

### CP40 — Memory-safety certification pass 1
- [x] use-after-free attempts.
- [x] dangling `T&` attempts.
- [x] invalid weak upgrades.
- [x] double-destruction attempts.
- [x] null safe-pointer cases.
- [x] iterator/reference invalidation cases.
- [x] sanitizers/Valgrind or equivalent on compiler/runtime tests.
- [x] document what safe Strut guarantees and what `unsafe` opts out of.

## Phase 8 — Includes/modules, structs-as-contracts, generics, enums, errors

### CP41 — Local/module include semantics
- [x] `include "foo.h"` / local dependency resolution.
- [x] one semantic load, no textual macro preprocessor behaviour.
- [x] include cycles/duplicate inclusion handling.
- [x] file/module symbol boundaries.
- [x] tests + regressions.

### CP42 — Package include syntax
- [x] `include <package>` grammar/semantic placeholder.
- [x] package namespace rules.
- [x] distinguish official/third-party/local packages without making source verbose.
- [x] tests + docs.

### CP43 — Abstract struct contracts
- [x] permit structs with unimplemented function declarations.
- [x] such structs cannot be instantiated while requirements remain unsatisfied.
- [x] out-of-struct definitions can satisfy required methods.
- [x] diagnostics list unsatisfied required methods.
- [x] tests + regressions.

### CP44 — Inheritance and multiple contracts
- [x] `struct User : Serializable, Printable` syntax.
- [x] settle concrete-base/data inheritance rule.
- [x] explicitly avoid accidental C++ diamond/layout complexity unless deliberately supported.
- [x] method resolution/conflict diagnostics.
- [x] tests + regressions.

### CP45 — Enums with explicit values
- [x] implicit enum values.
- [x] `name = integer` explicit values.
- [x] continuation after explicit values.
- [x] underlying representation policy.
- [x] tests + regressions.

### CP46 — `switch`
- [x] `case` / `default`.
- [x] enum/integer/string policy as appropriate.
- [x] fallthrough policy explicitly chosen; do not inherit C accidentally.
- [x] exhaustiveness diagnostics where useful.
- [x] tests + regressions.

### CP47 — `match`
- [x] basic enum matching.
- [x] wildcard/default pattern.
- [x] payload/destructuring is deferred until payload-carrying enum/data variants exist.
- [x] exhaustiveness checking where possible.
- [x] tests + regressions.

### CP48 — Generics/templates
- [x] square-bracket declaration parameters for functions/structs; operator generics are completed with operator declarations in CP51–CP53.
- [x] angle-bracket instantiated types: `Box<int>`, `T*`, etc.
- [x] generic functions and structs.
- [x] uppercase template identifier rule.
- [x] implicit generic lambda inference from typed uppercase parameters.
- [x] no constraint syntax until real use cases justify it.
- [x] bootstrap backend uses host C++ templates for predictable instantiation; direct backend may monomorphise later without changing source syntax.
- [x] diagnostics that remain readable.
- [x] tests + regressions + docs.

### CP49 — Checked error signatures
- [x] `-> X : Error`.
- [x] `-> X : (E1, E2, ...)`.
- [x] `throw Error(...)`.
- [x] validate declared throw set.
- [x] diagnostics for undeclared checked throws.
- [x] tests + regressions.

### CP50 — Try/catch
- [x] `try { ... }`.
- [x] typed `catch (Error err)`.
- [x] catch-all `catch { ... }`.
- [x] propagation behaviour/shorthand prototyped and deliberately chosen.
- [x] interaction with destructors/resource cleanup certified.
- [x] tests + regressions.

## Phase 9 — Operator overloading

### CP51 — Fixed operator table and overload resolution
- [x] define Strut's fixed operator spellings, precedence and fixity in one canonical compiler table.
- [x] do not permit arbitrary new punctuation/text operators.
- [x] do not permit user-defined precedence or associativity.
- [x] distinguish prefix/infix/postfix forms where the language already defines them.
- [x] define overload candidate lookup and ambiguity diagnostics.
- [x] tests + regressions.

### CP52 — Normal operator declarations
- [x] `operator +(A a, B b) -> R { ... }`.
- [x] generic `operator[T] ...` declarations.
- [x] overload arithmetic/comparison/stream operators from the approved set.
- [x] support prefix `*` dereference for `T*` and make it overloadable.
- [x] add indexing/call operators only where semantics remain clear.
- [x] explicitly reserve structural operators such as member access unless later justified.
- [x] tests + docs.

### CP53 — Lambda operator declarations
- [x] `operator<(A, B) -> R> + := (a, b) => ...;`.
- [x] `operator[T]<(A<T>, A<T>) -> R<T>> + := ...;`.
- [x] use exactly the same callable signature grammar as lambda function declarations.
- [x] accept visually repetitive operator tokens (`:= :=`, etc.) rather than adding special-case grammar.
- [x] tests + formatter/regression fixtures.

### CP54 — Assignment and initialization overloading
- [x] overload `=` for existing destination values.
- [x] overload `:=` for typed construction/initialization into new destination storage.
- [x] model `:=` destination as construction storage, not an existing `T&`.
- [x] inferred `x := value` must retain predictable type inference and must not let overloads invent an unrelated lhs type.
- [x] define copy/conversion initialization interaction deliberately.
- [x] tests + negative diagnostics.

## Phase 10 — Core IO/runtime facilities

### CP55 — Filesystem basics
- [x] paths.
- [x] exists/create/remove/copy/move.
- [x] directories/listing.
- [x] typed errors.
- [x] cross-platform abstractions.
- [x] tests + regressions.

### CP56 — Stream type foundation
- [x] implement `istream`, `ostream`, `sstream`, `ifstream`, and `ofstream`.
- [x] constructors/open/close and deterministic resource cleanup.
- [x] text/binary read/write foundations.
- [x] buffering and error semantics.
- [x] tests + regressions.

### CP57 — Standard console streams and stream syntax
- [x] `in` standard input stream.
- [x] `out` standard output stream.
- [x] `err` standard error stream.
- [x] `ofstream ofs(path);` style construction.
- [x] `<<` insertion and `>>` extraction using ordinary operator dispatch.
- [x] `endl` or deliberately chosen equivalent.
- [x] retain `print(...)`, `input()`, and `input(value)` conveniences.
- [x] user-defined stream insertion/extraction overload fixtures.
- [x] tests + regressions + docs.

### CP58 — Time/environment foundation
- [x] environment access.
- [x] clocks/durations.
- [x] platform abstractions and typed errors.
- [x] tests + docs.

### CP59 — Standard-library `exec`
- [x] argv-based `exec(program, args)` without requiring shell interpolation.
- [x] captured stdout/stderr and exit code.
- [x] inherited stdio mode.
- [x] cwd and environment overrides.
- [x] shell execution, if provided, must be an explicit opt-in path rather than the default `exec` behaviour.
- [x] typed launch/wait errors.
- [x] regression fixtures using small deterministic helper executables.
- [x] docs.

### CP60 — Child-process/pipe API
- [x] spawn long-lived child processes.
- [x] stream child stdin/stdout/stderr.
- [x] wait/status/terminate semantics.
- [x] pipe composition without requiring a shell.
- [x] interaction with async/blocking IO documented.
- [x] tests + regressions.

## Phase 11 — Threads, mutexes, channels, async

### CP61 — Native threads
- [x] `thread(function, args...)`.
- [x] lambda thread form.
- [x] `join` and lifecycle semantics.
- [x] return/error propagation policy.
- [x] safe pointer/ref behaviour across threads.
- [x] tests + stress regressions.

### CP62 — Mutex
- [x] `mutex` type.
- [x] explicit `lock()` / `unlock()`.
- [x] scoped `mtx.lock(() => { ... });`.
- [x] exception/error-safe unlocking.
- [x] recursive/non-recursive policy.
- [x] tests + regressions.

### CP63 — Channels/queues
- [x] typed channel primitive if design still justified.
- [x] send/receive/close semantics.
- [x] blocking/wakeup correctness.
- [x] tests + stress regressions.

### CP64 — Data-race safety policy
- [x] define what the compiler/runtime promises.
- [x] decide which sharing patterns require synchronization.
- [x] diagnostics where statically feasible.
- [x] document unsafe escape hatches honestly.

### CP65 — Async functions/futures
- [x] `async function`.
- [x] `await`.
- [x] future/task type representation.
- [x] async errors.
- [x] tests + regressions.

### CP66 — Async lambdas
- [x] `async (args) => expr`.
- [x] async block lambda.
- [x] capture/lifetime correctness.
- [x] tests + regressions.

### CP67 — Multithreaded async executor
- [x] implement real multithreaded executor.
- [x] work scheduling/wakeup.
- [x] blocking-operation policy.
- [x] interaction with explicit threads.
- [x] stress and race testing.
- [x] benchmark scheduler overhead.

## Phase 12 — FFI and package foundation

### CP68 — C ABI FFI
- [x] call C functions.
- [x] primitive/struct ABI mapping.
- [x] `ptr<T>` integration.
- [x] unsafe boundary rules.
- [x] callbacks if practical. (deferred: raw C function-pointer callback values are not yet exposed; ordinary C calls are complete)
- [x] tests against tiny C fixtures.

### CP69 — Native library linking model
- [x] support native static libraries and dynamic libraries through the FFI/build system.
- [x] Linux `.a`/`.so`, macOS `.a`/`.dylib`, Windows static/import library + `.dll` model as appropriate.
- [x] package/build metadata can request static, dynamic, or platform-default linking.
- [x] fail with clear diagnostics when requested link mode is unavailable.
- [x] tests against tiny native libraries in both modes.

### CP70 — Static, dynamic, and mixed project builds
- [x] fully static build mode where platform/dependencies permit.
- [x] ordinary dynamic build mode.
- [x] mixed per-dependency static/dynamic mode.
- [x] ensure the one-binary deployment path does not force all Strut programs to be statically linked.
- [x] document runtime-library/linker search-path behaviour.
- [x] release builds support symbol stripping and dead-code elimination regardless of link mode where the backend/toolchain permits.
- [x] regression/integration tests.

### CP71 — Package manifest/cache design
- [x] define project/package manifest format.
- [x] dependency version syntax.
- [x] lock/reproducibility strategy.
- [x] shared local cache.
- [x] deterministic resolution.
- [x] docs.

### CP72 — Dedicated `strut-packages` ecosystem contract
- [x] establish expected GitHub organisation/repository convention.
- [x] define official package quality/test/docs expectations.
- [x] package source/build metadata.
- [x] local package development workflow.
- [x] security/reproducibility expectations.

### CP73 — Package CLI
- [x] settle `strut add <package>` or equivalent.
- [x] install/update/remove/list workflow as needed.
- [x] package include resolution for `include <...>`.
- [x] lockfile updates are deterministic.
- [x] tests + docs.

## Phase 13 — Networking and official packages

### CP74 — Socket/networking substrate
- [x] decide minimal core vs official package boundary.
- [x] TCP client/server primitives required by higher layers.
- [x] async integration.
- [x] typed errors.
- [x] tests.

### CP75 — Official TLS package
- [x] choose dependency/implementation strategy: pre-approved libcurl for verified client TLS.
- [x] safe certificate verification defaults.
- [x] client TLS support required by the HTTP client; server TLS is explicitly deferred until an approved server-side TLS backend exists.
- [x] regression/integration tests for generated TLS support and verification policy.

### CP76 — Official HTTP package: client
- [x] requests/methods/headers/body.
- [x] JSON convenience using core JSON support.
- [x] async client.
- [x] redirects/timeouts/streaming policy.
- [x] tests + docs.

### CP77 — Official HTTP package: server
- [x] server/listen lifecycle.
- [x] routing/path params/query/headers.
- [x] request body text/bytes/JSON.
- [x] response text/bytes/JSON/HTML.
- [x] async handlers.
- [x] tests + docs.

### CP78 — Official SQLite package
- [x] create/use official SQLite package rather than bloating core stdlib.
- [x] connection/open/close.
- [x] prepared statements/parameters by default.
- [x] query/exec.
- [x] typed row mapping where feasible.
- [x] transactions.
- [x] async/blocking policy.
- [x] tests + docs.

## Phase 14 — Embedded resources and first serious web app

### CP79 — Embedded files/directories
- [x] compile file bytes into executable.
- [x] directory embedding.
- [x] content metadata/MIME helpers as appropriate.
- [x] deterministic builds.
- [x] tests.

### CP80 — Static asset serving helpers
- [x] official HTTP integration for embedded assets.
- [x] index/fallback support.
- [x] ETag/cache headers.
- [x] precompressed variants evaluated; deferred until content-encoding negotiation is part of the HTTP server API.
- [x] safe path handling.
- [x] tests.

### CP81 — Build the first production-ish one-binary app
- [x] HTTP server.
- [x] JSON API.
- [x] SQLite package.
- [x] async handlers.
- [x] embedded HTML/JS/CSS/assets.
- [x] compile to one native executable.
- [x] no runtime Node/static-server dependency.
- [x] add integration/regression fixture.
- [x] document on website.

## Phase 15 — Incremental object builds and project state

### CP82 — `strut init` and `.strut/config.json`
- [x] add `strut init` for creating project-local build metadata/config without overwriting existing project files.
- [x] define `.strut/config.json` schema for entrypoint, output, target, build mode, linking defaults, and incremental mode.
- [x] keep `strut.json` as package/project manifest; `.strut/config.json` is build-machine/project build state/configuration.
- [x] document which `.strut` files are source-controlled vs generated/ignored.
- [x] use JSONIC for config parsing.
- [x] tests + docs.

### CP83 — Persistent object directory
- [x] compile translation units to persistent native `.o`/`.obj` files under `.strut/obj/<target>/<mode>/`.
- [x] separate compilation from final linking.
- [x] cache generated backend source only when useful for diagnostics/debugging.
- [x] object cache keys include compiler version, target, build mode, relevant compiler flags, and linking ABI settings.
- [x] deleting `.strut/obj` must always be a safe clean rebuild path.
- [x] tests + docs.

### CP84 — Per-object dependency `.info.json` metadata
- [x] follow Nift's proven per-output metadata model, adapted for compilation units.
- [x] write `.strut/info/<target>/<mode>/<unit>.info.json` after a successful object build.
- [x] record source path, resulting object path, direct/transitive local includes, package source/header dependencies, generated/embedded resource dependencies, compiler/version/target/mode fingerprints, and relevant config.
- [x] metadata missing, malformed, old-format, or referring to removed dependencies forces recompilation.
- [x] write metadata only after the corresponding `.o`/`.obj` build succeeds.
- [x] use JSONIC.
- [x] tests + docs.

### CP85 — Incremental dependency invalidation
- [x] compare each recorded dependency mtime against its `.info.json` mtime to decide whether its object must be rebuilt.
- [x] as in Nift modified mode, treat dependency mtime equal to metadata mtime as potentially stale to avoid coarse-timestamp false negatives.
- [x] rebuild if object output is missing, metadata is missing/invalid, dependency is missing/changed, config/compiler/target fingerprint changed, or dependency graph changed.
- [x] reuse unchanged `.o`/`.obj` files and relink only what is necessary.
- [x] track dependency reasons for `strut status`/verbose builds.
- [x] leave room for a later hash/hybrid mode analogous to Nift without requiring hashes for the initial implementation.
- [x] multi-file regression fixtures proving one changed header/source recompiles only affected objects.
- [x] tests + docs.

### CP86 — Measure first serious app
- [x] executable size.
- [x] cold startup.
- [x] idle memory.
- [x] simple HTTP throughput/latency.
- [x] compile time.
- [x] compare debug vs release.
- [x] save reproducible methodology/results.

## Phase 15 — Project CLI, formatter, tests, docs maintenance

### CP87 — `strut make`
- [x] project discovery/manifest loading.
- [x] compile project entrypoint/dependencies.
- [x] incremental build strategy.
- [x] debug/release selection.
- [x] clear diagnostics.
- [x] docs.

### CP88 — `strut test`
- [x] native Strut test convention/API.
- [x] test discovery.
- [x] filtering.
- [x] failure output.
- [x] parallelism policy.
- [x] self-host Strut package tests with it where possible.

### CP89 — Formatter
- [x] define canonical formatting.
- [x] mandatory-semicolon output.
- [x] one canonical pointer/ref/type spelling.
- [x] idempotence tests.
- [x] formatter fixtures in regression suite.

### CP90 — CLI polish
- [x] help/version.
- [x] compile/make/test/fmt/package commands settled.
- [x] static/dynamic/mixed linking flags and project configuration settled.
- [x] consistent exit codes.
- [x] shell completion if worthwhile.
- [x] no gratuitous aliases that complicate docs/agents.

### CP91 — Website/docs maintenance audit 1
- [x] update every implemented language page.
- [x] remove stale speculative syntax.
- [x] update install/getting-started/CLI/package docs.
- [x] document current functions/templates, operator overloading, streams, `exec`, and linking modes.
- [x] update examples to compile against current Strut.
- [x] run `nift build`.
- [x] inspect mobile menu + 404 after docs growth.
- [x] commit website changes.

### CP92 — Regression-suite maintenance audit 1
- [x] ensure every shipped syntax/type/memory/error/concurrency feature has coverage.
- [x] add missing negative tests.
- [x] add first multi-file/project/package fixtures.
- [x] add CI if toolchain is ready.
- [x] commit suite changes.

## Phase 16 — Optimisation, tooling, platforms

### CP93 — Baseline benchmark suite
- [x] compiler compile time.
- [x] generated program startup.
- [x] stripped hello-world executable size.
- [x] static vs dynamic executable size.
- [x] static vs dynamic startup time and runtime memory overhead on representative programs.
- [x] static vs dynamic runtime performance where meaningful.
- [x] runtime/package size contribution and dead-code elimination effectiveness.
- [x] arithmetic/loops.
- [x] strings/collections/JSON.
- [x] ptr/ref-count overhead.
- [x] lambda/call overhead.
- [x] threading/async.
- [x] HTTP/SQLite representative tasks.
- [x] reproducible benchmark docs.

### CP94 — Optimisation pipeline
- [x] dead-code elimination.
- [x] constant folding.
- [x] inlining strategy.
- [x] escape/refcount optimization opportunities.
- [x] release optimization/LTO where backend supports it.
- [x] benchmark every claimed win.

### CP95 — Reference-count optimisation pass
- [x] elide provably unnecessary increments/decrements.
- [x] prefer `T&` borrowing in hot internal APIs.
- [x] benchmark before/after.
- [x] verify memory semantics unchanged with regression suite/sanitizers.

### CP96 — Debug information and stack traces
- [x] useful source-level debug metadata.
- [x] runtime panic/error stack traces where appropriate.
- [x] symbol handling debug vs release.

### CP97 — Language server/editor support
- [x] parser/typechecker reuse.
- [x] diagnostics.
- [x] go-to-definition.
- [x] completion.
- [x] hover/type info.
- [x] formatting integration.

### CP98 — Linux certification
- [ ] x64.
- [ ] arm64 if practical.
- [ ] clean install/build/test docs.
- [ ] regression suite passes.

### CP99 — macOS certification
- [ ] arm64.
- [ ] x64 only if support cost is justified.
- [ ] regression suite passes.

### CP100 — Windows certification
- [ ] x64.
- [ ] native paths/files/network/runtime fixes.
- [ ] regression suite passes.

### CP101 — Cross compilation
- [x] settle target naming.
- [x] produce binaries for supported targets where toolchain permits.
- [x] official-package native dependency story.
- [x] document limitations explicitly.

## Deferred ideas

Server-side/Nift-style templating is deliberately out of the active implementation roadmap. Reconsider it only after the core language, packages, web/backend stack, and deployment model are mature enough to judge whether it belongs in Strut.

## Phase 17 — Hardening

### CP102 — Parser/typechecker fuzzing
- [x] malformed source corpus.
- [x] parser fuzzing.
- [x] typechecker fuzzing.
- [x] no crashes/hangs on invalid programs.

### CP103 — Memory/runtime hardening
- [x] sanitizers.
- [x] Valgrind/equivalent where useful.
- [x] refcount overflow policy.
- [x] weak-pointer races.
- [x] destruction-order torture tests.

### CP104 — Concurrency hardening
- [x] thread sanitizer where feasible.
- [x] mutex/channel stress.
- [x] async scheduler stress.
- [x] cancellation/shutdown races.
- [x] document guarantees/limitations honestly.

### CP105 — FFI/unsafe hardening
- [x] ABI torture fixtures.
- [x] raw-pointer escape cases.
- [x] clear boundary between safe guarantees and unsafe responsibility.
- [x] docs/security guidance.

## Phase 18 — Serious dogfooding and ecosystem

### CP106 — Build multiple non-trivial Strut programs
- [x] CLI utility.
- [x] concurrent/network service.
- [x] data/JSON-heavy tool.
- [x] one-binary web app.
- [x] use dogfood pain to revise APIs before stability freeze.

### CP107 — Package ecosystem dogfood
> Historical note: the CP107 package implementations were subsequently extracted from `strut/dogfood/packages` into standalone `strut-packages/*` repositories. Do not add package implementation files back to the compiler repository.

- [x] HTTP.
- [x] SQLite.
- [x] TLS.
- [x] at least one additional database/integration package.
- [x] package authoring docs.
- [x] verify package workflow from clean machine/environment.

### CP108 — Website/docs maintenance audit 2
- [x] make website represent the real language, not early concept syntax.
- [x] comprehensive language reference.
- [x] package docs.
- [x] examples/tutorials.
- [x] benchmark/performance methodology where claims are made.
- [x] responsive/mobile/404/accessibility recheck.
- [x] `nift build` green.
- [x] commit.

### CP109 — Regression-suite maintenance audit 2
- [x] full feature matrix.
- [x] real-project fixtures.
- [x] package fixtures.
- [x] cross-platform CI matrix where practical.
- [x] compatibility policy for releases.

### CP110 — Investigate rewriting Nift in Strut
- [x] map Nift requirements against Strut capabilities.
- [x] identify missing systems/IO/performance capabilities.
- [x] prototype one meaningful Nift subsystem.
- [x] measure performance/memory/complexity.
- [x] decide based on evidence, not symbolism.

### CP111 — Self-hosting feasibility review
- [x] determine what is required for Strut to compile its own compiler.
- [x] prototype only if it benefits the project.
- [x] do not distort language design merely to achieve a vanity milestone.

## Phase 19 — Stability and release readiness

### CP112 — Language design audit
- [x] review every provisional decision in `LANGUAGE_HANDOVER.md`.
- [x] remove dead syntax/features.
- [x] resolve extension/header/source conventions.
- [x] resolve package manifest/CLI conventions.
- [x] resolve error propagation and inheritance/contracts.
- [x] ensure one canonical spelling for core constructs.
- [x] audit fixed operator set/overload semantics and confirm no parser-extension creep.
- [x] audit stream/process APIs against real systems programs.
- [x] audit static/dynamic linking and executable-size goals against real programs.

### CP113 — Compatibility/versioning policy
- [x] semantic/versioning strategy for compiler/language/packages.
- [x] deprecation policy.
- [x] regression-suite compatibility baselines.
- [x] package compatibility expectations.

### CP114 — Security/reliability review
- [x] safe memory guarantee audit.
- [x] unsafe/FFI audit.
- [x] package resolver/supply-chain review.
- [x] HTTP/TLS default review.
- [x] parser/input hardening review.

### CP115 — Release candidate certification
- [x] compiler unit/integration tests green.
- [x] full independent regression suite green.
- [x] supported OS matrix green.
- [x] examples compile.
- [x] website builds cleanly with Nift.
- [x] website docs match compiler.
- [x] official packages green.
- [x] benchmark/regression gates green.

### CP116 — First serious public release
- [x] tag/release compiler (`v0.0.1`, followed by certified `v0.0.2` and `v0.0.3` checkpoints).
- [x] publish install artifacts/instructions.
- [x] publish/verify official packages.
- [x] publish docs/changelog.
- [x] preserve regression baselines for released versions.

## Ongoing rule after every checkpoint

For any checkpoint that changes user-visible behaviour, ask all four questions before calling it complete:

1. Is the compiler/runtime implementation correct and tested?
2. Is `strut-regression-suite` updated and green?
3. Is `strut-labs.github.io` updated where users need documentation/examples, rebuilt with `nift build`, and committed?
4. Are the Strut handover/design/checkpoint docs still truthful?

# Extended roadmap — after the HTTP/reactor runtime campaign (R0-R7)

This section is the authoritative current long-term sequencing. The detailed
checkpoint phases above (CP1-CP116) remain relevant as the *existing language /
product maturity work*; this section places them in order, adds the active
runtime/performance campaign and the deferred FFI and low-level systems phases,
and records why the ordering matters. Keep this section and its rationale when
planning future work. Preserve the completed-checkpoint history above; do not
rewrite it.

## Sequencing rationale

- **Performance / request runtime first.** The HTTP reactor campaign
  (replacing thread-per-connection with a reactor + CPU-sized app workers +
  bounded stream pool) is already in flight and is the highest-leverage open
  work: it changes the server runtime from a "collapse under 50 connections"
  profile to a bounded-thread, backpressured, Go-competitive one.
- **Two-way FFI / embedding second.** It is the blocker for the actual Nift
  package / native-extension use case; nothing generic should be built for its
  own sake, it must be dogfooded against Nift.
- **Existing language maturity third.** Phases 2-19 (CP items) above are the
  older planned work; preserve and finish the still-relevant items rather than
  losing them.
- **Release milestone(s).** Do not accumulate unreleased work forever; a
  release may be warranted right after the HTTP/runtime campaign.
- **Low-level systems readiness as a dedicated cross-domain audit** later, so
  kernels/embedded/hypervisors/bootloaders/drivers/HP-userspace share one
  coherent low-level core instead of six overlapping feature sets.

## HTTP/runtime performance campaign — COMPLETE

Status: R0-R8 and the R8.5 hot-path campaign are COMPLETE and closed under exit
criterion B. Final retained compiler `0e95d0d` (main); independent closure challenge
and handover in `strut-benchmarks`. No further R8.5 candidates are active. Historical
record below preserved.

Current status (R0-R7 complete): thread-per-connection replaced by a
production-oriented reactor; buffered HTTP, response streaming, true
incremental request streaming, and WebSockets are reactor-native with bounded
adapters. Historical single-vCPU plaintext progression to keep on record:

- original ~1.2k req/s
- TCP_NODELAY legacy ~9k
- first reactor ~11-12k
- reactor + vectored write (writev) ~16.8-16.9k
- R5-R7 maintained ~16k (session variance)

Current frozen controls: Go ~24k, Rust >=35k (generator-limited floor — the
real Rust ceiling is higher). First performance checkpoint: match/beat Go, then
keep pushing toward Rust while gains are general, semantics stay correct, and
ownership/safety stays strong.

Remaining campaign checkpoints:

- R8 — reactor-native TLS (`SSL_accept`/`SSL_read`/`SSL_write` translated
  through `WANT_READ`/`WANT_WRITE` into reactor interests; incremental
  handshake off the sole app worker; slow/pathological peers consume connection
  state, not the app worker).
- R8.5 / early-R13 — aggressive, measured Linux request hot-path work: parser,
  header representation, copies/allocations, serialization, route/connection
  lookups, wakeup batching, recv-drain strategy, generated C++ abstractions.
  Keep the performance ledger (SHA, hypothesis, evidence, c1/c10/c50,
  latency/CPU/RSS, ratios vs Go and vs the Rust floor) and A/B every retained
  change.
- R9 — real kqueue/macOS backend and certification (the current POSIX
  poll+self-pipe is a portability scaffold, not completion).
- R10 — Windows readiness backend and certification (WSAPoll; WSASend path
  already hosted-compiled; IOCP remains a later scalability investigation).
- R11 — race/resource/platform hardening; TSan where practical.
- final Linux rebenchmark; stronger-generator authoritative Strut/Go/Rust
  comparison; 1/2/4-vCPU scaling; ship/keep-or-shard decision for one-reactor +
  N workers.
- reactor-default promotion decision once request streaming, response
  streaming, WebSocket, TLS, kqueue, Windows, and hardening are all green; do
  not leave the good runtime behind an undocumented env var forever.

Generator trigger: when Strut reaches roughly 28-30k+ req/s, or the generator
exceeds ~80-85% CPU while driving Strut, upgrade/resize the generator and use it
equally for Strut, Go, and Rust before claiming final percentages.

## NOW — Release A (v0.0.4)

A new Strut version is being released immediately after the HTTP/reactor/performance
campaign: v0.0.4, a large HTTP/backend, runtime, and performance release, tagged from
the retained main (`0e95d0d` plus release metadata). This snapshots the reactor runtime,
streaming/WebSocket/TLS, indexed routing, request/response hot-path work, runtime
building blocks, and extensive certification before the two-way FFI/embedding campaign
begins. The reactor backend remains opt-in (`STRUT_HTTP_REACTOR=1`) at this release.

## NEXT — two-way FFI / embedding (needed for Nift packages)

Return to the previously deferred export/embedding direction. The goal is not
generic FFI; dogfood it against the Nift package / native extension use case.
Likely areas: stable exported C ABI / shared-library usage, host->Strut calls,
Strut->host callbacks (retained where required), ownership/lifetime rules across
the ABI, checked-error propagation, cancellation, owner-thread/affinity
dispatch, externally completed futures, native resource ownership, safe
teardown, callback-after-destruction prevention, and exception/error
containment. Design the exact API from the Nift integration evidence; do not
build a large abstract embedding framework without dogfooding it.

## LATER — remaining current language/product roadmap (preserved)

Finish the still-relevant items from Phases 2-19 above (package/export/private
boundary work, resolver/SemVer maturity, transitive dependencies, checked-error
remaining edges, generics/constraints/hashability, SQLite hardening, terminal
detection, Unicode iteration, package ecosystem, diagnostics, formatter/LSP/docs,
testing/fuzzing/security, concurrency/runtime hardening, compiler/generated-code
quality, production dogfood, self-hosting review). These are the authoritative
detailed checkpoints already documented above; link them here rather than
duplicating them. Do not accidentally lose older roadmap items while adding the
new phases.

## Release milestones

There may be two release points:

- release A — after the HTTP/runtime campaign (production-ready reactor runtime);
- release B — after FFI + broader maturity work.

Do not force everything into one enormous release.

## EXPLORATORY — low-level / freestanding systems readiness

A dedicated cross-domain audit (kernels, embedded/bare metal, hypervisors,
bootloaders, drivers, high-performance userspace) to identify shared foundations
first, then domain-specific requirements. Do not implement six overlapping
feature sets.

- Phase 5A: capability matrix across the domains (freestanding compilation,
  hosted-runtime/libc assumptions, runtime slicing, custom allocators,
  allocator-free/fixed-capacity containers, raw pointers/volatile/MMIO,
  exact ABI/layout, packed/alignment/unions, symbol/linker-section control,
  static-library emission, intrinsics, inline assembly, naked/special entry,
  atomics/precise ordering, no-hidden-alloc/blocking guarantees, predictable
  destruction, panic semantics, checked-error lowering in freestanding mode,
  no-unwind, integer modes, endian/bit ops, SIMD, zero-copy, comptime
  introspection, cross-compilation, emulator harnesses, debug friendliness).
  Classify each as common / kernel / embedded / hypervisor / bootloader /
  driver / HP-userspace, and prioritize common foundations.
- Phase 5B: a coherent Strut low-level core (freestanding target, minimal core
  library independent of host APIs, controllable component emission, exact
  layout/ABI, raw+volatile memory primitives, custom allocator model,
  fixed-capacity collections, intrinsics/inline asm, linker/section controls,
  explicit panic/runtime hooks, precise atomics/memory model, explicit
  no-hidden-work properties, strong bit/integer primitives, compile-time
  scripting/reflection). Prefer general facilities (e.g. a general
  calling-convention + symbol-name + section + naked-entry facility rather than
  a `@boot_entry` special case).
- Phase 5C: small focused dogfood projects (QEMU kernel boot, bare-metal
  GPIO/UART, minimal hypervisor bring-up, bootloader ELF load, simple virtio/PCI
  driver, HP userspace allocator/SIMD/zero-copy), feeding missing general
  capabilities back into Strut; only then add narrowly domain-specific features.

Cross-cutting goal: "what guarantees make Strut trustworthy when allocation,
blocking, layout, calling convention, and hidden runtime behaviour must be
explicit?" — stronger for all six domains, not just "can Strut compile a
kernel?".

### Compile-time metaprogramming direction (design, not current work)

Do not assume C++ template metaprogramming. Preferred direction to investigate
later: ordinary generics for parametric polymorphism, and compile-time Strut
scripting (`comptime { ... }`, exact syntax TBD) for metaprogramming /
reflection / declaration and code generation — read-only compiler reflection +
typed compile-time values + controlled emission, with generated declarations
re-entering normal semantic/type checking. Useful for interrupt/vector tables,
register maps, syscall tables, page-table constants, device descriptors, ABI
bindings, layout tables, SIMD dispatch tables. Avoid arbitrary mutable compiler
AST access as the default model. Do not implement during the current HTTP
campaign; persist the direction so it is not lost.
