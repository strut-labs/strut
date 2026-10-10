# FFI-10: Strut-source binary round trip — SUPPORTED mechanism (record)

SUPERSEDED: an earlier investigation concluded that Strut could not pass source-origin byte
buffers through extern FFI. That conclusion was WRONG. The supported route (verified at commit
time, GCC Linux, Strut 0.0.5-dev):

    bytes payload := [97, 0, 98, 255, 128];       // array literal -> bytes container
    raw_ptr<uint_8> data := payload.data();        // bytes.data() returns the content pointer
    rc := strut_nift_bytes_roundtrip(data, 5, out.data(), 5, ptr(out_len));   // extern "C"

Only the array-element form `ptr(buf[0])` and `bytes("literal")`/`ptr(b.data)` (no-call form)
were rejected -- those are expression-shape issues, not capability gaps. `bytes.from_string()` /
array-literal construction, `.data()`, `.length()`, indexing `b[i]`, `bytes(n)` capacity, and
`raw_ptr<uint_8> extern` parameters together provide full source-owned contiguous-byte
marshalling. Full round trip (Strut -> adapter -> nift_engine_set_bytes -> evaluate "b" ->
value_bytes -> Strut-owned output -> per-byte Strut verification, plus the empty-buffer case)
passes: BYTES-RT-OK / EMPTY-OK. Hex integer literals (0xFF) remain unsupported by this Strut
parser version; use decimal bytes.
