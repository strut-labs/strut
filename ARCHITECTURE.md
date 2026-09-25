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

Safe Strut ownership is reference-counted (`ptr<T>`) with `weak_ptr<T>` for non-owning shared references and `ref<T>` for non-owning non-null borrows. `raw_ptr<T>` is restricted to `unsafe` code. Strut has no tracing garbage collector.

## Jsonic++

Jsonic++ is the canonical JSON implementation for the bootstrap compiler/runtime wherever C++ needs to parse or emit JSON (project manifests, compiler metadata, tests, and later the Strut JSON implementation where appropriate). Keep the vendored header visibly attributable and easy to sync with upstream Jsonic++.

## Cross-platform rule

Do not put POSIX assumptions into parser, AST, semantic, or IR code. Platform branching belongs at narrow boundaries. Every new OS-facing API must define its Windows/macOS/Linux behaviour when introduced.

## Typed IR boundary

The parser AST is not a backend contract. After semantic checking, `IRLowerer` creates a backend-facing typed IR that preserves source spans while recording resolved/inferred type names on values. Backends consume this IR rather than parser details. This separation is intentional so native code generation can evolve independently of syntax and parsing.
