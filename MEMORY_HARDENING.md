# Memory/runtime hardening

Safe Strut ownership is deterministic reference counting with `T*`, non-owning `weak_ptr<T>`, and non-owning non-null `T&`. The current bootstrap maps owning/weak pointers to the C++ standard library's `std::shared_ptr` / `std::weak_ptr` control block.

## Refcount exhaustion policy

Strut code must never observe ownership count wraparound as object destruction. An implementation that cannot acquire another ownership reference may fail the operation or terminate rather than silently wrapping the count. The bootstrap delegates the physical counter to `std::shared_ptr`; forcing a real overflow is not practical and is not claimed as a tested condition.

## Weak-pointer races

`weak_ptr<T>.lock()` is the only operation that promotes a weak observation to temporary ownership. Concurrent destruction of the final `T*` and a weak lock must result in either a live owning pointer or an expired result, never a dangling pointer. The bootstrap inherits the thread-safe control-block operations of `std::shared_ptr`/`std::weak_ptr` and includes a generated-program stress test for this path.

## Sanitizers

Compiler/runtime hardening uses AddressSanitizer and UndefinedBehaviorSanitizer on supported Clang/GCC hosts. The generated-runtime stress fixture combines `T*`, `weak_ptr<T>`, native threads and repeated lock/destruction transitions. Valgrind is useful when available but is not a required dependency and was not available in the current development container.
