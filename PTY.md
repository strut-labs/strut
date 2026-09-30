# Pseudo-terminal API

P7 provides a binary pseudo-terminal independently of HTTP and WebSockets:

```strut
pty_spawn(string program, string[] args) -> pty : PtyError
pty_spawn(string program, string[] args, json options) -> pty : PtyError
pty_spawn(string program, string[] args, cancellation_token token) -> pty : PtyError
pty_spawn(string program, string[] args, json options, cancellation_token token) -> pty : PtyError
```

`options` accepts `cwd`, string-valued `env`, `rows`, and `columns`; dimensions default to 24 by 80. On supported platforms, `cwd` is applied atomically by the native `posix_spawn` chdir file action. Launch is direct and argv-safe, with no implicit shell. Methods are `read_bytes(int_64)`, `write_bytes(bytes)`, `eof()`, `wait()`, `running()`, `exit_code()`, and `close()`. The stream is opaque binary data with merged stdout/stderr and no public native descriptor.

Handles are copyable views of shared ownership. One reader and one writer can run concurrently, duplicate same-direction operations fail, cancellation interrupts blocked I/O with `PtyError` code 125, and close is shared and idempotent. Closing the master lets the kernel hang up the terminal's current foreground process group without an unsafe numeric-PGID signal. Bounded TERM/KILL cleanup targets only the session leader's original process group after a WNOWAIT identity probe. Background groups outside that leader group and deliberately detached groups can escape POSIX containment. Exactly one lifecycle owner reaps the child; `wait()` does not hold the lifecycle mutex while using native wait operations, concurrent close wakes it, and later waits return cached status or a deterministic cached error. External SIGCHLD reaping of PTY children is unsupported; detected ownership loss closes safely without signalling a potentially reused PID.

Linux PTY support requires glibc 2.34 or newer, where `POSIX_SPAWN_SETSID`, `posix_spawn_file_actions_addclosefrom_np`, and `posix_spawn_file_actions_addchdir_np` are available. macOS uses its native equivalents and is included in macOS ARM64 CI. Older glibc versions and other POSIX libcs compile a deterministic `PtyError` unsupported path, even when `cwd` is omitted; PTY launch never enumerates descriptors or falls back to post-fork child code. Windows exposes a warning-clean P7 stub with the message `PTY unsupported on Windows in P7`. P8 will add resize and public signal delivery. P9 will add ConPTY.
