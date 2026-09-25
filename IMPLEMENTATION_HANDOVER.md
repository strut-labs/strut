# Strut Implementation Handover — Checkpoint Game Plan

This is the ordered implementation roadmap. Checkpoints are intentionally concrete and should be marked complete only when code, tests, regressions, docs, and relevant commits are done.

Do not treat later checkpoint numbering as a reason to preserve a bad early decision. If implementation exposes a design problem, update the design docs and roadmap deliberately.

Current implementation progress: **CP0–CP38 complete; CP39 is next.**

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
- [x] owned runtime storage; `ptr<T>` reference-count integration is intentionally completed with the memory model in CP35.
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

### CP35 — Prototype `ptr<T>` runtime ownership
- [x] implement safe reference-counted owning pointer prototype.
- [x] copy increments count.
- [x] release decrements count.
- [x] zero count destroys deterministically.
- [x] define nullability interaction.
- [x] benchmark baseline overhead before optimizing.

### CP36 — Const pointer semantics
- [x] `ptr<T>`.
- [x] `ptr<const T>`.
- [x] `const ptr<T>`.
- [x] `const ptr<const T>`.
- [x] enforce referent vs binding const independently.
- [x] tests + regressions.

### CP37 — `ref<T>` safe borrows
- [x] `ref<T>` non-null/non-owning.
- [x] `ref<const T>`.
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

### CP39 — `unsafe` and `raw_ptr<T>`
- [ ] `unsafe { ... }`.
- [ ] `raw_ptr<T>` creation/use restrictions.
- [ ] pointer arithmetic policy.
- [ ] conversion rules between safe/weak/raw pointers.
- [ ] compiler prevents raw operations outside unsafe contexts.
- [ ] tests + regressions.

### CP40 — Memory-safety certification pass 1
- [ ] use-after-free attempts.
- [ ] dangling `ref<T>` attempts.
- [ ] invalid weak upgrades.
- [ ] double-destruction attempts.
- [ ] null safe-pointer cases.
- [ ] iterator/reference invalidation cases.
- [ ] sanitizers/Valgrind or equivalent on compiler/runtime tests.
- [ ] document what safe Strut guarantees and what `unsafe` opts out of.

## Phase 8 — Includes/modules, structs-as-contracts, generics, enums, errors

### CP41 — Local/module include semantics
- [ ] `include "foo.h"` / local dependency resolution.
- [ ] one semantic load, no textual macro preprocessor behaviour.
- [ ] include cycles/duplicate inclusion handling.
- [ ] file/module symbol boundaries.
- [ ] tests + regressions.

### CP42 — Package include syntax
- [ ] `include <package>` grammar/semantic placeholder.
- [ ] package namespace rules.
- [ ] distinguish official/third-party/local packages without making source verbose.
- [ ] tests + docs.

### CP43 — Abstract struct contracts
- [ ] permit structs with unimplemented function declarations.
- [ ] such structs cannot be instantiated while requirements remain unsatisfied.
- [ ] derived struct can provide required definitions.
- [ ] diagnostics list unsatisfied required methods.
- [ ] tests + regressions.

### CP44 — Inheritance and multiple contracts
- [ ] `struct User : Serializable, Printable` syntax.
- [ ] settle concrete-base/data inheritance rule.
- [ ] explicitly avoid accidental C++ diamond/layout complexity unless deliberately supported.
- [ ] method resolution/conflict diagnostics.
- [ ] tests + regressions.

### CP45 — Enums with explicit values
- [ ] implicit enum values.
- [ ] `name = integer` explicit values.
- [ ] continuation after explicit values.
- [ ] underlying representation policy.
- [ ] tests + regressions.

### CP46 — `switch`
- [ ] `case` / `default`.
- [ ] enum/integer/string policy as appropriate.
- [ ] fallthrough policy explicitly chosen; do not inherit C accidentally.
- [ ] exhaustiveness diagnostics where useful.
- [ ] tests + regressions.

### CP47 — `match`
- [ ] basic enum matching.
- [ ] wildcard/default pattern.
- [ ] payload/destructuring support only when corresponding types exist.
- [ ] exhaustiveness checking where possible.
- [ ] tests + regressions.

### CP48 — Generics/templates
- [ ] square-bracket declaration parameters: `function name[T]`, `struct Name[T]`, `operator[T]`.
- [ ] angle-bracket instantiated types: `Box<int>`, `ptr<T>`, etc.
- [ ] generic functions and structs.
- [ ] uppercase template identifier rule.
- [ ] implicit generic lambda inference from typed uppercase parameters.
- [ ] no constraint syntax until real use cases justify it.
- [ ] monomorphisation or chosen predictable strategy.
- [ ] diagnostics that remain readable.
- [ ] tests + regressions + docs.

### CP49 — Checked error signatures
- [ ] `-> X : Error`.
- [ ] `-> X : (E1, E2, ...)`.
- [ ] `throw Error(...)`.
- [ ] validate declared throw set.
- [ ] diagnostics for undeclared checked throws.
- [ ] tests + regressions.

### CP50 — Try/catch
- [ ] `try { ... }`.
- [ ] typed `catch (Error err)`.
- [ ] catch-all `catch { ... }`.
- [ ] propagation behaviour/shorthand prototyped and deliberately chosen.
- [ ] interaction with destructors/resource cleanup certified.
- [ ] tests + regressions.

## Phase 9 — Operator overloading

### CP51 — Fixed operator table and overload resolution
- [ ] define Strut's fixed operator spellings, precedence and fixity in one canonical compiler table.
- [ ] do not permit arbitrary new punctuation/text operators.
- [ ] do not permit user-defined precedence or associativity.
- [ ] distinguish prefix/infix/postfix forms where the language already defines them.
- [ ] define overload candidate lookup and ambiguity diagnostics.
- [ ] tests + regressions.

### CP52 — Normal operator declarations
- [ ] `operator +(A a, B b) -> R { ... }`.
- [ ] generic `operator[T] ...` declarations.
- [ ] overload arithmetic/comparison/stream operators from the approved set.
- [ ] support prefix `*` dereference for `ptr<T>` and make it overloadable.
- [ ] add indexing/call operators only where semantics remain clear.
- [ ] explicitly reserve structural operators such as member access unless later justified.
- [ ] tests + docs.

### CP53 — Lambda operator declarations
- [ ] `operator<(A, B) -> R> + := (a, b) => ...;`.
- [ ] `operator[T]<(A<T>, A<T>) -> R<T>> + := ...;`.
- [ ] use exactly the same callable signature grammar as lambda function declarations.
- [ ] accept visually repetitive operator tokens (`:= :=`, etc.) rather than adding special-case grammar.
- [ ] tests + formatter/regression fixtures.

### CP54 — Assignment and initialization overloading
- [ ] overload `=` for existing destination values.
- [ ] overload `:=` for typed construction/initialization into new destination storage.
- [ ] model `:=` destination as construction storage, not an existing `ref<T>`.
- [ ] inferred `x := value` must retain predictable type inference and must not let overloads invent an unrelated lhs type.
- [ ] define copy/conversion initialization interaction deliberately.
- [ ] tests + negative diagnostics.

## Phase 10 — Core IO/runtime facilities

### CP55 — Filesystem basics
- [ ] paths.
- [ ] exists/create/remove/copy/move.
- [ ] directories/listing.
- [ ] typed errors.
- [ ] cross-platform abstractions.
- [ ] tests + regressions.

### CP56 — Stream type foundation
- [ ] implement `istream`, `ostream`, `sstream`, `ifstream`, and `ofstream`.
- [ ] constructors/open/close and deterministic resource cleanup.
- [ ] text/binary read/write foundations.
- [ ] buffering and error semantics.
- [ ] tests + regressions.

### CP57 — Standard console streams and stream syntax
- [ ] `in` standard input stream.
- [ ] `out` standard output stream.
- [ ] `err` standard error stream.
- [ ] `ofstream ofs(path);` style construction.
- [ ] `<<` insertion and `>>` extraction using ordinary operator dispatch.
- [ ] `endl` or deliberately chosen equivalent.
- [ ] retain `print(...)`, `input()`, and `input(value)` conveniences.
- [ ] user-defined stream insertion/extraction overload fixtures.
- [ ] tests + regressions + docs.

### CP58 — Time/environment foundation
- [ ] environment access.
- [ ] clocks/durations.
- [ ] platform abstractions and typed errors.
- [ ] tests + docs.

### CP59 — Standard-library `exec`
- [ ] argv-based `exec(program, args)` without requiring shell interpolation.
- [ ] captured stdout/stderr and exit code.
- [ ] inherited stdio mode.
- [ ] cwd and environment overrides.
- [ ] shell execution, if provided, must be an explicit opt-in path rather than the default `exec` behaviour.
- [ ] typed launch/wait errors.
- [ ] regression fixtures using small deterministic helper executables.
- [ ] docs.

### CP60 — Child-process/pipe API
- [ ] spawn long-lived child processes.
- [ ] stream child stdin/stdout/stderr.
- [ ] wait/status/terminate semantics.
- [ ] pipe composition without requiring a shell.
- [ ] interaction with async/blocking IO documented.
- [ ] tests + regressions.

## Phase 11 — Threads, mutexes, channels, async

### CP61 — Native threads
- [ ] `thread(function, args...)`.
- [ ] lambda thread form.
- [ ] `join` and lifecycle semantics.
- [ ] return/error propagation policy.
- [ ] safe pointer/ref behaviour across threads.
- [ ] tests + stress regressions.

### CP62 — Mutex
- [ ] `mutex` type.
- [ ] explicit `lock()` / `unlock()`.
- [ ] scoped `mtx.lock(() => { ... });`.
- [ ] exception/error-safe unlocking.
- [ ] recursive/non-recursive policy.
- [ ] tests + regressions.

### CP63 — Channels/queues
- [ ] typed channel primitive if design still justified.
- [ ] send/receive/close semantics.
- [ ] blocking/wakeup correctness.
- [ ] tests + stress regressions.

### CP64 — Data-race safety policy
- [ ] define what the compiler/runtime promises.
- [ ] decide which sharing patterns require synchronization.
- [ ] diagnostics where statically feasible.
- [ ] document unsafe escape hatches honestly.

### CP65 — Async functions/futures
- [ ] `async function`.
- [ ] `await`.
- [ ] future/task type representation.
- [ ] async errors.
- [ ] tests + regressions.

### CP66 — Async lambdas
- [ ] `async (args) => expr`.
- [ ] async block lambda.
- [ ] capture/lifetime correctness.
- [ ] tests + regressions.

### CP67 — Multithreaded async executor
- [ ] implement real multithreaded executor.
- [ ] work scheduling/wakeup.
- [ ] blocking-operation policy.
- [ ] interaction with explicit threads.
- [ ] stress and race testing.
- [ ] benchmark scheduler overhead.

## Phase 12 — FFI and package foundation

### CP68 — C ABI FFI
- [ ] call C functions.
- [ ] primitive/struct ABI mapping.
- [ ] `raw_ptr<T>` integration.
- [ ] unsafe boundary rules.
- [ ] callbacks if practical.
- [ ] tests against tiny C fixtures.

### CP69 — Native library linking model
- [ ] support native static libraries and dynamic libraries through the FFI/build system.
- [ ] Linux `.a`/`.so`, macOS `.a`/`.dylib`, Windows static/import library + `.dll` model as appropriate.
- [ ] package/build metadata can request static, dynamic, or platform-default linking.
- [ ] fail with clear diagnostics when requested link mode is unavailable.
- [ ] tests against tiny native libraries in both modes.

### CP70 — Static, dynamic, and mixed project builds
- [ ] fully static build mode where platform/dependencies permit.
- [ ] ordinary dynamic build mode.
- [ ] mixed per-dependency static/dynamic mode.
- [ ] ensure the one-binary deployment path does not force all Strut programs to be statically linked.
- [ ] document runtime-library/linker search-path behaviour.
- [ ] release builds support symbol stripping and dead-code elimination regardless of link mode where the backend/toolchain permits.
- [ ] regression/integration tests.

### CP71 — Package manifest/cache design
- [ ] define project/package manifest format.
- [ ] dependency version syntax.
- [ ] lock/reproducibility strategy.
- [ ] shared local cache.
- [ ] deterministic resolution.
- [ ] docs.

### CP72 — Dedicated `strut-packages` ecosystem contract
- [ ] establish expected GitHub organisation/repository convention.
- [ ] define official package quality/test/docs expectations.
- [ ] package source/build metadata.
- [ ] local package development workflow.
- [ ] security/reproducibility expectations.

### CP73 — Package CLI
- [ ] settle `strut add <package>` or equivalent.
- [ ] install/update/remove/list workflow as needed.
- [ ] package include resolution for `include <...>`.
- [ ] lockfile updates are deterministic.
- [ ] tests + docs.

## Phase 13 — Networking and official packages

### CP74 — Socket/networking substrate
- [ ] decide minimal core vs official package boundary.
- [ ] TCP client/server primitives required by higher layers.
- [ ] async integration.
- [ ] typed errors.
- [ ] tests.

### CP75 — Official TLS package
- [ ] choose dependency/implementation strategy.
- [ ] safe certificate verification defaults.
- [ ] client/server support needed by HTTP.
- [ ] regression/integration tests.

### CP76 — Official HTTP package: client
- [ ] requests/methods/headers/body.
- [ ] JSON convenience using core JSON support.
- [ ] async client.
- [ ] redirects/timeouts/streaming policy.
- [ ] tests + docs.

### CP77 — Official HTTP package: server
- [ ] server/listen lifecycle.
- [ ] routing/path params/query/headers.
- [ ] request body text/bytes/JSON.
- [ ] response text/bytes/JSON/HTML.
- [ ] async handlers.
- [ ] tests + docs.

### CP78 — Official SQLite package
- [ ] create/use official SQLite package rather than bloating core stdlib.
- [ ] connection/open/close.
- [ ] prepared statements/parameters by default.
- [ ] query/exec.
- [ ] typed row mapping where feasible.
- [ ] transactions.
- [ ] async/blocking policy.
- [ ] tests + docs.

## Phase 14 — Embedded resources and first serious web app

### CP79 — Embedded files/directories
- [ ] compile file bytes into executable.
- [ ] directory embedding.
- [ ] content metadata/MIME helpers as appropriate.
- [ ] deterministic builds.
- [ ] tests.

### CP80 — Static asset serving helpers
- [ ] official HTTP integration for embedded assets.
- [ ] index/fallback support.
- [ ] ETag/cache headers.
- [ ] precompressed variants if justified.
- [ ] safe path handling.
- [ ] tests.

### CP81 — Build the first production-ish one-binary app
- [ ] HTTP server.
- [ ] JSON API.
- [ ] SQLite package.
- [ ] async handlers.
- [ ] embedded HTML/JS/CSS/assets.
- [ ] compile to one native executable.
- [ ] no runtime Node/static-server dependency.
- [ ] add integration/regression fixture.
- [ ] document on website.

### CP82 — Measure first serious app
- [ ] executable size.
- [ ] cold startup.
- [ ] idle memory.
- [ ] simple HTTP throughput/latency.
- [ ] compile time.
- [ ] compare debug vs release.
- [ ] save reproducible methodology/results.

## Phase 15 — Project CLI, formatter, tests, docs maintenance

### CP83 — `strut make`
- [ ] project discovery/manifest loading.
- [ ] compile project entrypoint/dependencies.
- [ ] incremental build strategy.
- [ ] debug/release selection.
- [ ] clear diagnostics.
- [ ] docs.

### CP84 — `strut test`
- [ ] native Strut test convention/API.
- [ ] test discovery.
- [ ] filtering.
- [ ] failure output.
- [ ] parallelism policy.
- [ ] self-host Strut package tests with it where possible.

### CP85 — Formatter
- [ ] define canonical formatting.
- [ ] mandatory-semicolon output.
- [ ] one canonical pointer/ref/type spelling.
- [ ] idempotence tests.
- [ ] formatter fixtures in regression suite.

### CP86 — CLI polish
- [ ] help/version.
- [ ] compile/make/test/fmt/package commands settled.
- [ ] static/dynamic/mixed linking flags and project configuration settled.
- [ ] consistent exit codes.
- [ ] shell completion if worthwhile.
- [ ] no gratuitous aliases that complicate docs/agents.

### CP87 — Website/docs maintenance audit 1
- [ ] update every implemented language page.
- [ ] remove stale speculative syntax.
- [ ] update install/getting-started/CLI/package docs.
- [ ] document current functions/templates, operator overloading, streams, `exec`, and linking modes.
- [ ] update examples to compile against current Strut.
- [ ] run `nift build`.
- [ ] inspect mobile menu + 404 after docs growth.
- [ ] commit website changes.

### CP88 — Regression-suite maintenance audit 1
- [ ] ensure every shipped syntax/type/memory/error/concurrency feature has coverage.
- [ ] add missing negative tests.
- [ ] add first multi-file/project/package fixtures.
- [ ] add CI if toolchain is ready.
- [ ] commit suite changes.

## Phase 16 — Optimisation, tooling, platforms

### CP89 — Baseline benchmark suite
- [ ] compiler compile time.
- [ ] generated program startup.
- [ ] stripped hello-world executable size.
- [ ] static vs dynamic executable size.
- [ ] static vs dynamic startup time and runtime memory overhead on representative programs.
- [ ] static vs dynamic runtime performance where meaningful.
- [ ] runtime/package size contribution and dead-code elimination effectiveness.
- [ ] arithmetic/loops.
- [ ] strings/collections/JSON.
- [ ] ptr/ref-count overhead.
- [ ] lambda/call overhead.
- [ ] threading/async.
- [ ] HTTP/SQLite representative tasks.
- [ ] reproducible benchmark docs.

### CP90 — Optimisation pipeline
- [ ] dead-code elimination.
- [ ] constant folding.
- [ ] inlining strategy.
- [ ] escape/refcount optimization opportunities.
- [ ] release optimization/LTO where backend supports it.
- [ ] benchmark every claimed win.

### CP91 — Reference-count optimisation pass
- [ ] elide provably unnecessary increments/decrements.
- [ ] prefer `ref<T>` borrowing in hot internal APIs.
- [ ] benchmark before/after.
- [ ] verify memory semantics unchanged with regression suite/sanitizers.

### CP92 — Debug information and stack traces
- [ ] useful source-level debug metadata.
- [ ] runtime panic/error stack traces where appropriate.
- [ ] symbol handling debug vs release.

### CP93 — Language server/editor support
- [ ] parser/typechecker reuse.
- [ ] diagnostics.
- [ ] go-to-definition.
- [ ] completion.
- [ ] hover/type info.
- [ ] formatting integration.

### CP94 — Linux certification
- [ ] x64.
- [ ] arm64 if practical.
- [ ] clean install/build/test docs.
- [ ] regression suite passes.

### CP95 — macOS certification
- [ ] arm64.
- [ ] x64 only if support cost is justified.
- [ ] regression suite passes.

### CP96 — Windows certification
- [ ] x64.
- [ ] native paths/files/network/runtime fixes.
- [ ] regression suite passes.

### CP97 — Cross compilation
- [ ] settle target naming.
- [ ] produce binaries for supported targets where toolchain permits.
- [ ] official-package native dependency story.
- [ ] document limitations explicitly.

## Deferred ideas

Server-side/Nift-style templating is deliberately out of the active implementation roadmap. Reconsider it only after the core language, packages, web/backend stack, and deployment model are mature enough to judge whether it belongs in Strut.

## Phase 17 — Hardening

### CP98 — Parser/typechecker fuzzing
- [ ] malformed source corpus.
- [ ] parser fuzzing.
- [ ] typechecker fuzzing.
- [ ] no crashes/hangs on invalid programs.

### CP99 — Memory/runtime hardening
- [ ] sanitizers.
- [ ] Valgrind/equivalent where useful.
- [ ] refcount overflow policy.
- [ ] weak-pointer races.
- [ ] destruction-order torture tests.

### CP100 — Concurrency hardening
- [ ] thread sanitizer where feasible.
- [ ] mutex/channel stress.
- [ ] async scheduler stress.
- [ ] cancellation/shutdown races.
- [ ] document guarantees/limitations honestly.

### CP101 — FFI/unsafe hardening
- [ ] ABI torture fixtures.
- [ ] raw-pointer escape cases.
- [ ] clear boundary between safe guarantees and unsafe responsibility.
- [ ] docs/security guidance.

## Phase 18 — Serious dogfooding and ecosystem

### CP102 — Build multiple non-trivial Strut programs
- [ ] CLI utility.
- [ ] concurrent/network service.
- [ ] data/JSON-heavy tool.
- [ ] one-binary web app.
- [ ] use dogfood pain to revise APIs before stability freeze.

### CP103 — Package ecosystem dogfood
- [ ] HTTP.
- [ ] SQLite.
- [ ] TLS.
- [ ] at least one additional database/integration package.
- [ ] package authoring docs.
- [ ] verify package workflow from clean machine/environment.

### CP104 — Website/docs maintenance audit 2
- [ ] make website represent the real language, not early concept syntax.
- [ ] comprehensive language reference.
- [ ] package docs.
- [ ] examples/tutorials.
- [ ] benchmark/performance methodology where claims are made.
- [ ] responsive/mobile/404/accessibility recheck.
- [ ] `nift build` green.
- [ ] commit.

### CP105 — Regression-suite maintenance audit 2
- [ ] full feature matrix.
- [ ] real-project fixtures.
- [ ] package fixtures.
- [ ] cross-platform CI matrix where practical.
- [ ] compatibility policy for releases.

### CP106 — Investigate rewriting Nift in Strut
- [ ] map Nift requirements against Strut capabilities.
- [ ] identify missing systems/IO/performance capabilities.
- [ ] prototype one meaningful Nift subsystem.
- [ ] measure performance/memory/complexity.
- [ ] decide based on evidence, not symbolism.

### CP107 — Self-hosting feasibility review
- [ ] determine what is required for Strut to compile its own compiler.
- [ ] prototype only if it benefits the project.
- [ ] do not distort language design merely to achieve a vanity milestone.

## Phase 19 — Stability and release readiness

### CP108 — Language design audit
- [ ] review every provisional decision in `LANGUAGE_HANDOVER.md`.
- [ ] remove dead syntax/features.
- [ ] resolve extension/header/source conventions.
- [ ] resolve package manifest/CLI conventions.
- [ ] resolve error propagation and inheritance/contracts.
- [ ] ensure one canonical spelling for core constructs.
- [ ] audit fixed operator set/overload semantics and confirm no parser-extension creep.
- [ ] audit stream/process APIs against real systems programs.
- [ ] audit static/dynamic linking and executable-size goals against real programs.

### CP109 — Compatibility/versioning policy
- [ ] semantic/versioning strategy for compiler/language/packages.
- [ ] deprecation policy.
- [ ] regression-suite compatibility baselines.
- [ ] package compatibility expectations.

### CP110 — Security/reliability review
- [ ] safe memory guarantee audit.
- [ ] unsafe/FFI audit.
- [ ] package resolver/supply-chain review.
- [ ] HTTP/TLS default review.
- [ ] parser/input hardening review.

### CP111 — Release candidate certification
- [ ] compiler unit/integration tests green.
- [ ] full independent regression suite green.
- [ ] supported OS matrix green.
- [ ] examples compile.
- [ ] website builds cleanly with Nift.
- [ ] website docs match compiler.
- [ ] official packages green.
- [ ] benchmark/regression gates green.

### CP112 — First serious public release
- [ ] tag/release compiler.
- [ ] publish install artifacts/instructions.
- [ ] publish/verify official packages.
- [ ] publish docs/changelog.
- [ ] preserve regression baseline for the released version.

## Ongoing rule after every checkpoint

For any checkpoint that changes user-visible behaviour, ask all four questions before calling it complete:

1. Is the compiler/runtime implementation correct and tested?
2. Is `strut-regression-suite` updated and green?
3. Is `strut-labs.github.io` updated where users need documentation/examples, rebuilt with `nift build`, and committed?
4. Are the Strut handover/design/checkpoint docs still truthful?
