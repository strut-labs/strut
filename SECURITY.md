# Strut security and reliability model

## Safe-language boundary

Safe Strut is intended to prevent dangling safe pointers/references, use-after-free, double free, unchecked null safe-pointer dereference, and lifetime errors covered by the safe ownership/reference model. `T*` is reference-counted ownership, `T&` is non-owning/non-null borrowing with compiler lifetime restrictions, and `weak_ptr<T>` breaks shared-ownership cycles.

These guarantees do not turn unsynchronised shared mutation into safe concurrent logic. Cross-thread mutation must use the documented synchronization primitives.

## `unsafe`, raw pointers and FFI

`ptr<T>` operations and C ABI calls cross an explicit `unsafe` boundary. Once code opts into unmanaged pointers or external native code, Strut cannot prove that the external code respects object lifetimes, bounds, thread safety, or ABI contracts. Keep unsafe regions small and wrap them behind safe APIs where possible.

The approved embedded third-party dependencies are JSONIC, libcurl, and OpenSSL. No additional library should be vendored/embedded without explicit project approval. SQLite support may use the external/system SQLite library.

## Parser/input hardening

The frontend has a deterministic malformed-source fuzz smoke test covering lexer, parser, and typechecker. Invalid source must diagnose or reject rather than crash/hang. Release hardening should continue adding corpus inputs for every parser bug discovered in real use.

JSON parsing uses JSONIC. Network/database/file data remains untrusted input and must still be length/semantic validated by the consuming program.

## Package and build supply chain

- package paths are validated against escaping package roots;
- lockfiles record immutable revision/content information for reproducibility;
- build caches are invalidated by dependency/build fingerprints;
- current package dogfood is local/cache based rather than silently fetching arbitrary remote code;
- future registry/network fetching must authenticate transport, verify selected content against lock metadata, and avoid implicit build-script execution unless such a mechanism is explicitly designed and reviewed.

## HTTP and TLS

libcurl client TLS keeps certificate and hostname verification enabled by default. Disabling verification must never be an accidental default.

The built-in server is currently plain HTTP. OpenSSL is approved for future server-side TLS, but HTTPS server support must not be documented or implied until implemented and tested. When added, TLS 1.2/1.3 configuration, certificate/key handling, ALPN, shutdown/error paths, and secure defaults require dedicated certification.

## Concurrency

Native threads, mutexes, channels and the async executor have stress fixtures. `T*` protects object lifetime, not arbitrary mutation. Non-owning `T&` is rejected at direct thread boundaries where lifetime cannot be established safely. ThreadSanitizer should be part of CI where the runner/toolchain supports a working runtime; the current local Swift-Clang TSAN runtime is not usable because of its libdispatch linkage issue.

## Native linking and processes

`exec` is argv-based by default so arguments are not implicitly interpreted by a shell. Shell execution is an explicit opt-in path. Static/dynamic native libraries and FFI inherit the security properties of the linked native code.

## Reliability gates

Release-candidate certification should include, at minimum:

- compiler unit/integration tests under supported compilers;
- independent black-box regressions;
- malformed-input fuzz smoke;
- sanitizer-backed runtime/memory tests;
- concurrency stress;
- FFI static/dynamic fixture tests;
- platform CI matrix;
- clean documentation build;
- benchmark/regression checks for material performance-sensitive changes.

## Current limitations

Strut is pre-1.0. Release candidates are certified on Linux x64/ARM64, macOS ARM64, and Windows x64. The independent regression suite remains a required CI gate.
