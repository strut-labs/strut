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
- official shorthand resolves only through the `strut-packages` organization; explicit third-party Git dependencies remain opt-in;
- fetched packages use authenticated HTTPS, immutable Git revisions, lock metadata, SHA-256 content verification, verified caches, and no implicit package build-script execution.

## HTTP and TLS

libcurl client TLS keeps certificate and hostname verification enabled by default. Disabling verification must never be an accidental default.

The built-in server supports strict HTTP/1.0 and HTTP/1.1 plus OpenSSL-backed HTTPS. Request lines, field syntax, Host, Content-Length and Transfer-Encoding are validated before dispatch. Duplicate Content-Length, TE/CL combinations, unsupported transfer framing, controls, folded fields and ambiguous line endings fail closed. Response field names and values are validated before commitment; the transport exclusively owns Content-Type, Content-Length, Transfer-Encoding and Connection, and invalid handler metadata becomes a safe 500. Buffered and streamed output share that validator. Unknown-length HTTP/1.1 streams use internally generated chunks and one terminal chunk; application data cannot inject framing. Failure after commitment closes the connection instead of attempting a second response. Admission and native workers are bounded by the configured connection limit, and synchronous streaming applies socket backpressure without an unbounded response queue. TLS 1.2 is the minimum, certificate/key material is validated before listening, handshake progress has an absolute timeout, failures never fall back to plaintext, and client/server lifecycle paths are release-certified. HTTP/2, chunked request decoding, ALPN-driven protocol upgrades, certificate issuance/rotation, and ACME remain outside the implemented surface.

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

The tag-triggered release workflow gates every Linux x64, Linux ARM64, macOS ARM64 and Windows x64 package on CTest plus HTTP framing, buffered-response, response-streaming, worker, lifecycle and backend-baseline certification. It also runs bytes, streams, cancellation and process cancellation before installation, archive upload and publication. Native static/dynamic FFI linkage is additionally certified for the POSIX packages; the current native-link fixture does not support the Windows toolchain.

## Current limitations

Strut is pre-1.0. Release candidates are certified on Linux x64/ARM64, macOS ARM64, and Windows x64. The independent regression suite remains a required CI gate.
