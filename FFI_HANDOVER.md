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
  `void <module>_ffi_free_bytes(uint8_t*)`, where `<module>` is the digest-qualified module
  namespace (same value in codegen and header; the header also emits a readable
  `...#_FFI_FREE_STRING` macro alias — see the module-slug note below). Hosts
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
## FFI-3 status (one POD aggregate ABI, both directions — COMPLETE, cross-platform certified)
- **One explicit primitive-only POD aggregate C ABI shared by Strut->native and native->Strut,
  certified across supported platforms (run `37783089417`, commit `d0ca784`, all five jobs
  green).** The bidirectional closure tests executed on every platform: macOS ARM64 AppleClang
  (bidir 2.07s / slug 6.29s), Windows x64 MSVC (bidir 2.60s / slug 7.57s), linux-x64-gcc
  (bidir 1.70s / slug 4.85s), plus linux-x64-clang and linux-arm64-gcc.
- **Earlier native->Strut certification run `37777610235` (commit `732f63e`): all five jobs green,
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
- **Strut -> native `extern "C"` direction (SAME ABI, symmetric).** `extern "C" function
  name(...) -> Pair;` declarations now lower their aggregate parameters/returns to the SAME
  generated ABI POD at the foreign boundary (`strut_ffi_<module>_Pair`), with field-by-field
  conversion at the call site (internal -> ABI for arguments, ABI -> internal for the return).
  The internal Strut struct NEVER crosses a C ABI boundary in either direction. Certified by
  `strut_ffi_aggregate_bidir_tests` (a native C library with a matching POD struct is linked
  via `--lib`; expected `7`/`11`).
- **Compile-time ABI POD assertions.** For every generated aggregate the emitted C++ asserts
  `std::is_standard_layout` and `std::is_trivially_copyable` on the ABI POD (the contract) —
  not on the internal Strut struct, whose representation is free to change.
- **ABI module identity is LOGICAL and checkout-independent.** Canonical identity is the
  source path **as provided to the compiler** (project/package-relative when builds invoke with
  relative paths), NEVER the build machine's absolute path — so the same project built under
  different checkout roots / CI workspaces / package-cache paths yields identical public ABI
  names. The ABI namespace = readable sanitized prefix + an always-present **128-bit
  deterministic digest** of that logical identity (`abi_digest128`: two FNV-1a-64 passes,
  fixed algorithm/input/output, not `std::hash`). This is a **deterministic strongly-
  disambiguated namespace, not an injectivity proof** — an accidental collision is non-credible
  but not impossible. No 32-bit surface; no sanitization-form ambiguity (`foo-bar`/`foo_bar`/
  `slug_a_deadbeef` distinct).
- **Readable, module-qualified aliases; unique include guards.** The generated header exposes
  aliases derived from the full logical id (`PKGA_UTIL_Pair`, `MODULE_A_UTIL_FFI_FREE_STRING`)
  so hosts never type the digest AND two headers that both define `Pair` (or both own results)
  coexist without collision. The include guard is also derived from the digest-qualified module
  slug, so two same-named sources in different modules do not swallow each other's declarations.
  Certified by `strut_ffi_module_slug_tests` and `strut_ffi_abi_identity_tests` (same module in
  two different absolute roots ⇒ identical headers; two `util.p` headers with `Pair` compile
  together under `-Wall -Wextra -Werror` / MSVC `/W4 /WX`).
- Strict local wall: CTest **24/24** normal + GCC/Clang -Werror + ASan/UBSan; regressions
  **302/302** default + reactor; clean under ASan + leak detection.
## FFI-4 status (borrowed primitive pointers/references — COMPLETE, cross-platform certified)
- **Cross-platform certification run `37792685458` (commit `97c2ba8`, after the MSVC C4456
  fix-forward `97c2ba8`): all five jobs green, with `strut_ffi_pointer_tests` and
  `strut_ffi_pointer_bidir_tests` demonstrably executed on every platform** — macOS ARM64
  AppleClang (2.08s / 2.38s), Windows x64 MSVC (2.22s / 2.22s), linux-x64-gcc (2.07s / 2.05s),
  plus linux-x64-clang and linux-arm64-gcc. (Earlier run `37789315890` at `c450f3f` was green
  on all but Windows, which failed the compiler build on MSVC `/WX` C4456 — fixed forward.)
- **Scope (deliberately narrow).** Borrowed `raw_ptr<T>` (possibly null) and `ref<T>`
  (non-null) are ABI candidates, and only when `T` has a direct primitive C representation
  (fixed-width int/uint or IEEE float). Both lower to the SAME C shape `T_c*`; the semantic
  contract differs (raw_ptr may be null; ref must be non-null, valid for the call only).
  Pointees of string/bytes/aggregates, nested ownership, and pointer-to-pointer are rejected.
- **Safe owning pointers and `weak_ptr<T>` are NEVER exposed.** Terminology: Strut *source
  syntax* for the safe owning pointer is `T*` (e.g. `int* p := new(7)`); its *inner/internal
  type spelling* is `ptr<T>`; its *generated C++ representation* is `std::shared_ptr<T>`. No
  `std::shared_ptr`/`std::weak_ptr`/control-block representation may cross the C ABI. Sema
  rejects them with an ownership-specific diagnostic ("cannot cross the C ABI: safe owning ptr
  / weak_ptr carries ownership/control-block representation") pointing at borrowed pointers or
  a future opaque handle API. No handle framework was invented.
- **Pointer RETURNS rejected.** A borrowed pointer/reference return has no defined
  lifetime/provenance across the ABI yet; sema rejects it and suggests an out-parameter.
  Ownership/lifetime are documented: input pointers are borrowed for the call duration only.
- **One descriptor, both directions.** `abi_pointer_supported`/`abi_pointer_inner`/
  `abi_pointer_owning` in `abi_type.h` drive sema, the generated C header, the export wrapper,
  and the `extern "C"` declaration/call-site lowering — no scattered `if (starts_with("raw_ptr<"))`
  branches. `ref<T>` params are realized internally as `strut_ref<T>` (non-owning) and its C
  boundary is `T_c*`; `raw_ptr<T>` is identity (`T_c*`).
- **Null semantics (certified).** `raw_ptr<T>` is nullable and compares against `null`
  (C `NULL` → true). For `ref<T>`, **`NULL` passed by a foreign caller is INVALID
  FOREIGN-CALLER BEHAVIOR, outside the valid ABI contract**: FFI-4 does NOT detect it, convert
  it to a checked error, or recover — structured boundary failure is FFI-6. The valid `ref<T>`
  contract is simply: non-null, pointee alive for the call duration. No C++ reference (`T&`)
  appears in any generated C header.
- **Certified by** `strut_ffi_pointer_tests` (native→Strut: raw_ptr/ref mutation, read, null;
  header is pure C, `int32_t*`) and `strut_ffi_pointer_bidir_tests` (Strut→native CP68
  `unsafe extern "C"` raw/ref mutation, expected `42`/`43`), both from independent C and C++
  hosts. Rejections: owning pointer, pointer return, pointer to non-primitive.
- Strict local wall: CTest **23/23** normal + GCC/Clang -Werror + ASan/UBSan; regressions
  **302/302** default + reactor; ASan clean.
- **Deferred:** nested aggregates/tuples (later FFI-3 substep), pointers to aggregates
  (copy-in/out semantics), owning/weak pointers (opaque handle + retain/release later),
  callbacks (FFI-5), error ABI (FFI-6).

## FFI-6 status (checked-error ABI — COMPLETE, cross-platform certified)
- **Cross-platform certification run `37822732197` (commit `cfe5bca`): all five jobs green,
  with `strut_ffi_error_tests` and `strut_ffi_error_bidir_tests` demonstrably executed on every
  platform** — macOS ARM64 AppleClang (2.45s / 2.34s), Windows x64 MSVC (3.43s / 3.15s),
  linux-x64-gcc (2.00s / 1.91s), plus linux-x64-clang and linux-arm64-gcc.
- **Model lowers the EXISTING Strut checked error** (`strut_checked_error`: `type` name,
  `message`, `code`), not a parallel one. No `std::exception*`/`type_info`/`exception_ptr`/
  RTTI crosses C.
- **C ABI:** `typedef int32_t strut_ffi_status;` (0 success, nonzero failure) + a module-owned
  **opaque error handle** `strut_ffi_<module>_error*` written to an `out_error` out-parameter.
  Query: `<module>_ffi_error_query(e, &type,&type_len,&message,&message_len,&code)` (borrowed
  views valid until release). Release: `<module>_ffi_error_release(e)`. Module-qualified
  symbols + header macro aliases (same provenance rule as FFI-2; no generic release symbol, no
  thread-local "last error"). Errors are explicit values, not TLS.
- **Exported checked-error functions** (now accepted where previously rejected):
  `export "C" function f(args) -> T : E` lowers to `strut_ffi_status f(args..., T* out_value,
  <module>_ffi_error** out_error)` (no `out_value` for `void`). Success: status 0, value set,
  error NULL. Failure: status != 0, concrete error (type/message/code) preserved. FFI-6 initial
  scope requires a **primitive or void return** for checked exports (string/aggregate returns
  deferred).
- **Native -> Strut:** `extern "C" function native(args) -> T : E;` lowers to a C
  status-returning call with a caller-provided native error descriptor
  (`strut_ffi_<module>_native_error { code, type/type_len, message/message_len }`); a nonzero
  status becomes `throw E(...)` inside Strut, catchable by `try/catch (E e)`. Symmetric with
  the export direction. **Multiple declared errors** `: (E1, E2)` preserve concrete identity.
  **Unknown-error policy:** if native reports a type NOT in the declared set, that is a native
  ABI contract violation → deterministic fatal boundary (`std::abort` with a diagnostic); the
  error is never coerced into a declared type.
- **Out-parameter preconditions (checked exports).** For `T f(..., T* out_value, error** out_error)`:
  `out_error` must be non-NULL; for non-void `T`, `out_value` must be non-NULL. On success:
  status 0, `*out_error = NULL`, `out_value` populated. On failure: status non-zero,
  `*out_error != NULL`, `out_value` is **unspecified** (not promised zeroed). `void` checked
  functions have no `out_value`.
- **Error-handle contract.** The handle stays valid until released; `query` returns
  type/message **borrowed views into the handle** (no NUL dependence; byte length
  authoritative), invalid after release; `release` is called exactly once, by the SAME module,
  and `query(NULL)`/`release(NULL)` are invalid host behavior (not promised safe).
- **ABI version.** The generated header defines `STRUT_FFI_ABI_VERSION_MAJOR`/`_MINOR`
  (currently 1/0) — distinct from the Strut language/compiler version. No negotiation yet.
- **Synchronous fallible callbacks.** A callback type declaring checked errors
  (`function<(args)->ret : E>` / `: (E1, E2)`) lowers to a status-returning C callback with a
  **caller-owned, borrowed** error descriptor (no per-failure heap handle):
  `strut_ffi_status (*)(void* context, args…, [ret* out_value,] <module>_callback_error* out_error)`
  where `callback_error { const char* type_data; size_t type_len; const char* message_data;
  size_t message_len; int32_t code; }`. ONE representation both directions.
  - **native->Strut:** the wrapper copies the descriptor, validates the reported type against
    the callback's declared errors (undeclared → fatal), and `throw`s the internal checked
    error; the export's own wrapper turns it into the ordinary opaque host error handle.
  - **Strut->native:** a generated per-signature trampoline catches the declared
    `strut_checked_error`, materializes context-owned strings, fills the borrowed descriptor,
    and returns failure status (native does not see `abort`, and copies before the views
    expire). `catch(...)`→`abort` remains for undeclared/unexpected exceptions.
  - Borrowed-view lifetime: valid until the next callback invocation or the enclosing foreign
    call returns, whichever is first; receivers copy before that. Infallible callback ABI is
    unchanged (`int32_t (*)(void*, int32_t)`). Certified by
    `strut_ffi_fallback_callback_tests` (C host callback fails → Strut → host error handle; 10k
    stress) and `strut_ffi_fallback_callback_bidir_tests` (Strut throw → native → Strut catch:
    `12/thrown/5`).
- **Containment (precise scope).** No exception originating from a generated Strut **export
  wrapper** or a Strut **callback trampoline** is allowed to unwind into foreign C: both are
  `try { … } catch (const strut_checked_error&) { …structured… } catch (...) { std::abort(); }`.
  The checked-error catch that allocates the opaque handle is itself wrapped
  (`try { *out_error = new …; } catch (...) { std::abort(); }`) so a `bad_alloc` while
  *marshalling the error* cannot escape either. For a function with no declared error channel,
  an unexpected/internal exception is a contained fatal boundary (never a fabricated success).
  **Foreign `extern "C"` implementations MUST obey the C ABI**: they must NOT throw C++
  exceptions across the boundary; failure must be reported through the FFI-6 status/error
  contract (throwing is a native-ABI contract violation). We deliberately do NOT wrap every
  ordinary native call in try/catch.
- **Non-error exports unchanged:** functions with no declared checked errors keep their
  existing direct signatures; only their bodies gained the containment boundary.
- **Certified by** `strut_ffi_error_tests` (success/failure/type/message/code/release/void +
  10k failure/release stress; C + C++ hosts; pure-C header) and
  `strut_ffi_error_bidir_tests` (native status/error -> Strut `ParseError` catch: `30/bad/7`).
- Strict local wall: CTest **30/30** normal + GCC/Clang -Werror + ASan/UBSan; regressions
  **307/307** default + reactor; ASan clean.
- **Deferred:** checked-error returns carrying string/bytes/aggregate; FFI-7 retained/
  cross-thread callbacks; FFI-8 embedding. **Next: FFI-7.**

## HTTP performance detour (bounded, COMPLETE; campaign banked 0.0.5-dev)
A bounded Campfire-Card transfer (HTTP-C*) was run and CLOSED before resuming FFI-7:
fair release (--release) two-node Strut reactor ~30.0k rps vs Rust ~34.96k rps (~14% behind);
~13.3k vs ~11.2k Ir/request (~19% compute gap). ~5% practical parity NOT reached (A, not D).
Main discovery: earlier ~10x compute and ~15.5k-vs-20.8k figures were artifacts of benchmarking
Strut at -O0; the benchmark harness now mandates --release and logs the effective flags. Remaining
~14% parity chase is recorded as FUTURE work in investigation/http-campfire-transfer/. Full detail:
`investigation/http-campfire-transfer/{README,PLAN,RESULTS,LEDGER}.md`.

## FFI-7 COMPLETE / CERTIFIED (retained / cross-thread callback lifetime)
Closure contract (as implemented and certified):
- Language: `retained_callback<sig[:E]>` is a first-class Strut value with SHARED callback
  identity, structurally distinct from `function<sig>` (dedicated TypeNodeKind), explicit
  `retained_callback(callable)` construction boundary, checked-error signature preserved,
  ordinary `function<...>` unchanged. No source-level destructive move -> no moved-from state.
- Direction A (Strut->C): OPAQUE digest-qualified C handle owning an atomic C refcount + one
  internal retained value; C `retain`/`release`/`invoke`; INVOKE PRECONDITION = caller already
  owns a live C ref (no acquire-from-unowned, no resurrection-from-zero, no registry, no
  lifetime lock across user code); a handle passed back into Strut is BORROWED (wrapper copies
  the internal value, never consumes the caller's C ref); final release destroys the handle
  and its internal value owner.
- Direction B (native->Strut): `native_callback<sig[:E]>` = fn + ctx + retain_ctx + release_ctx
  (infallible `R(*)(void*,args...)`; fallible `int32_t(*)(void*,args...,R*out,error*)`).
  retain_ctx EXACTLY once at shared-state construction; release_ctx EXACTLY once at final state
  death; ordinary Strut copies/assignments never re-trigger native retain/release.
- Fallible A: FFI-6 status + out value + caller-owned callback_error descriptor; invoke() backs
  the descriptor with PER-THREAD module TLS valid AFTER invoke() returns, INVALIDATED by the
  next retained-callback failure reusing the backing on that thread (caller MUST copy
  immediately); declared errors only (undeclared -> abort). Same-thread nested reentrancy is
  proven (inner copied before outer overwrite).
- Fallible B: wrapper copies the native descriptor IMMEDIATELY into strut_checked_error, validates
  the declared set (undeclared -> abort), normal Strut checked-error flow.
- Concurrency/reentrancy: cross-thread A (8 owned-ref workers + main owner release mid-run,
  4000 invokes), cross-thread B (second pthread, ownership counts exact), bounded reentrancy
  C->Strut->C->Strut, exact-once destruction at every layer; 10k sequential invoke + 10k
  retain/release stress; two-module header coexistence; no global registry.
- Local walls: GCC -Werror, Clang -Werror, ASan/UBSan CTest 33/33 each; regressions 307/307
  default and reactor. ABI stays 1.0; FFI-5/6 declarations unchanged (additive).
- FIVE-PLATFORM GATE (raw-log verified): run 37963612906 on main (0392905) all five SUCCESS:
  linux-x64-gcc, linux-x64-clang, linux-arm64-gcc, macos-arm64-appleclang, windows-x64-msvc.
  Raw AppleClang log: `strut_ffi_retained_tests ... Passed`, `strut_ffi_retained_b_tests ...
  Passed`, `strut_ffi_retained_f_tests ... Passed`, `strut_codegen_tests ... Passed`,
  "100% tests passed out of 33".
  Raw MSVC log: same retained B/A/F tests `Passed`, `strut_codegen_tests ... Passed`,
  "100% tests passed, 0 tests failed out of 33" (MSVC /WX fixes: wd5045/4820/4668/5105 + lib
  import names + C4267 casts).
- Implemented commits: 8387094 (value) -> ff36c0a (value certification) -> 9729ba3 (Direction A)
  -> 2c8920d (Direction B) -> 4b7ac1b (fallible A+B) -> b6783c2 (concurrency/reentrancy) ->
  2766008/a8c4e9a/e93545c/347a825/0392905 (handover + cross-platform fixes).
Retained_callback is a first-class Strut VALUE with shared callback identity, structurally
distinct from function<sig> (dedicated TypeNodeKind), explicitly constructed, checked-error
signature preserved, ordinary function<...> unchanged. Strut has no source-level destructive
move for these values: assignment/return/copy are value copies of the shared control-block
pointer, so there is no moved-from source state; C++ moves are implementation mechanics only.

Crossing the ABI in two directions, additive to FFI-5/6 (ABI version stays 1.0):

- DIRECTION A (Strut -> C): an exported retained_callback<sig> is an OPAQUE digest-qualified
  C handle owning an atomic C refcount + one internal retained_callback value. retain/release
  manage the C refcount (never the shared_ptr count); invoke requires the caller to ALREADY
  own a live C ref for the whole invocation (no acquire-from-unowned, no resurrection, no
  registry, no lifetime lock across user code). A C handle passed back into Strut is BORROWED:
  the wrapper copies the internal value (Strut shared-owner count may rise) and never consumes
  the caller's C ref. Final release destroys the handle -> releases its internal value owner.
- DIRECTION B (native -> Strut): native_callback<(args)->ret [: E]> carries fn + ctx +
  retain_ctx + release_ctx (R(*)(void*,args...), or int32_t(*)(void*,args...,R*,err*) when
  fallible) INTO an exported function; Strut constructs a retained_callback whose shared state
  calls retain_ctx EXACTLY ONCE at construction and release_ctx EXACTLY ONCE at final state
  death; ordinary Strut copies never re-trigger native retain/release (native origin drop 2->1,
  Strut global sole owner, invoke later, replacement -> refs 0, destroyed 1).
- FALLIBLE (FFI-6 status + out value + caller-owned callback error descriptor; no third ABI):
  Direction A invoke catches a Strut checked error and backs the descriptor with PER-THREAD
  module TLS strings valid AFTER invoke() returns until the next retained-callback failure that
  reuses the backing on that thread (caller MUST copy immediately); undeclared -> abort.
  Direction B wrapper copies the native descriptor IMMEDIATELY into strut_checked_error (valid
  only through the copy), validates the declared set (undeclared -> abort), normal Strut
  checked-error flow.
- CONCURRENCY/REENTRANCY: 8 owned-ref workers + main owner release mid-run (Direction A, 4000
  invokes); cross-thread native-backed invocation (Direction B); same-thread nested fallible
  TLS reentrancy (C->Strut->C->Strut) proving the inner descriptor copy survives the outer TLS
  overwrite; bounded general reentrancy. Exact-once at every ownership layer.
- no hidden global handle registry.

Local certification (ALL GREEN): GCC -Werror, Clang -Werror, ASan/UBSan CTest 33/33 each;
regressions 307/307 default and reactor.
Commit sequence: 8387094 (retained_callback value) -> ff36c0a (value certification/fix-forward)
-> 9729ba3 (Direction A opaque handle) -> 2c8920d (Direction B native-owned) -> 4b7ac1b
(fallible A+B) -> b6783c2 (concurrency/reentrancy closure).
REMAINING BEFORE FFI-7 COMPLETE: the mandatory final five-platform gate (Linux x64 GCC/Clang,
Linux ARM64, macOS ARM64 AppleClang, Windows x64 MSVC) with RAW-LOG proof that the retained
CTA tests (strut_ffi_retained_tests / _b_tests / _f_tests) actually executed on AppleClang and
MSVC. Then FFI-7 COMPLETE -> FFI-8.
## FFI-5 baseline (borrowed callbacks) - unchanged
Representation (extends the FFI-5 fn-ptr+context model; ONE consistent model):
- C ABI stays `ret (*)(void* context, args...)`; the callback TRAMPOLINE is unchanged.
- Language first: `retained_callback<sig[:E]>` is a first-class Strut VALUE (shared control
  block; Direction A additionally wraps it in an OPAQUE HANDLE, Direction B feeds it from a
  `native_callback` transport; see the FFI-7 implemented section above). No `std::function`,
  registry-global, or generic `free()` crosses the ABI.
- Ownership: `handle = <module>_ffi_cb_retain(handle)` (inc) / `release(handle)` (dec->destruct,
  deterministic; `release(NULL)` invalid host behavior). Retained handles may outlive the
  originating call and be invoked from a foreign thread; borrowed callbacks (FFI-5) unchanged.
- Cross-thread invocation: allowed; the trampoline runs the Strut callable on the calling thread
  with a documented execution-context rule (Strut cancellation/token binding + checked error
  containment: no unwinding into the foreign thread's C frame; fallible retained callbacks use the
  FFI-6 status/descriptor). Callback lifetime is SELF-CONTAINED and independent of any embedding
  runtime: a handle is valid from create/retain until the matching release(s) (deterministic
  destruction). It is NOT bound to a runtime/context shutdown -- that relationship is deferred to
  FFI-8 (embedding context lifecycle) and deliberately NOT invented here.
  INVOKE OWNERSHIP CONTRACT (closed, not racy): EVERY THREAD THAT MAY INVOKE A RETAINED HANDLE
  MUST ITSELF OWN A LIVE RETAINED REFERENCE FOR THE ENTIRE INVOCATION. Creation sets refcount=1;
  retain() requires the handle already live and adds one owned ref; release() releases one owned
  ref; invoke() may only be called while the caller owns a live ref (an internal RAII
  liveness-guard may be used for nested implementation lifetime, but it does NOT make an
  otherwise-unowned raw pointer safe against a concurrent final free -- acquisition from an
  unowned pointer would be use-after-free and is NOT supported; there is no
  resurrection-from-zero). Therefore a concurrent release by another owner cannot be the FINAL
  release while an invocation is in flight, because the invoker still owns a ref; final release
  thus destroys exactly when no invoke can be in progress. After final release the pointer is
  host-contract UB. No hazard/epoch/registry machinery is introduced (keeps explicit ownership,
  no hidden registry, deterministic lifetime). Concurrent invocation of the same handle is
  allowed where captured Strut state semantics permit; the handle protects LIFETIME only, never
  serializing user concurrency.
  ERROR BACKING LIFETIME (closed): the borrowed type/message views pointed to in the fallible
  callback descriptor must remain valid THROUGH the C callback return boundary and the immediate
  foreign-side copy. Backing is per-invocation frame storage owned around the native call (the
  trampoline writes into the frame, the callback returns a descriptor into it, the native wrapper
  copies descriptor contents, then the frame is destroyed) -- NEVER a single shared mutable
  buffer on the handle (concurrent-invocation race) and never stack-local std::string destroyed
  at return.
- Reentrancy: callback -> exported Strut -> same/native callback allowed for synchronous
  transient + retained handles; tested (native->Strut->native->Strut).
- No hidden global registry: handles are per-module opaque pointers (no g_handles table).
Semantics recorded; implementation + tests + five-platform gate as the immediate FFI-7 work.

## FFI-8 COMPLETE / CERTIFIED (C embedding API)
Opaque C embedding-surface: `stru_embed_context` create/destroy (destroy returns int;
BUSy whenever an invocation is in flight OR retained leases exist; context stays intact and
usable; succeeds after releases), load_source/load_file with STABLE logical module identity,
dynamic by-name invocation, and per-context native compile/dlopen lifecycle (no CLI spawn; no
C++ types across the boundary; no global last-error; errors returned with owner handles).
VALUES: bool/int/float/string/bytes + RETAINED (single-owner; module lease held while live;
survives successful reload; must be released before destroy) + CALLBACK (typed
int32_t(*)(void*, int32_t) -- the SUPPORTED borrowed signature; no fn-through-object-pointer
at the public ABI). Module ownership: ordinary results/errors copied into embedding-owned
storage (no module dependency); inactive modules with outstanding retained leases stay loaded
and unload exactly once after the last release; A->B->C reload keeps A's callbacks callable;
failed reload preserves the active module. BORROWED callbacks are direct-synchronous-callee
only (sema rejects alias/store/return/retain/forward/parenthesized-callee for export-C
function params; ordinary Strut callables unrestricted); retained conversion of a borrowed
param is structurally rejected; escape negatives load as structured SEMANTIC failures with
recovery. REENTRANCY: different-context and same-context bounded (depth 3) certified; reload
and destroy during an in-flight invocation are deterministically BUSY and the invocation
completes. DIAGNOSTICS: STRUT_EMBED_INVOKE_{ARITY,KIND,NOTFOUND,UNSUPPORTED} on category
INVOKE; checked Strut errors keep type/message/code; parse/semantic/load categories distinct.
FIVE-PLATFORM (run 38023309245 on cd7bdfc): linux-x64-gcc, linux-x64-clang, linux-arm64-gcc,
macos-arm64-appleclang (stru_embed_tests Passed 52.86s, "100% tests passed out of 34"),
windows-x64-msvc (stru_embed_tests Passed 93.26s). Local walls GCC/Clang/ASan-UBSan 34/34
each; regressions 307/307 default and reactor. Commits: 609dd9e/9e4ea70 (first layer),
fe13a0f (provenance+multictx+file), e4fed3b (bytes/unified-release/reload/threads),
5926c3c (outstanding values survive reload/destroy), c213b0f/bd8044c (CI fixes),
70ba97c (retained+lease), 95f0cea (diagnostics), 9e879ea (borrowed+reentrancy),
886d661 (indirect-escape audit + same-context), cd7bdfc (typed cb ABI + in-flight BUSY).
REMAINING: FFI-9 real consumers (Go/Python/Node/C#), THEN FFI-10, final ABI audit,
release certification, v0.0.5.

## FFI-9 COMPLETE / CERTIFIED (independent language consumers + installation)
Shared `libstrut_embed` (STRUT_EMBED_API; Windows dllexport when STRUT_EMBED_BUILD_SHARED)
plus four genuinely independent, executable consumers over the FFI-8 public C ABI, each with a
permanent CTest that BUILDS and EXECUTES (build and run must both pass; rc==0 AND ok marker;
SKIP_RETURN_CODE 77 only when its toolchain is absent).

CONSUMERS AND RUNNERS (tests/embed/):
- python/strut_consumer.py (ctypes; STRUT_EMBED_LIB absolute library), 9-PYTHON.
- go/strut_consumer.go + run_go_consumer.py (cgo; borrowed callback via a C trampoline to an
  //export goCbImpl fixed function; fail() panics so the guarded deferred context destroy always
  runs; no POSIX-only -ldl; libdir on PATH), 9-GO. Commits c4e840c, 7c624d8, 38f6abd, 1eda89b,
  08bc5d0.
- node/ (dependency-free N-API addon: addon.cc number/string/Buffer/Function inference
  marshalling, copy-before-release into JS-owned Buffers, structured thrown Errors,
  synchronous-rooted borrowed JS callback trampoline, explicit StrutFree single-owner release;
  binding.gyp declares include_dirs <(strut_include_dir) and (Windows) libraries
  <(strut_embed_lib) fed through node-gyp's documented npm_config_*->gyp-variable promotion;
  MSVC needs the declarative build config, not env CXXFLAGS/LDFLAGS/INCLUDE/LIB), 9-NODE.
  Commits 81d006c, 28f6a15? not needed).
- csharp/ (P/Invoke; NativeLibrary.SetDllImportResolver resolves libstrut_embed by absolute
  STRUT_EMBED_LIB; NativeError/value layouts mirror embed.h; cdecl borrowed delegate rooted via
  Marshal.GetFunctionPointerForDelegate; --smoke isolation path prints startup/native-load/
  create/destroy markers; runner pins `dotnet build -o` and executes the exact dll), 9-CSHARP.
  Commits 336a47a, 8eed13b, 30f6f90, 41d67b9.
- Common corpus across all four: add(20,22)=42, double_64 float addf=5.75 (float/bool are NOT
  ABI-exportable in Strut; STRUT_EMBED_VALUE_BOOL exists only as a dynamic value kind), greet
  string round-trip, echo_bytes Buffer equality [0x61,0,0x62,0xff], risky(-1) -> structured
  EmbedErr{type:'EmbedErr',message:'boom'}, borrowed callback apply_cb=211, retained make_rc(100)
  ->105 before AND after module reload (module lease), BUSY destroy rejected, explicit retained
  release then destroy==0.

FIVE-PLATFORM CONSUMER MATRIX —  25/25 PASS (run 38069818109 on 37dd55a; Windows MSVC raw:
  strut_embed_tests Passed 93.45s, python 3.62s, go 36.14s, node 49.99s, csharp 25.49s;
  linux-x64-gcc/linux-x64-clang/linux-arm64-gcc/macos-arm64-appleclang jobs green incl all
  consumers; the lone recurring red is the dogfood `Certify sanitized generated HTTP runtime`
  WebSocket stage -- intermittent `unexpected WebSocket upgrade response: b''`, ctest 38/38
  passed on the affected job, NOT reproduced in 4/4 local runs, cause unresolved, unrelated to
  FFI). Earlier runs: 38032885435 (FFI-8 consumers), 38036549178, 38042265629 (mac link/DYLD),
  38058744246 (mac C# armored runtime), 38063827372/38065965620/38067898898 (Windows fixes).

INSTALLATION / CERT (9-CERT): cmake install rules ship strut_embed (LIBDIR/BINDIR portability),
include/strut public headers (INCLUDEDIR, incl embed.h), the strut CLI, and share/strut/jsonic
runtime-support headers. Auto discovery order in codegen.cpp jsonic_include_dir():
STRUT_JSONIC_INCLUDE_DIR override -> executable-relative (build tree or installed
prefix/share/strut/jsonic) -> EMBEDDING-LIBRARY-relative (<libdir>/../share/strut/jsonic) via a
dladdr/GetModuleHandle anchor so host-loaded Python/Go/Node/.NET locate installed assets with no
env. Verified: staged prefix at /tmp/strut-prefix runs all four consumers against ONLY installed
artifacts (tools/ffi9_cert.py, 4/4 PASS), relocated prefix (cp A->B) runs Python+Go with the env
unset, installed CLI compiles+runs a real json.p without the override. Source loading compiles
native C++ with the configured native compiler (CXX env / STRUT_HOST_CXX) and REQUIRES the
runtime-support headers; documented, not interpreter-style.

OWNERSHIP SUMMARY (unchanged from FFI-7/FFI-8): embedding-owned strings/bytes/errors +
retained single-owner values released exactly once via strut_embed_value_free / -error_release;
borrowed callbacks synchronous, direct-callee-only, never escape; retained callbacks survive
reload through module leases and release before BUSY destroy; structured error categories and
stable INVOKE sub-codes; bool export restriction; Node JS-thread-only synchronous borrowed
callbacks; Go fixed-export borrowed-callback scope; Python ctypes consumers not ASan-certified
(native fixture remains ASan/UBSan clean; Python runs in ordinary builds).

Commits: 0df9e38 (shared lib + python), 15d483c/0016d7b/181c238 (embed export convention + MSVC
C2375), c4e840c/7c624d8/38f6abd/1eda89b/08bc5d0 (go), 81d006c + node runner fixes (node),
336a47a/8eed13b/30f6f90/b2472fe/c179f51/41d67b9 (csharp), 5ec725d (install rules + tools/ffi9_cert.py),
d97bd0c/30f6f90 (library-relative discovery + resolver), 424c64b/753088d (CI provisioning:
arm64 .NET + node-gyp + x64 .NET), 9a1d7f7/37dd55a (declarative node-gyp include + link via
npm_config promotion; MSVC env-inheritance gap). Local walls 39/39 GCC+embed; Nift integration
d8a1165 (see FFI-10 below).
REMAINING: FFI-10 direct Nift<->Strut language-level calls, final ABI audit, release
certification, v0.0.5.

## Release sequence (after this dev line)
FFI-7 -> FFI-8 -> FFI-9 consumers -> FFI-10 Nift dogfood -> final ABI audit -> full certification
-> v0.0.5 release. v0.0.4 immutable; no v0.0.5 tag until the full FFI roadmap is complete.

## FFI-6 COMPLETE (final)
- Normal checked functions: bidirectional. Multiple checked errors: bidirectional concrete
  identity. Unknown foreign error: explicit fatal contract violation. **Synchronous checked
  callbacks: bidirectional.** Error ownership: explicit (opaque handle for exports, borrowed
  caller-owned descriptor for callbacks). Generated export-side exception containment:
  complete (incl. error-handle allocation). Foreign `extern "C"` implementations:
  contractually forbidden from throwing C++ exceptions. ABI version 1.0. Cross-platform
  certified (runs `37822732197`, `37825915909`, `37833884102`; fallible-callback certification
  recorded below).

## FFI-5 status (synchronous borrowed callbacks — COMPLETE, cross-platform certified)
- **Cross-platform certification run `37810967154` (commit `3548707`): all five jobs green,
  with `strut_ffi_callback_tests` and `strut_ffi_callback_bidir_tests` demonstrably executed on
  every platform** — macOS ARM64 AppleClang (3.84s / 3.24s), Windows x64 MSVC (2.62s / 2.47s),
  linux-x64-gcc (2.04s / 1.95s), plus linux-x64-clang and linux-arm64-gcc.
- **One C callback representation, both directions.** A Strut function value
  `function<(args)->ret>` (canonical spelling) lowers to a C function pointer whose first
  parameter is an opaque `void* context`: `ret (*)(void* context, args...)`. The C++ closure
  type (`std::function`) NEVER crosses the ABI. Generated typedef is
  `strut_ffi_<module>_cb_<sigdigest>` (signature-derived, deterministic).
- **Initial surface:** primitive `int/uint` (fixed-width) and `double_32`/`double_64`
  parameters; primitive-or-`void` return; synchronous; same-thread; **borrowed for the
  duration of the foreign call only**. Checked-error callbacks (FFI-6), async/`future`
  callbacks (FFI-7), callback returns, and non-primitive callback parameters/returns are
  rejected with specific diagnostics.
- **native -> Strut (A):** an exported function may take a callback; the wrapper builds a
  `std::function` capturing the C fn pointer + context and invokes it synchronously. Certified
  by `strut_ffi_callback_tests` (repeated invocation, context round-trip, C + C++ hosts,
  pure-C typedef).
- **Strut -> native (B):** an `extern "C"` declaration may take a callback; the compiler emits
  a type-erased trampoline template `strut_ffi_cb_trampoline<R,A...>` and passes a
  stack-scoped `std::function` context (the call is wrapped in an IIFE so the context is an
  lvalue alive for the call). Certified by `strut_ffi_callback_bidir_tests` (capturing lambda
  `x => x*base`, base=3 => `21`).
- **Calling convention:** ordinary platform C calling convention throughout; the generated
  typedef, trampoline, and native fixture declaration all use it (no `__stdcall`).
- **Exception containment at the callback boundary (temporary policy):** the trampoline
  `catch(...)`es and calls `std::abort()` (deterministic process failure) — an unexpected
  Strut/C++ exception never unwinds through native C frames, and is not silently turned into a
  fabricated result. Structured propagation is deferred to FFI-6; this is not the FFI-6
  contract.
- **Lifetime contract (prominent):** callbacks and their context are **borrowed for the
  duration of the foreign call only** — native code MUST NOT retain the function pointer or
  context for later use; retained/cross-thread callbacks are FFI-7.
- Strict local wall: CTest **26/26** normal + GCC/Clang -Werror + ASan/UBSan; regressions
  **307/307** default + reactor; ASan callback run clean.
- **Next: FFI-6** (checked-error propagation both directions + host error retrieval) or the
  campaign's next evidenced step.
