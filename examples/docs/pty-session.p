function drain(pty terminal) -> string : PtyError {
    string transcript := "";
    while (!terminal.eof()) {
        bytes chunk := terminal.read_bytes(4096);
        if (!chunk.empty()) {
            transcript = transcript + chunk.to_string();
        }
    }
    return transcript;
}
function run_terminal(cancellation_token token) -> int : PtyError {
    terminal := pty_spawn("sh", [], {
        "cwd": ".",
        "env": {"TERM": "xterm-256color"},
        "rows": 30,
        "columns": 100
    }, token);
    terminal.write_bytes(bytes.from_string("printf ready; exit 0\n"));
    string transcript := drain(terminal);
    return terminal.wait();
}
function main() -> int {
    return 0;
}
