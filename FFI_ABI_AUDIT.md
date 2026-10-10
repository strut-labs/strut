# Strut v0.0.5 public ABI / source-compatibility audit

Authoritative, concise record of what the 0.0.5 dev line actually guarantees for foreign
interoperability. Companion to `FFI_HANDOVER.md` (FFI-1..10 sections retain the full campaign
evidence, commit IDs and CI runs).

## 1. Executive assessment

- The public embedding interface (`include/strut/embed.h`) is a **pure C header** with an
  opaque-context, value/error ABI; it is the stable public contract that four external language
  runtimes (Python ctypes, Go cgo, Node N-API, C# P/Invoke) consumed and executed on **five
  platforms (25/25, real execution, run 38069818109)**.
- FFI-1..7 typed C exports / callbacks and FFI-9 installation behaviour remain certified and
  unchanged by the FFI-10 work.
- FFI-10 (Nift) demonstrated bidirectional integers, UTF-8 strings and arbitrary binary values
  through **existing public interfaces of both languages**; it is **functional and Linux-local
  certified only** (4/4). Nift core untouched (workspace boundary).
- One bridging fixture (`strut_string { std::string v; }`) is a **private compiler-coupled C++
  representation** and is explicitly NOT part of any public C ABI. Production byte boundaries
  use C-compatible `raw_ptr<uint_8>` + explicit length.
- Release readiness is **BLOCKED** by the un-resolved recurring WebSocket dogfood certification
  failure; nothing else in this audit currently prevents release.

## 2. Public ABI inventory

Public exported surfaces:

| Surface | Kind | Notes |
|---|---|---|
| `strut_embed_context_create/destroy` | C | destroy returns int; BUSY while in-flight or retained leases hold |
| `strut_embed_context_load_source/load_file` | C | compile+dlopen in-process; needs native C++20 compiler + runtime-support headers |
| `strut_embed_invoke` / `strut_embed_retained_invoke` | C | dynamic, by-name; typed retained-callback entry |
| `strut_embed_value_free` / `strut_embed_error_release` | C | single-owner release; zeroed on release; error routing via ownership tag |
| `strut_embed_value` struct | C | kind/int64/double/bool/{data,len}/retained/cb_fn/cb_ctx; layout verified from C# mirrors |
| `strut_embed_error` struct | C | category/owner/type/message/code; stable INVOKE sub-codes 1..4 |
| `strut_embed_callback_i32_fn` | C | typed borrowed callback `int32_t(*)(void*,int32_t)` |
| Generated typed C exports (FFI-1..4) | C | fixed-width ints/floats, POD aggregates, borrowed raw_ptr/ref to primitives |
| Generated string/bytes exports (FFI-2) | C | pointer+length transport; validated empirically via Nift `(cstr,u64)`/`(buffer,u64)` |
| `extern "C"` forward FFI (FFI-1..10) | depends | must be genuinely C-compatible at the declared signature |

Installed SDK layout: `include/strut/*` (INCLUDEDIR), `lib/libstrut_embed.*` (LIBDIR/BINDIR),
`bin/strut`, `share/strut/jsonic` (runtime support); library-relative discovery via dladdr /
GetModuleHandle anchor; `STRUT_JSONIC_INCLUDE_DIR`, `STRUT_LDFLAGS`, `STRUT_CXXFLAGS`, `CXX`
documented overrides.

## 3. Compatibility findings

- `embed.h` compiles as C (`-std=c11`); `STRUT_EMBED_API` is `dllexport`-only when the DLL is
  built (MSVC C2375 fixed by exporting only under `STRUT_EMBED_BUILD_SHARED`).
- Fixed-width type mappings (int32/int64/size_t/void*) verified in the C# consumer on five
  platforms.
- `bool` is **not** an exportable typed-FFI type (embedding value kind BOOL exists; a typed bool
  export is rejected) - intentional restriction, preserved.
- FFI-5 borrowed-callback forwarding restriction (direct-callee only) preserved.
- FFI-7 retained single-owner + module lease across reload preserved; BUSY destroy recoverable.
- **Potential-risk flag (no defect demonstrated):** any future `extern "C"` fixture that passes a
  C++ wrapper by value (like the FFI-10 `strut_string`) is a *private*, not public, contract.
  Public-facing string/byte signatures must stay pointer+length.

## 4. Ownership / lifetime findings

- Embedding-owned strings/bytes/errors: freed via the correct release function exactly once;
  MUST be released before context destroy when module-owned.
- Borrowed callbacks: synchronous, direct-callee, must never escape the native call.
- Retained callbacks: single owner; release exactly once; callbacks remain callable after
  successful reload; leaks block destroy via BUSY.
- Nift temporary `ffi_open` handles auto-release at evaluation completion
  (`erase_new` + `FfiLibraryInstance` destructor); explicit `ffi_close` unreachable in the
  expression evaluator (documented).
- Sanitizer coverage: native C embedding ASan/UBSan-clean; Python/Go/Node/C# consumers execute
  in ordinary builds (not claims of interpreter-hosted ASan certification).

## 5. Cross-platform verification matrix (real execution)

| Consumer | Linux GCC | Linux Clang | Linux ARM64 | macOS ARM64 | Windows MSVC |
|---|---|---|---|---|---|
| Native C (strut_embed_tests) | PASS | PASS | PASS | PASS | PASS |
| Python ctypes | PASS | PASS | PASS | PASS | PASS |
| Go cgo | PASS | PASS | PASS | PASS | PASS |
| Node N-API | PASS | PASS | PASS | PASS | PASS |
| C# P/Invoke | PASS | PASS | PASS | PASS | PASS |
| Installed staging (9-CERT) | PASS (4/4, relocated-prefix) | - | - | - | - |
| Nift bidirectional (FFI-10) | PASS 4/4 | - | - | - | - |

FFI-10 is Linux-local only; no five-platform FFI-10 claim is made.

## 6. Remaining restrictions

- `bool` typed export unsupported; hex integer literals unsupported in this Strut parser
  version (decimal bytes used); `\uXXXX` escapes not decoded by Nift scripts (real UTF-8 bytes
  must be embedded); UTF-8 string lengths are BYTES.
- Interpreter/runtime-hosted consumers not ASan-certified.
- **WebSocket dogfood blocker**: intermittent `unexpected WebSocket upgrade response: b''` in
  `Certify sanitized generated HTTP runtime`; ctest 38/38 passes on the affected job; not
  reproduced in 4/4 local runs; cause unresolved. Must be resolved with server-side evidence
  before overall release approval.

## 7. Validation evidence

- FFI-9 five-platform run **38069818109** (Windows raw: embed 93.45s, python 3.62s, go 36.14s,
  node 49.99s, csharp 25.49s).
- Local relgcc GCC wall **42/42** currently; regressions 307/307 (default + reactor) historically
  green; a transient mass-failure was recorded as root-cause-unconfirmed (build-then-ctest
  ordering now enforced).

## 8. Confirmed defects fixed during the audit window

None discovered in the public C contracts during this audit; all corrections during the campaign
(MSVC strongly_canonical, node-gyp declarative include/link, macOS arm64 .NET provisioning,
C# resolver, boolean-negative/JSON-decoder fixtures) were implementation/test-layer, not public
ABI changes.

## 9. Blocking items

1. WebSocket dogfood certification failure (unresolved).
2. Full release certification run (pending that resolution).