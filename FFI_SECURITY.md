# FFI and unsafe boundary

Strut's C ABI surface is intentionally explicit:

```strut
extern "C" function c_read(raw_ptr<int> value) -> int;
```

Calling an external C function requires `unsafe { ... }`. ABI-compatible primitives, `raw_ptr<T>` and plain-layout structs can cross the boundary. Safe owning `ptr<T>` and `ref<T>` are Strut runtime concepts and are not silently reinterpreted as C pointers; convert to `raw_ptr<T>` explicitly inside `unsafe` when necessary.

The FFI torture fixture covers signed integers, `double_64`, structs by value, structs returned by value, const-style reads through raw pointers, mutation through raw pointers, and both static and dynamic native libraries.

## Responsibility boundary

Safe-memory guarantees stop at the `unsafe` boundary. C code can retain a raw pointer after the Strut owner dies, write outside an allocation, violate aliasing, or free memory incorrectly. Strut does not pretend to make such code safe. Prefer narrow wrappers that immediately translate native results back into safe Strut values.

Native library paths and ABI versions are deployment inputs. The package/build layer must never assume a host library is ABI-compatible with a cross target.
