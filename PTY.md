# Pseudo-terminal API

P8 extends the P7 binary pseudo-terminal independently of HTTP and WebSockets:

```strut
pty_spawn(string program, string[] args) -> pty : PtyError
pty_spawn(string program, string[] args, json options) -> pty : PtyError
pty_spawn(string program, string[] args, cancellation_token token) -> pty : PtyError
pty_spawn(string program, string[] args, json options, cancellation_token token) -> pty : PtyError
```

`options` accepts `cwd`, string-valued `env`, `rows`, and `columns`; dimensions default to 24 by 80. On supported platforms, `cwd` is applied atomically by the native `posix_spawn` chdir file action. Launch is argv-safe and does not interpret the requested program or arguments as shell text. The stream is opaque binary data with merged stdout/stderr and no public native descriptor.

The complete method surface is:

```strut
terminal.read_bytes(int_64 max_bytes) -> bytes : PtyError
terminal.write_bytes(bytes data) -> void : PtyError
terminal.eof() -> bool
terminal.resize(int rows, int columns) -> void : PtyError
terminal.interrupt() -> void : PtyError
terminal.terminate() -> void : PtyError
terminal.kill() -> void : PtyError
terminal.hangup() -> void : PtyError
terminal.wait() -> int : PtyError
terminal.running() -> bool
terminal.exit_code() -> int
terminal.close() -> void
```

`resize` accepts dimensions from 1 through 65535 and applies `TIOCSWINSZ` to the master. The kernel alone delivers any resulting `SIGWINCH`; Strut does not send a duplicate signal. `interrupt`, `terminate`, `kill`, and `hangup` map to `SIGINT`, `SIGTERM`, `SIGKILL`, and `SIGHUP`. Immediately before each signal, the runtime uses a non-reaping `waitid(..., WNOWAIT)`, verifies that the pinned leader still owns its session with `getsid`, and verifies the master still belongs to that session with `tcgetsid`. Once that terminal-session identity matches, the kernel's `tcgetpgrp` result is authoritative even if no process currently has a PID equal to the foreground PGID; Strut does not incorrectly pass a PGID to `getsid`. If no usable foreground PGID is available, the still-pinned leader group is used after `getpgid` validation. An already observed exit or `ESRCH` from the final group signal is successful no-op completion. POSIX has no atomic `tcgetpgrp`-plus-`killpg` operation, so foreground membership can still change in that final syscall race.

Handles are copyable views of shared ownership. One reader and one writer can run concurrently; a duplicate same-direction operation fails. Reads are pull-based, writes synchronously complete or fail under kernel backpressure, and there is no PTY queue. `resize`, signal, close, wait, and status races are serialized around shared descriptor and lifecycle state. Closing through any copy is idempotent, wakes active I/O, closes the master exactly once, and performs bounded TERM/KILL cleanup of the pinned leader group. Master HUP, EOF, and Linux `EIO` all converge on EOF. Exactly one lifecycle owner reaps the leader, so concurrent waits return one cached status and no zombie is left.

The spawn token remains the only cancellation model. Cancellation wakes blocked read and write operations with `PtyError("PTY I/O cancelled", 125)` and blocked `wait()` with `PtyError("PTY wait cancelled", 125)`; it does not signal the child. Completion already observed and cached wins over cancellation. `close()` publishes `close_requested` under the lifecycle mutex before it begins I/O shutdown. Once published, close cleanup wins over concurrent cancellation, cancellation no longer short-circuits the cleanup polling interval, and waiters receive the final cached status. If cancellation is observed first while the child is still running, the wait is cancelled and releases lifecycle ownership so a later close can reap normally.

Closing the master lets the kernel hang up the current foreground group. Bounded explicit cleanup can safely target only the session leader's original process group. POSIX provides no signal-by-session operation: background groups outside the leader group can survive normal wait/cleanup, and deliberately detached groups can escape completely. Applications needing containment beyond this limitation need an external supervisor/cgroup. External SIGCHLD reaping of PTY children is unsupported; detected ownership loss closes safely without signalling a potentially reused PID.

Linux PTY support requires glibc 2.34 or newer, where `POSIX_SPAWN_SETSID`, `posix_spawn_file_actions_addclosefrom_np`, and `posix_spawn_file_actions_addchdir_np` are available. Darwin applies spawn file actions before the new session can acquire a controlling terminal, so macOS starts a fixed `/bin/sh` launcher as the native-spawned session leader. The launcher opens the slave through positional parameters and immediately `exec`s the parent-resolved target; requested values are never interpolated into shell source, the target retains the launcher's PID/session identity, and there is still no post-fork application path. The public functional fixture executes in macOS ARM64 CI. Older glibc versions and other POSIX libcs compile a deterministic unsupported path; launch never enumerates descriptors or falls back to post-fork child code. On Windows, spawn, read, write, resize, signal, and wait operations throw `PtyError("PTY unsupported on Windows until P9 ConPTY")`; `eof()` returns true, `running()` returns false, `exit_code()` returns -1, and `close()` is a no-op. P9 remains responsible for ConPTY; P8 adds no Windows terminal implementation, WebSocket integration, or terminal-emulation dependency.
