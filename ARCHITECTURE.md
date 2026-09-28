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

Runtime requirements are represented by stable `RuntimeComponentId` values and a
component registry in `runtime_components.cpp`. A single typed-IR traversal records
requirements for operations and runtime-backed types, then resolves this graph:

```text
IR operation/type -> required component IDs -> transitive closure -> ordered emit
```

The implemented IDs cover core values, strings, collections, IO, cancellation, JSON,
filesystem, environment, time, processes, safe/weak/raw pointers, threading,
channels, mutexes, async, networking, HTTP client/server, SQLite, embedded assets,
FFI, and the explicit migration fallback. Resolution computes transitive closure,
deduplicates IDs, emits dependencies before consumers in stable order, and rejects
unknown IDs or dependency cycles. Component metadata owns external link libraries;
the same resolved result now drives both source selection and native linking.

Runtime source bodies are physically grouped into proven slices and a full
compatibility body. Shared component emitters in `generated_runtime.cpp` provide
the owned bytes value, generic streams, cancellation, executor, TCP, HTTP client, HTTP
request/response helpers and active HTTP server to both paths, so those
facilities have one maintained implementation.
Component requirements choose among the bodies; remaining slice eligibility
predicates are safety assertions while core, JSON, SQLite, filesystem and thread
helpers are separated further. Mixed component sets conservatively use the full
body, preventing helper under-generation. This fallback is migration debt, not
the feature-selection API.

The process component depends on cancellation because each process pipe owns a
copied token context. That ownership is explicit at process construction; pipe
read/write signatures remain transport-neutral. Native wake objects and handle
lifetime synchronization stay inside the generated process implementation.

HTTP request intake separates transport acquisition from a pure validated
request-head/framing result and exact body acquisition. Routing still receives a
buffered request, but later body streaming can replace the body sink without
redefining request-line, header, Host, Content-Length or Transfer-Encoding rules.
Each listener generation owns a bounded blocking queue, a lazily grown reusable
worker set and its admitted socket registry. Admission, graceful drain and forced
shutdown are linearized under the generation lock and share one monotonic
deadline. Native socket operations pin handle ownership; shutdown wakes blocked
operations before final close so a descriptor cannot be reused underneath them.
TLS handshakes use nonblocking OpenSSL progress with platform polling against an
absolute deadline before entering the shared HTTP parser.

Buffered, static, error and streaming handler output passes through one validated
response-head serializer and one uncommitted/committed/finished state machine.
The transport exclusively owns Content-Type, Content-Length, Transfer-Encoding
and Connection semantics; malformed or conflicting application metadata becomes
a safe 500 before commitment. Known-length output is written directly, unknown
HTTP/1.1 output receives transport-generated chunk framing, and unknown HTTP/1.0
output is close-delimited. Streaming writes are synchronous socket writes with
bounded native call sizes rather than an intermediate whole-response buffer.
Future file, NDJSON and upgrade producers must reuse this state machine.
Server lifecycle transitions are explicit and the pending generation is published
before resolve/bind, so stop linearizes against both startup and active admission.

Initialization and teardown are component-owned RAII declarations. Platform
implementations share one component ID and select their body at emission time, so
platform selection does not create a second dependency graph.

To add a runtime-backed feature, add or reuse a component descriptor, request its
ID from the IR operation/type traversal, declare its dependencies and libraries in
the registry, and add direct resolver plus mixed-feature coverage. Do not add a
second linker scan or a new top-level runtime-template decision.

### Structured type migration

The authoritative semantic identity is `TypeId`, backed by an interned immutable
tree with a kind (`primitive`, `named`,
`const`, `reference`, `safe_pointer`, `raw_pointer`, `weak_pointer`, `nullable`,
`vector`, fixed array, `tuple`, `generic`, or `function`), child type IDs, and an
optional array extent. IDs are deterministic hashes of structural keys rather than
addresses. Nested nodes are interned recursively, so equality and alias identity
are constant-time ID comparisons. `T[]` and `vector<T>` share one vector node and
use `T[]` as the preferred diagnostic spelling.

`TypeSyntax`, symbols, `TypeInfo`, and typed IR retain source/canonical strings for
diagnostics and compatibility, but also carry their interned identity. Semantic
compatibility, iterable element discovery, runtime-component type requirements,
and C++ lowering use structural nodes. C++ spelling is produced by a single
`TypeId` visitor. Source spans remain on syntax nodes and are never interned.

Add future type forms in `TypeNodeKind`, the canonical parser/interner, the Strut
pretty-printer, and the C++ lowering visitor. Semantic consumers should query node
kinds and children; they must not introduce another generic-string parser. Some
operation-specific semantic rules still use readable type spellings while their
conversion to direct `TypeId` queries continues; those strings are not used by the
runtime-component resolver or C++ type lowering.
