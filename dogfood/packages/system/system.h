function run_capture(string program, string[] args) -> string : ExecError {
    result := exec(program, args);
    return result.stdout;
}
