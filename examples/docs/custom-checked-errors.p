error ValidationError {
    string message;
    int code;
}

function validate(int value) -> int : ValidationError {
    if (value < 0) {
        throw ValidationError { message: "negative value", code: 42 };
    }
    return value;
}

function main() -> int {
    try {
        validate(-1);
    } catch (ValidationError err) {
        println(err.message);
        println(err.code);
    }
    return 0;
}
