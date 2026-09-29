# Strut security and reliability model

## Safe-language boundary

Safe Strut is intended to prevent dangling safe pointers/references, use-after-free, double free, unchecked null safe-pointer dereference, and lifetime errors covered by the safe ownership/reference model. `T*` is reference-counted ownership, `T&` is non-owning/non-null borrowing with compiler lifetime restrictions, and `weak_ptr<T>` breaks shared-ownership cycles.

These guarantees do not turn unsynchronised shared mutation into safe concurrent logic. Cross-thread mutation must use the documented synchronization primitives.

## `unsafe`, raw pointers and FFI

`ptr<T>` operations and C ABI calls cross an explicit `unsafe` boundary. Once code opts into unmanaged pointers or external native code, Strut cannot prove that the external code respects object lifetimes, bounds, thread safety, or ABI contracts. Keep unsafe regions small and wrap them behind safe APIs where possible.

The approved embedded third-party dependencies are JSONIC, libcurl, and OpenSSL. No additional library should be vendored/embedded without explicit project approval. SQLite support may use the external/system SQLite library.

## Cryptographic primitives

Public cryptographic operations are intentionally narrow and binary-first. `secure_random_bytes` uses OpenSSL `RAND_bytes` and never substitutes a non-cryptographic generator. SHA-256 and HMAC-SHA-256 return raw bytes; OpenSSL failures become `CryptoError` without exposing the native error queue. Equal-length secret comparisons use `CRYPTO_memcmp`, while differing public lengths may return false immediately.

Base64 and unpadded Base64url decoding are strict and canonical. They reject whitespace, mixed alphabets, misplaced padding and non-zero unused trailing bits. Encoding-only programs do not link OpenSSL; cryptographic programs link `libcrypto` without `libssl`. SHA-1, password hashing, key management, certificate APIs and general encryption are not exposed by this foundation.

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

The libcurl client accepts only absolute HTTP(S) URLs, keeps certificate and hostname verification enabled, prevents HTTPS redirect downgrade, validates request metadata, reserves transport framing headers, and bounds buffered bodies plus cumulative headers. Final response headers are lowercase and duplicate single-valued fields or trailers fail closed. The client protocol policy is not SSRF destination authorization: loopback, private, link-local, proxy-routed and DNS-rebound destinations remain reachable unless the application rejects them.

Query and URL-encoded form decoding reject malformed escapes, encoded NUL and controls while preserving repeated values; `+` has form-style space semantics. Request cookies require token names and strict cookie octets. Response cookies use a structured, bounded representation that validates names, values and attributes before the common response head is committed. Each cookie is emitted as a distinct `Set-Cookie` field. Generic Set-Cookie is reserved, so support for repeatable cookies does not weaken case-insensitive duplicate rejection for arbitrary response headers or permit response splitting.

The built-in server supports strict HTTP/1.0 and HTTP/1.1 plus OpenSSL-backed HTTPS. Request lines, field syntax, Host, Content-Length and Transfer-Encoding are validated before dispatch. Duplicate Content-Length, TE/CL combinations, unsupported coding chains, controls, folded fields and ambiguous line endings fail closed. HTTP/1.1 chunked bodies use strict hexadecimal, extension, CRLF and terminal syntax; decoded payload and framing overhead are independently bounded, non-empty trailers are rejected, and handlers see only decoded bytes. Buffered and streaming input share this parser/body state. Request-stream response commitment excludes active and future body reads, preventing concurrent OpenSSL access through escaped handles. Persistent connections are reused only after validated body EOF and a finished self-delimited response; post-body carry bytes remain under the same parser, pipelined requests run sequentially, and unread or invalid bodies close instead of being drained. This preserves the request-framing boundary across HTTP/1.0 and HTTP/1.1 reuse. Response field names and values are validated before commitment; the transport exclusively owns Content-Type, Content-Length, Transfer-Encoding and Connection, and invalid handler metadata becomes a safe 500. Buffered and streamed output share that validator. Unknown-length HTTP/1.1 streams use internally generated chunks and one terminal chunk; application data cannot inject framing. Failure after commitment closes the connection instead of attempting a second response. Each request receives an independent cancellation lifetime, including across connection reuse; cancellation on disconnect is cooperative and a CPU-only handler is not promised immediate peer-close detection. Admission and native workers are bounded by the configured connection limit, and synchronous streaming applies socket backpressure without an unbounded response queue. TLS 1.2 is the minimum, certificate/key material is validated before listening, handshake progress has an absolute timeout, failures never fall back to plaintext, and client/server lifecycle paths are release-certified. Upgrade ownership is detected but not exposed; HTTP/2, WebSockets, ALPN-driven protocol upgrades, certificate issuance/rotation, and ACME remain outside the implemented surface.

File responses parse decimal byte ranges with overflow checks, support only one range, and stream bounded chunks through the same response writer. Paths require valid UTF-8 without NUL and an initial regular-file check. Their explicit path is trusted, immutable application input: the helper does not turn request paths into safe filesystem paths, enforce root/symlink containment, or defend a checked path from concurrent local replacement.

NDJSON records use JSONIC's canonical validator to reject non-finite numbers, non-UTF-8 strings or keys, non-canonical exact-number spellings, and more than 512 nested levels before commitment. They then use JSONIC's compact serializer and append one LF after complete serialization, so untrusted strings cannot inject record delimiters. The helper checks request cancellation before and after serialization and keeps only one record in memory. Once any record commits the response, cancellation, disconnect, or write failure closes the stream without a synthetic terminal record or second HTTP response.

## Concurrency

Native threads, mutexes, channels and the async executor have stress fixtures. `T*` protects object lifetime, not arbitrary mutation. Non-owning `T&` is rejected at direct thread boundaries where lifetime cannot be established safely. Linux GCC CI runs the generated HTTP worker/shutdown suite under ThreadSanitizer; the local Swift-Clang TSAN runtime remains unusable because of its libdispatch linkage issue.

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

The tag-triggered release workflow gates every Linux x64, Linux ARM64, macOS ARM64 and Windows x64 package on CTest plus HTTP framing, buffered-response, response-streaming, request-streaming, application-helper, static-file/range, NDJSON, persistence, request-cancellation, worker, lifecycle and backend-baseline certification. Cross-platform CI additionally runs the high-risk generated HTTP suites and the crypto/encoding certification under GCC ASan/UBSan, plus the worker/shutdown suite under GCC ThreadSanitizer. Release certification also runs bytes, crypto/encoding, streams, cancellation and process cancellation before installation, archive upload and publication. Native static/dynamic FFI linkage is additionally certified for the POSIX packages; the current native-link fixture does not support the Windows toolchain.

## Current limitations

Strut is pre-1.0. Release candidates are certified on Linux x64/ARM64, macOS ARM64, and Windows x64. The independent regression suite remains a required CI gate.
