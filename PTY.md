# Pseudo-terminal API

P9 provides one binary pseudo-terminal API across POSIX PTYs and Windows ConPTY, independently of HTTP and WebSockets:

```strut
pty_spawn(string program, string[] args) -> pty : PtyError
pty_spawn(string program, string[] args, json options) -> pty : PtyError
pty_spawn(string program, string[] args, cancellation_token token) -> pty : PtyError
pty_spawn(string program, string[] args, json options, cancellation_token token) -> pty : PtyError
```

`options` accepts `cwd`, string-valued `env`, `rows`, and `columns`; dimensions default to 24 by 80. POSIX applies `cwd` atomically through the native `posix_spawn` chdir file action, and Windows passes it directly to `CreateProcessW`. Launch is argv-safe and does not interpret the requested program or arguments as shell text. Standard input, output, and error share one terminal stream with no public native descriptor. POSIX exposes the terminal byte stream directly. ConPTY carries UTF-8/virtual-terminal traffic through byte pipes, so arbitrary non-UTF-8 data, NUL, control bytes, and exact CR/LF round trips are not portable Windows behavior.

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

`resize` accepts dimensions from 1 through 32767 on every platform. POSIX applies `TIOCSWINSZ`; the kernel alone delivers any resulting `SIGWINCH`. POSIX `interrupt`, `terminate`, `kill`, and `hangup` map to `SIGINT`, `SIGTERM`, `SIGKILL`, and `SIGHUP`. Immediately before each signal, the runtime uses a non-reaping `waitid(..., WNOWAIT)`, verifies the pinned leader and terminal session, and then targets the current foreground group or validated leader group. An already observed exit or final `ESRCH` is successful no-op completion. POSIX has no atomic foreground-query-plus-signal operation, so foreground membership can still change in the final syscall race.

Windows controls are terminal and lifecycle approximations, not POSIX signal emulation. `resize` calls `ResizePseudoConsole`. `interrupt` writes ETX (`0x03`) through terminal input and depends on the attached application's console mode. `hangup` closes terminal input. `terminate` closes output and initiates pseudo-console close, so it prioritizes bounded shutdown over preserving unread final output. `kill` immediately terminates the owned Job and reports status 137. Normal direct-child completion also terminates remaining Job members. `wait`, `running`, and `exit_code` cache the direct child's native exit status.

Handles are copyable views of shared ownership. One reader and one writer can run concurrently; a duplicate same-direction operation fails. Reads are pull-based, writes synchronously complete or fail under kernel backpressure, and there is no PTY queue. `resize`, control, close, wait, and status races are synchronized. POSIX close performs bounded TERM/KILL cleanup and exactly one lifecycle owner reaps the leader. Windows assigns the suspended child to a kill-on-close Job before resume. A weak monitor owns only a duplicated process handle, caches direct-child completion, and terminates remaining Job members without retaining PTY state. Dropping the last public copy still cleans up a live child. A pending read independently observes completion and initiates pseudo-console close while it drains final output. `ClosePseudoConsole` runs off-thread because it can block during that drain on Windows versions before build 26100; explicit close and terminate retire output first, and worker setup failure does the same before synchronous fallback.

The spawn token remains the only cancellation model. Cancellation wakes blocked read and write operations with `PtyError("PTY I/O cancelled", 125)` and blocked `wait()` with `PtyError("PTY wait cancelled", 125)`; it does not signal the child. A token already cancelled when I/O starts fails before a native read or write. Completed I/O and cached process completion win over later cancellation. If close and cancellation are both pending when blocked I/O resumes, cancellation wins; close alone raises `PtyError("PTY is closed")`. Once close lifecycle cleanup starts, a concurrent wait proceeds to the final cached status rather than using cancellation to bypass cleanup.

Closing the master lets the kernel hang up the current foreground group. Bounded explicit cleanup can safely target only the session leader's original process group. POSIX provides no signal-by-session operation: background groups outside the leader group can survive normal wait/cleanup, and deliberately detached groups can escape completely. Applications needing containment beyond this limitation need an external supervisor/cgroup. External SIGCHLD reaping of PTY children is unsupported; detected ownership loss closes safely without signalling a potentially reused PID.

Linux PTY support requires glibc 2.34 or newer, where `POSIX_SPAWN_SETSID`, `posix_spawn_file_actions_addclosefrom_np`, and `posix_spawn_file_actions_addchdir_np` are available. Darwin native-spawns the current executable as a hidden, early-dispatched session launcher because its spawn file actions run before the new session can claim a controlling terminal. The launcher validates identity and inherited descriptors, claims the slave, restores standard descriptors, reports setup failures through a close-on-exec channel, and immediately `execve`s the parent-resolved target. There is no post-fork application path or shell interpretation. Older glibc versions and other POSIX libcs use a deterministic unsupported path.

Windows support requires ConPTY from Windows 10 version 1809/build 17763 or Windows Server 2019. The runtime resolves `CreatePseudoConsole`, `ResizePseudoConsole`, and `ClosePseudoConsole` dynamically so an older system still starts normally; `pty_spawn` then raises `PtyError("ConPTY requires Windows 10 version 1809 or newer")`. ConPTY uses non-inheritable synchronous terminal ends, overlapped host ends, a direct pseudo-console process attribute, and `bInheritHandles=FALSE`. Internal named pipes have random names, reject remote clients, require the first instance, and grant access only to the object owner and SYSTEM. No WebSocket integration or terminal-emulation dependency is included.
