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
  it. Round-trips primitives only (aggregates are FFI-3; checked-error propagation is
  FFI-6). Cross-platform. [Historical: the original FFI-1 plan over-scoped aggregate +
  checked error; the checkpoint was deliberately narrowed.]
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
  at FFI-8/9 after real consumers. `bool` is **deferred** (not exported, not an aggregate
  field) until its cross-toolchain ABI identity is explicitly certified.
## FFI-2 status (strings/bytes — COMPLETE, cross-platform certified, 0.0.5 dev)
- **Cross-platform certification run `37767470374` (commit `a5c8c28`): all five jobs green, with
  `strut_ffi_export_tests` demonstrably executed and passing on every platform** — macOS ARM64
  AppleClang (2.53s), Windows x64 MSVC (2.90s), linux-x64-gcc (1.85s), linux-x64-clang (2.86s),
  linux-arm64-gcc (2.42s). This covers `--shared` build (`.so`/`.dylib`/`.dll` + MSVC import
  lib), `--emit-c-header`, independent C + C++ hosts, and the full FFI-2 ownership round-trip
  (borrowed input, owned output, explicit release, binary bytes, empty/embedded-NUL/UTF-8).
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
  from the SAME library: `void <module>_ffi_free_string(char*)` /
  `void <module>_ffi_free_bytes(uint8_t*)`, where `<module>` is a deterministic slug derived
  from the source module name (same value in codegen and header; overridable later). Hosts
  must never `free()`/`delete[]`/`LocalFree()` these. This makes allocation/free provenance
  safe across the Windows DLL/host CRT boundary. **Why module-qualified rather than a generic
  `strut_ffi_free_*`:** two independently built Strut shared libraries loaded into one host
  would otherwise each export the same generic name — a duplicate-symbol failure at the MSVC
  import-library link step and interposition on ELF/mach-o, so a buffer from library A could be
  released by library B. Certified by the permanent `strut_ffi_multilib_tests` CTest (two
  libraries, one host, each buffer released through its own module's symbol).
- **Empty/null contract (valid input only).** Canonical empty is `data == NULL, len == 0` for
  both input and output, for both string and bytes. `data != NULL` means `len` bytes are valid
  for the duration of the call. `data == NULL && len > 0` is an **invalid host call, outside
  the valid ABI contract** — there is no promised behavior for it. The current wrapper avoids
  dereferencing such a pointer for defensive reasons only; **silent-empty is NOT contractual
  and must not be relied upon.** Structured handling of invalid ABI arguments is deferred to
  FFI-6 (status/error handle). `free(NULL)`/release of a canonical empty result is safe. FFI-2
  does not encode optional/nullability semantics (no NULL-means-null).
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
## FFI-3 status (plain POD aggregates — COMPLETE, cross-platform certified, 0.0.5 dev)
- **Cross-platform certification run `37777610235` (commit `732f63e`): all five jobs green,
  with `strut_ffi_aggregate_tests` demonstrably executed and passing on every platform** —
  macOS ARM64 AppleClang (1.69s), Windows x64 MSVC (3.56s), linux-x64-gcc (1.25s), plus
  linux-x64-clang and linux-arm64-gcc. FFI-1/FFI-2 CTests remained green in the same run
  (export 1.40s, multilib 2.20s on linux-x64-gcc).
- **ABI-safe aggregate rule (explicit; `abi_aggregate_safe` in `include/strut/abi_type.h`).**
  A user struct is ABI-safe for `export "C"` iff: not generic, no bases, no private fields,
  and **every field is an ABI primitive** (fixed-width int/uint or IEEE float). `string`/
  `bytes` fields (nested ownership), `bool` (deferred), nested aggregates, pointers/
  references, and collections are rejected with a **precise diagnostic naming the field and
  why** (e.g. `field 's' has type 'string' (string/bytes ownership in aggregates is not
  ABI-safe yet)`). `sizeof`/`alignof`/`offsetof` are therefore fully determined by the
  field list in declaration order; C++ traits alone are NOT treated as the contract.
- **Distinct generated C-ABI POD (not the internal Strut C++ struct).** The wrapper boundary
  exposes a generated C struct `strut_ffi_<module>_<Name>` (module-qualified to avoid
  collisions across modules). The internal Strut struct is never emitted as the C ABI: the
  wrapper converts field-by-field in (ABI -> internal) and out (internal -> ABI), so the
  internal representation can evolve without changing the promised ABI. This deliberately
  avoids the v0.0.4 AppleClang `-Wreturn-type-c-linkage` pattern of returning a C++ user type.
- **Header is the ABI authority.** `--emit-c-header` emits the `typedef struct
  strut_ffi_<module>_<Name> { ... }` definition; hosts must not duplicate the struct.
- **Layout certification from the C side.** `tests/ffi/run_export_aggregate_tests.py` (CTest
  `strut_ffi_aggregate_tests`) checks the C compiler's own `sizeof`/`offsetof` for the
  generated structs (e.g. `Pair` = 8 bytes @ offsets 0/4; `Mixed{int32,double,uint8}` = 24
  bytes @ offsets 0/8/16), plus struct-as-input, struct-as-return, mixed primitive fields,
  and an internal Strut call to an exported aggregate function (proves internal call sites
  hit the private impl, not the wrapper). Independent C and C++ hosts.
- **Naming/tuples:** aggregate C type names are module-qualified. Tuples are NOT implemented
  in this first FFI-3 step (user structs first); they remain rejected. Nested aggregates,
  `bool`, string/bytes fields, pointers, and collections remain rejected.
- **Strut -> native `extern "C"` direction:** the same ABI-safe POD rule and layout govern it
  (a standard-layout struct with only primitive fields is layout-identical), so the two
  directions are representation-compatible. The `export "C"` direction is fully implemented
  and certified here; routing the declaration direction through the same generated typedef is
  a deliberate follow-up (no incompatible second representation was introduced).
- Strict local wall: CTest **19/19** normal + GCC/Clang -Werror + ASan/UBSan; regressions
  **299/299** default + reactor; clean under ASan + leak detection.
- **Next: FFI-4** — pointers / nullable / references (and, if evidenced, nested aggregates /
  tuples as a later FFI-3 substep). No callbacks, error ABI, R9/R10, or Nift yet.
