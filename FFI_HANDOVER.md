# FFI / embedding handover

Development line: 0.0.5. Active major roadmap item: two-way FFI / embedding.
Reference: `IMPLEMENTATION_HANDOVER.md` ("NEXT — two-way FFI / embedding"), `FFI_SECURITY.md`.

## Direction model (do not collapse)
- **A. Strut → native** (call C functions): largely COMPLETE.
- **B. native → Strut** (host calls Strut functions): NOT present.
- **C. callbacks** (native invokes a Strut callback): DEFERRED/absent.
- **D. embedding** (host owns compiler/runtime lifecycle, compiles/evaluates/calls Strut):
  NOT present.

## Current capability matrix (from source, 0.0.5)
| Capability | Strut→native | native→Strut | callbacks | embedding |
|---|---|---|---|---|
| primitives | yes (CP68) | no | n/a | no |
| plain-layout struct by value/return | yes (CP68) | no | n/a | no |
| `ptr<T>` | yes (CP68) | no | no | no |
| strings/bytes across ABI | partial (via ptr+manual) | no | no | no |
| static lib linking | yes (CP69) | no | n/a | no |
| dynamic lib linking (.so/.dylib/.dll) | yes (CP69) | no | n/a | no |
| function-pointer callbacks | **deferred** | no | **no** | no |
| checked-error propagation across ABI | via `unsafe` only | no | no | no |
| exports / shared-library Strut | **no** | **no** | no | no |
| host consumers (C/C++/Go/Python/Node/C#) | n/a | none | none | none |
| cross-platform ABI warnings under -Werror | now handled (release #4 fix) | - | - | - |

Existing tests: `tests/ffi/ffi.p` + `fixture.c` + `run_native_link_tests.py` (primitive,
`double_64`, struct arg/return, const/mutable `ptr<int>`, static + dynamic). `FFI_SECURITY.md`
documents the unsafe boundary.

## ABI contract (to be frozen, cross-platform first-class: GCC/Clang/AppleClang/MSVC)
- Never expose a C++ implementation detail as C ABI: no `std::string`, `std::vector`,
  `std::shared_ptr`, C++ references, or exceptions across the boundary.
- Primitives: fixed-width (`int_8..64`, `uint_*`), `bool` as C `_Bool`/1 byte,
  `float_32`/`float_64` as IEEE, `ptr<T>` as raw pointer.
- Strings/bytes: pointer + length on input and output (`const uint8_t*`/`char*` + `size_t`),
  with an explicit ownership contract: **all allocation and ownership transitions are
  explicit at the ABI.** Library-owned results MAY allocate (that is expected); what is
  forbidden is *hidden ownership*, allocator ambiguity, or a host freeing Strut memory with
  the wrong allocator. There is no borrowed pointer silently becoming owned and no
  undocumented lifetime.
- Aggregates (structs): cross the ABI only if standard-layout + trivially copyable + all
  fields ABI-supported + fixed field order. Otherwise lower through a wrapper
  representation. (Motivated by the 0.0.4 AppleClang `-Wreturn-type-c-linkage` finding.)
- Ownership: every FFI-visible type states who allocates/owns/frees, retention, thread
  crossing, and post-call storage rules. Prefer explicit handles + retain/release.
- Errors: native exceptions never cross the ABI. Define Strut checked error -> C status/
  result + error handle/code (thread/runtime-local retrieval), and native failure ->
  Strut checked error. Base on the existing checked-error model; no parallel design.
- Callbacks: opaque callback handle + trampoline + context pointer; explicit
  retain/release; no raw pointers to movable/temporary Strut closures; defined
  invocation API, lifetime, thread/affinity, cancellation, and error behavior; prevent
  callback-after-destruction.
- Threads/reentrancy: define thread-safety vs thread-affinity vs explicit attach/detach vs
  serialization for "native calls Strut", "Strut calls native", "native callback calls
  Strut again", and cross-thread callbacks.
- Encoding/calling convention/symbol visibility: fixed and documented per platform
  (extern "C", default-visible exports; MSVC `__declspec(dllexport)`/`.def` as needed).

## Reconstructed checkpoint plan (reconcile with roadmap; will refine during FFI-1)
- FFI-0 (this doc): baseline matrix + ABI/lifetime/error/callback contract + plan.
- FFI-1: **export** — Strut builds a shared library exposing C ABI entry points
  (`export "C" function`/equivalent or a generated C shim); a C host compiles/links/calls
  it. Round-trips primitives + plain-layout aggregate + checked error. Cross-platform.
- FFI-2: strings/bytes/handles with explicit ownership + free functions.
- FFI-3: structs/tuples/aggregates formalization + ABI-safe rule enforcement.
- FFI-4: pointers/nullable/references.
- FFI-5: callbacks (opaque handle/trampoline/context; retain/release; invocation API).
- FFI-6: checked-error propagation both directions + host error retrieval.
- FFI-7: threads/callback lifetime/reentrancy (native calls Strut, reentrant callback).
- FFI-8: C embedding API stabilization (init/shutdown, call, value conversion).
- FFI-9: language bindings/consumers (C, then Go/Python/Node/C# as evidenced).
- FFI-10: Nift dogfood (real host, deliberate, non-disruptive to Nift releases).

## Rules
- Cross-platform ABI correctness (GCC/Clang/AppleClang/MSVC) is a first-class constraint in
  EVERY checkpoint, certified early — not at release time (v0.0.4 lesson).
- Keep the compiler/runtime boundary clean: explicit ABI layer, documented runtime types,
  stable exported functions, opaque handles — not generated C++ reaching into internals.
- No R9/R10/comptime/freestanding or unrelated language work during this campaign.
- No v0.0.5 tag/release until explicit instruction.

## Architecture decision needed before FFI-1 implementation
The export surface shape is a genuine decision: (a) a Strut `export "C" function` keyword
(sema/parser/codegen), vs (b) a generated C shim header/source from existing declarations,
vs (c) a compiler `--emit-c-abi` mode. Recommend (a) for first-class, stable, documented
exports, with a generated header for hosts; confirm before implementing.

## FFI-1 status (implemented locally, 0.0.5 dev)
- **Syntax:** first-class `export "C" function name(params) -> type { ... }` (parser/AST/IR
  flag `is_export_c`; represented through the pipeline, not a codegen text special-case).
- **Codegen:** every `export "C"` function is emitted as a **private internal implementation**
  (`strut_cexport_impl_<name>`, normal Strut C++ signature) plus a **public C-ABI wrapper**
  (`extern "C" STRUT_C_ABI_EXPORT`, the requested name) with a portable visibility macro
  (`__declspec(dllexport)` MSVC / `__attribute__((visibility("default")))` GCC/Clang / empty
  otherwise). Internal Strut call sites (including recursion) target the private
  implementation, never the wrapper. For primitive-only signatures the wrapper is a direct
  pass-through; FFI-2 gives the wrapper its marshalling role for string/bytes. This wrapper
  boundary is the intended future interception point for FFI-6 error containment.
- **Sema ABI validation:** exports must be free, non-async, non-generic, have a body; the
  return/parameter types must be ABI-supported primitives. **bool is DEFERRED** from the
  FFI-1 exported ABI (C `_Bool`/C++ `bool` cross-toolchain identity not yet certified).
  Supported: void, int/int_8/16/32/64, uint/uint_8/16/32/64, double_32 (->C float),
  double_64 (->C double). Checked-error exports are rejected with a deliberate diagnostic
  until the FFI error ABI (FFI-6); non-ABI types (aggregates/pointers/…) are rejected at
  sema. **Duplicate exported C symbol names are rejected** (no overloading across C ABI).
- **Single ABI type table:** `include/strut/abi_type.h` (`abi_type_info`) is the one source
  of truth used by BOTH sema validation and C-header generation (no drift; the exhaustive
  primitive fixture caught a `double_32`/`float_32` naming mistake early).
- **`export` composition:** `export` is NOT an existing Strut construct in this repo (not a
  keyword; `private` exists for struct methods; the roadmap's "package/export/private
  boundary" is a future item). `export "C"` is parsed with a guarded lookahead
  (`export` + string `"C"` + `function`) so it composes with a future ordinary `export`
  and does not consume a bare `export`.
- **Artifacts:** `strut --shared -o libfoo.so foo.p` builds a shared library; `strut
  --emit-c-header foo.h foo.p` writes a generated host header (C and C++ consumable).
- **Proof:** `tests/ffi/run_export_tests.py` builds the library + header, checks the
  generated declarations (incl. zero-arg `f(void)` and include-guard/multi-inclusion),
  compiles INDEPENDENT C and C++ hosts against the generated header, links the library,
  and asserts add/mul/double_32/double_64/uint_8/void results. Passing locally on Linux.
- **Status: FFI-1 COMPLETE (cross-platform certified).** `tests/ffi/run_export_tests.py` is
  registered as CTest `strut_ffi_export_tests` and executes on every platform. Cross-platform
  certification run (`f31dd10`): **linux-x64-gcc, linux-x64-clang, linux-arm64, macOS-arm64
  AppleClang, windows-x64-msvc — all green, with the FFI-1 round-trip demonstrably executed**
  (macOS 0.98s, Windows MSVC 1.51s, Linux GCC 0.59s): Strut `--shared` builds the platform
  library (`.so`/`.dylib`/`.dll` + import lib), `--emit-c-header` generates the header, and
  independent C and C++ hosts link and call exported functions with correct results.
- Strict local wall: CTest 17/17 normal + GCC/Clang -Werror + ASan/UBSan; regressions
  297/297 default + reactor.
- **Exception containment (limitation, explicit):** the C-ABI wrapper boundary now exists
  (private impl + public wrapper), but the wrapper does **not** yet add catch-all
  containment. An undeclared runtime `std::exception` (e.g. an internal bounds/allocation
  failure) could still propagate through the C linkage wrapper. General exception/error
  containment across the C ABI is NOT implemented yet; it belongs to the FFI error/wrapper
  checkpoint (FFI-6), which must guarantee nothing C++ unwinds into a C caller. Callers must
  treat exported functions as non-throwing by construction only.
- **ABI contract status:** C ABI **under development** (not yet frozen/versioned); freeze
  at FFI-8/9 after real consumers. Bool uses C `bool` (1 byte) and is included.
## FFI-2 status (strings/bytes, implemented locally, 0.0.5 dev)
- **Transport types (deliberately narrow, FFI-2 only).** `string` and `bytes` are NOT
  primitives and do NOT map to a single C type. They are lowered to explicit data/length
  pairs. These are **ABI plumbing for FFI-2**, NOT evidence that arbitrary Strut `ptr<T>`
  (FFI-4) or user aggregates/structs-by-value (FFI-3) are exportable — those remain rejected
  until their own checkpoints.
- **Borrowed input (native -> Strut).** `string`: `const char* <name>_data, size_t
  <name>_len`. `bytes`: `const uint8_t* <name>_data, size_t <name>_len`. No NUL dependence;
  length is authoritative. The host owns the buffer; Strut may READ it for the duration of
  the call and MUST NOT retain the pointer past return. The wrapper materializes a
  `strut_string`/`strut_bytes` copy at entry (copy-on-entry; zero-copy retained borrowing is
  not attempted yet).
- **Owned output (Strut -> native).** Returning `string`/`bytes` lowers the wrapper to
  `void` plus trailing out-parameters `char** out_data, size_t* out_len` (or `uint8_t**`
  for bytes) — chosen deliberately over returning a struct by value so FFI-3's
  aggregate-by-value checkpoint is not pre-empted. The buffer is allocated by the Strut
  library (`new[]` inside the library) and released only through an exported Strut function
  from the SAME library: `void strut_ffi_free_string(char*)` /
  `void strut_ffi_free_bytes(uint8_t*)`. Hosts must never `free()`/`delete[]`/`LocalFree()`
  these. This makes allocation/free provenance safe across the Windows DLL/host CRT
  boundary.
- **Empty/null.** Canonical empty is `data == NULL, len == 0` for both input and output, for
  both string and bytes. `NULL` + non-zero length is invalid host use (documented
  precondition; defensively treated as empty rather than dereferenced). `free(NULL)` is safe.
  FFI-2 does not yet encode optional/nullability semantics (no NULL-means-null).
- **String semantics (actual, not invented).** Strut `string` is `std::string`-backed:
  length is a **BYTE count** (not code points), **embedded NUL is preserved**, and the ABI
  does not scan for a terminator. No Unicode validation is claimed beyond what Strut string
  construction already does; arbitrary byte sequences round-trip.
- **Bytes are binary.** Arbitrary `0x00..0xff` including embedded zero; never NUL-terminated;
  length authoritative.
- **Single source of truth.** `include/strut/abi_type.h` now also holds the transport helpers
  (`abi_is_transport`, `abi_transport_c_element`, `abi_type_supported`) used by both sema and
  the generated-header lowering, alongside `abi_type_info`.
- **Proof:** `tests/ffi/run_export_tests.py` (CTest `strut_ffi_export_tests`) now also builds
  string/bytes exports and asserts, from INDEPENDENT C and C++ hosts: string echo/transform,
  UTF-8 byte length, embedded-NUL preservation, empty canonical form, input-lifetime
  (mutate host buffer after the call), owned-output lifetime, and 20k+20k alloc/free stress
  for string and bytes (binary `0x00/0x01/0x7f/0x80/0xff`). Clean under ASan + leak detection.
- Strict local wall: CTest 17/17 normal + GCC/Clang -Werror + ASan/UBSan; regressions
  297/297 default + reactor.
- **Deliberately NOT in FFI-2:** general `ptr<T>`/nullable/references (FFI-4), user
  aggregates/structs-by-value (FFI-3), generic opaque object handles (deferred until a real
  exported resource needs one), structured error/exception containment (FFI-6), a large FFI
  benchmark campaign. `bool` remains deferred.
- **Next: FFI-3** — structs/tuples/aggregates formalization + ABI-safe rule enforcement
  (standard-layout + trivially copyable + all fields ABI-supported; otherwise a wrapper
  representation). Only then may aggregate-by-value cross the ABI.
