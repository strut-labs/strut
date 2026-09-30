function run_child(cancellation_token token) -> int : ExecError {
    child := process("worker", ["--stream"], {
        "cwd": ".",
        "env": {"MODE": "batch"}
    }, token);
    child.in.write_line("first job");
    child.close_input();
    child.terminate();
    return child.wait();
}
function main() -> int {
    return 0;
}
