# Strut safe-memory contract

Safe Strut uses deterministic reference-counted `ptr<T>`, non-owning `ref<T>`, and `weak_ptr<T>`. It has no tracing garbage collector.

Safe code is intended to prevent dangling safe references, use-after-free through safe pointers, double destruction, unchecked null safe-pointer dereference, and invalid ownership upgrades. `ptr<T>` dereference is runtime-checked and traps with a Strut runtime error if the pointer is empty. `weak_ptr<T>.lock()` is the only supported upgrade path from weak ownership and may produce an empty `ptr<T>` after expiry.

`unsafe { ... }` is an explicit opt-out boundary. `raw_ptr<T>`, raw pointer arithmetic and raw dereference are not covered by the safe-memory guarantees. Unsafe code is responsible for lifetime, bounds and aliasing correctness.

Reference-count cycles are not garbage collected. Use `weak_ptr<T>` for non-owning back-references. The compiler warns for simple statically visible strong cycles.

## Certification

Configure with `-DSTRUT_SANITIZERS=ON` under GCC/Clang to run compiler/runtime tests with AddressSanitizer and UndefinedBehaviorSanitizer. The independent regression suite also carries negative lifetime/null/weak-pointer cases.

Dynamic-array element borrows are currently rejected because later growth could invalidate their address. This conservative rule can be relaxed only when the compiler can prove the reference remains valid.
