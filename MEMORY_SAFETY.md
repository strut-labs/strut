# Strut safe-memory contract

Safe Strut uses deterministic reference-counted `T*`, non-owning `T&`, and `weak_ptr<T>`. It has no tracing garbage collector.

Safe code is intended to prevent dangling safe references, use-after-free through safe pointers, double destruction, unchecked null safe-pointer dereference, and invalid ownership upgrades. `T*` dereference is runtime-checked and traps with a Strut runtime error if the pointer is empty. `weak_ptr<T>.lock()` is the only supported upgrade path from weak ownership and may produce an empty `T*` after expiry.

`unsafe { ... }` is an explicit opt-out boundary. `ptr<T>`, raw pointer arithmetic and raw dereference are not covered by the safe-memory guarantees. Unsafe code is responsible for lifetime, bounds and aliasing correctness.

Reference-count cycles are not garbage collected. Use `weak_ptr<T>` for non-owning back-references. The compiler warns for simple statically visible strong cycles.

## Certification

Configure with `-DSTRUT_SANITIZERS=ON` under GCC/Clang to run compiler/runtime tests with AddressSanitizer and UndefinedBehaviorSanitizer. The independent regression suite also carries negative lifetime/null/weak-pointer cases.

Dynamic-array element borrows are currently rejected because later growth could invalidate their address. This conservative rule can be relaxed only when the compiler can prove the reference remains valid.

## Thread/data-race policy (CP64)

Strut's safe ownership primitives make object lifetime safe across threads, but they do not make arbitrary mutable object access race-free. `T*` reference-count operations are safe to copy across threads; access to the referenced mutable `T` must still be synchronized when multiple threads can touch it concurrently. Use `mutex` or `channel<T>` to establish that synchronization.

Direct `T&` arguments are rejected at `thread(...)` boundaries because a borrow does not own its lifetime and could outlive or race its source. Immutable values may be copied freely. `unsafe` and `ptr<T>` deliberately opt out of Strut's race/lifetime guarantees.

The compiler should diagnose provable unsafe sharing patterns, but CP64 does not claim whole-program race freedom. The runtime promises deterministic mutex/channel synchronization and thread-safe `T*` lifetime accounting; user code remains responsible for synchronizing shared mutable state.

## Recursive pointer types

Pointer and reference types compose rather than stopping at one level. `T**` is a safe reference-counted pointer to a safe `T*`; `T***` continues recursively; `T*&` is a non-owning reference to a pointer binding. Pointers to references (`T&*`) and references to references are rejected. Raw pointers use `ptr<T>` at every unmanaged level and remain restricted to `unsafe`.
