# CP95 reference-count optimisation

The first Strut-specific ownership optimisation targets function parameters. A `ptr<T>` parameter that is not reassigned by the callee is emitted as a borrowed `const std::shared_ptr<T>&` in the bootstrap backend instead of a by-value `std::shared_ptr<T>`. That preserves source semantics for read-only parameters while avoiding an otherwise mandatory atomic reference-count increment/decrement on every call. Parameters that are reassigned remain by-value owners.

This is deliberately conservative: it does not change explicit Strut `ptr<T>` copies, returned ownership, stored ownership, or parameters whose bodies assign to the binding. The optimisation is verified in codegen tests so later compiler changes cannot silently reintroduce the copy.

`ptr_param.cpp` provides a host-level before/after model using non-inlined shared-pointer calls. `ptr_param.cp95.txt` records the current development-host result. `ptr_baseline.cpp` remains the lower-level copy-cost baseline. These microbenchmarks are for regression/profiling guidance, not public cross-language claims.

Hot Strut-owned runtime helpers continue to accept collections/pointers by reference where ownership is not needed. More advanced escape analysis can build on this rule later rather than changing language semantics now.
