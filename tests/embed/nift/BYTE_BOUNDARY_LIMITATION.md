# FFI-10: Strut-source byte-pointer boundary — precise limitation (with reproducer)

Reproducer: `tests/embed/nift/proto_probe.p` (extern `probe_bytes(ptr<uint_8>, int_32)`).

Observed failures (Strut 0.0.5-dev, native toolchain, `strut <p> -o`):

1. Array-element pointer: `buf := [uint_8(97), uint_8(0), ...]; ptr(buf[0])`
   -> `error: ptr(...) requires a T* safe pointer` (the extern FFI accepts `ptr<T>` only
   from a `new()`-owned lvalue; array-element lvalues are not accepted by this version).
2. Bytes literal: `bytes("abc")` -> `error: bytes size must fit int_64` (the `bytes`
   constructor is size-based, not content-based, in this Strut version).
3. Exposed data pointer: `ptr(b.data)` on a bytes value -> `error: ptr(...) requires a T* safe
   pointer` (no content-addressable data member is surfaced for the extern boundary).
4. Hex integer literals (`0xFF`) are not accepted by this parser version.

Working boundaries on the same toolchain (verified in committed fixtures):
   - int_32/int_64 arguments and returns (status/out contract),
   - `string` by-value via the documented compiler-coupled private fixture,
   - Nift byte-engine path inside the adapter (`strut_nift_bytes_check`).

Therefore Strut's *extern forward-FFI layer* as shipped cannot currently pass a source-origin
byte array (pointer + length) safely: the supporting language constructs (array-to-pointer,
content bytes literal, `.data` address) are not yet exposed. This is a Strut-side
language-surface limitation, not a Nift or public-strut-ABI defect. The adapter carries the
C-compatible `(data,length)` view internally; the full Strut-source byte marshalling is
tracked as a follow-up beyond FFI-10 (would require exposing the missing Strut constructs,
not a Nift change). Verified against relgcc (GCC) at commit time; not a Nift core issue.
