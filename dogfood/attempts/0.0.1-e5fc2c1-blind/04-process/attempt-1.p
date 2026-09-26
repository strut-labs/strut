function main() -> int : ExecError {
    result := exec("printf", ["hello\\n"]);
    print(result.stdout);
    return result.exit_code;
}
