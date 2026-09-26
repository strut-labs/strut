# Strut Compiler Architecture

## Bootstrap implementation

The first Strut compiler and runtime are implemented in portable C++20.

Reasons:

- Strut is a native compiled language and C++20 gives direct control over memory, processes, files, threads, dynamic/static libraries, and platform APIs without introducing a managed bootstrap runtime.
- Nift and Jsonic++ provide useful C++ reference implementations already available to the project.
- GCC, Clang, Apple Clang, and MSVC make the bootstrap compiler practical on Linux, macOS, and Windows.
- The implementation can later be rewritten in Strut without changing the language contract.

The compiler source must avoid compiler-specific extensions unless isolated behind a portability abstraction and covered by platform checks.

## Frontend boundary

The frontend is deliberately independent of code generation:

```text
source manager
    -> lexer
    -> parser / AST
    -> name resolution
    -> type checking + safety analysis
    -> typed IR
```

Source locations are preserved from lexing onward. Diagnostics consume source spans rather than re-deriving line/column data late.

## Backend boundary

The initial bootstrap backend will lower typed Strut IR to portable C++20 and invoke an available host C++ compiler to produce native executables. This keeps the first native backend small and immediately usable across GCC/Clang/MSVC hosts while the language semantics are still changing.

The backend is an interface, not a frontend assumption. A later direct native backend (for example LLVM or another AOT backend) may replace or coexist with the C++ bootstrap backend without changing parsing, typing, ownership analysis, package semantics, or the Strut source language.

Backend requirements from the beginning:

- debug and release modes;
- static, dynamic, and mixed native library linking;
- Linux, macOS, and Windows target descriptions;
- explicit target/platform data rather than scattered preprocessor checks;
- deterministic command construction for external toolchains;
- generated artifacts isolated under the build directory.

## Runtime boundary

The runtime is kept small. Platform-specific services are isolated behind runtime/platform interfaces, including:

- executable/process launching;
- filesystem edge cases;
- dynamic library loading;
- clocks/environment;
- threads/synchronisation;
- sockets and other OS handles.

Safe Strut ownership is reference-counted (`T*`) with `weak_ptr<T>` for non-owning shared references and `T&` for non-owning non-null borrows. `ptr<T>` is restricted to `unsafe` code. Strut has no tracing garbage collector.

## Jsonic++

Jsonic++ is the canonical JSON implementation for the bootstrap compiler/runtime wherever C++ needs to parse or emit JSON (project manifests, compiler metadata, tests, and later the Strut JSON implementation where appropriate). Keep the vendored header visibly attributable and easy to sync with upstream Jsonic++.

## Cross-platform rule

Do not put POSIX assumptions into parser, AST, semantic, or IR code. Platform branching belongs at narrow boundaries. Every new OS-facing API must define its Windows/macOS/Linux behaviour when introduced.

## Typed IR boundary

The parser AST is not a backend contract. After semantic checking, `IRLowerer` creates a backend-facing typed IR that preserves source spans while recording resolved/inferred type names on values. Backends consume this IR rather than parser details. This separation is intentional so native code generation can evolve independently of syntax and parsing.

## Bootstrap code-generation compile-time policy

The C++ bootstrap backend emits a feature-minimal generated runtime for ordinary scalar, collection, lambda and safe-pointer programs instead of compiling the full JSON/network/filesystem/runtime surface into every translation unit. Full runtime support is emitted only when the program actually uses those facilities. Direct single-source release compilation intentionally skips LTO because the generated application and required inline runtime are already one translation unit; project/object builds retain LTO where it can optimise across translation units. This keeps cold compile time close to the equivalent host-C++ compilation while preserving `.strut` object caching for incremental builds.

### Runtime composition migration

Runtime selection currently uses mutually exclusive minimal, SQLite, HTTP-client,
JSON, async, HTTP-server, filesystem, and full-runtime emitters. Their eligibility
walks inspect IR operations and, in some cases, textual type names. A conservative
fallback to the full runtime prevents known mixed-feature programs from omitting
helpers, but every eligibility list is a manually maintained dependency boundary.

The target model is a deterministic component graph:

```text
IR operation/type -> required component IDs -> transitive closure -> ordered emit
```

Initial component IDs should cover core values, strings, JSON, safe/weak/raw
pointers, collections, checked errors, filesystem, environment/time, processes,
async executor, threads/channels/mutexes, sockets, TLS, HTTP client, HTTP server,
SQLite, and embedded assets. Each component owns its headers, declarations,
definitions, platform variants, link libraries, and component dependencies. For
example, HTTP client depends on strings, JSON, checked errors, and curl; async HTTP
also depends on the async executor. A topological sort with a stable component-ID
tie-break provides deterministic ordering, while a set of IDs deduplicates shared
helpers.

Migration is checkpointed:

1. Introduce a component manifest and compare its computed requirements with the
   existing runtime choice in tests, without changing emitted code.
2. Move external headers and link libraries to the manifest.
3. Extract one low-risk slice at a time (filesystem, SQLite, HTTP client), retaining
   full-runtime fallback whenever an operation has no declared component.
4. Delete each legacy eligibility walk only after mixed-feature and generated-C++
   certification covers the replacement.
5. Make an unknown runtime operation a compiler error in development builds and a
   full-runtime fallback in releases until the graph is complete.

Initialization and teardown are component-owned RAII declarations. Platform
implementations share one component ID and select their body at emission time, so
platform selection does not create a second dependency graph.

### Structured type migration

`TypeSyntax`, semantic types, and typed IR still carry canonicalized type names as
strings. This requires repeated parsing and textual prefix/subsequence checks in
semantic analysis, IR lowering, and C++ generation. Surface pointer/reference forms
inside generic arguments must therefore be canonicalized by the parser just as
top-level forms are.

The target `Type` is an interned immutable tree with a kind (`primitive`, `named`,
`const`, `reference`, `safe_pointer`, `raw_pointer`, `weak_pointer`, `nullable`,
`array`, `tuple`, `generic`, or `function`), child type IDs, optional array extent,
function parameters/result, and a resolved symbol ID for user-defined types.
Source spans remain on syntax nodes rather than interned semantic types. Formatting
and C++ spelling become visitors over this tree.

Migration is likewise incremental:

1. Add a single canonical type parser/interner and round-trip tests while retaining
   the existing string field as a compatibility spelling.
2. Store a `TypeId` beside type strings in semantic results and typed IR.
3. Replace compatibility, module-requirement, ownership, and container-element
   string checks with structural queries.
4. Convert C++ type emission and runtime-feature discovery to `TypeId` visitors.
5. Remove compatibility strings after all backends and diagnostics use structured
   types.
