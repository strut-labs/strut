error ModuleError {
    string message;
    int code;
}

function module_failure() -> int : ModuleError {
    throw ModuleError { message: "from module", code: 7 };
}
