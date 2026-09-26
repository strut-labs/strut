# Concurrency hardening

Strut's safe concurrency surface currently consists of native `thread`, shared `mutex`, typed `channel<T>`, async functions/lambdas, futures and the global multithreaded executor.

## Guarantees

- `mutex.lock(() => { ... })` releases the lock when the callable exits normally or throws.
- `channel<T>` serializes queue state, wakes blocked receivers on send/close, rejects sends after close, and returns an empty optional after a closed channel drains.
- executor queue state is mutex-protected; shutdown drains already queued work before worker threads join.
- exceptions in native worker threads are captured and rethrown at `join()`; async exceptions are rethrown by `await`.
- direct `T&` values cannot cross a native thread boundary. Owning values must be used when lifetime extends across the boundary.

## Cancellation

Strut does not currently expose task cancellation. That is deliberate: there is therefore no partial cancellation contract to race against shutdown. A future cancellation feature must define cooperative cancellation points and receive its own race/stress certification before becoming part of the safe surface.

## Stress policy

The repository includes a generated Strut program that exercises 1,000 channel messages, concurrent mutex-protected mutation and async scheduling. Certification runs the resulting native executable repeatedly. ThreadSanitizer should also be used where the host toolchain/runtime supports it; sanitizer availability is not assumed on every CI image.
