# Concurrency hardening

Strut's safe concurrency surface currently consists of native `thread`, shared `mutex`, typed `channel<T>`, async functions/lambdas, futures, unified cancellation and the global multithreaded executor.

## Guarantees

- `mutex.lock(() => { ... })` releases the lock when the callable exits normally or throws.
- `channel<T>` serializes queue state, wakes blocked receivers on send/close, rejects sends after close, and returns an empty optional after a closed channel drains.
- executor queue state is mutex-protected; shutdown drains already queued work before worker threads join.
- exceptions in native worker threads are captured and rethrown at `join()`; async exceptions are rethrown by `await`.
- direct `T&` values cannot cross a native thread boundary. Owning values must be used when lifetime extends across the boundary.

## Cancellation

`cancellation_source` and `cancellation_token` share reference-counted state without raw lifetime coupling. Cancellation is monotonic and idempotent. Observation is atomic; `wait()` uses a condition variable rather than polling. Internal native-operation subscriptions are registered and removed under the state lock, but callbacks execute outside it. Removal synchronizes with an already-running callback so captured operation state cannot be destroyed prematurely.

There is no global or thread-local current token. Operations receive an explicit copied token through their owning context, allowing unrelated work to cancel independently. Source destruction is not cancellation.

Process pipes bind that context through `process(program, args, token)` while preserving the existing read/write signatures. Each active operation owns its wake object. Closing an endpoint marks shared endpoint state and wakes the operation, but native descriptor destruction is deferred until the operation releases it; cancellation callbacks never close descriptors. Only one operation may use a particular pipe endpoint at a time, while stdin, stdout, stderr, and unrelated processes can block independently.

On POSIX, process descriptors are nonblocking and wait with `poll` beside a nonblocking close-on-exec wake pipe. SIGPIPE remains blocked only in the writing thread around the native write, with no coordination mutex held across `poll` or write progress. On Windows, parent pipe endpoints are overlapped named-pipe handles and wait on the I/O completion event plus an operation wake event; cancellation uses `CancelIoEx` for the exact overlapped request. Completion is checked before cancellation when both events are ready.

## Stress policy

The repository includes generated Strut programs that exercise 1,000 channel messages, concurrent mutex-protected mutation, async scheduling, copied cancellation tokens, 32 simultaneous waiters, 8 concurrent cancellers, blocked process reads and pipe-capacity-blocked writes, independent operation tokens, close/cancel wakeups, EOF and completion. Certification runs the resulting native executables repeatedly. ThreadSanitizer should also be used where the host toolchain/runtime supports it; sanitizer availability is not assumed on every CI image.
