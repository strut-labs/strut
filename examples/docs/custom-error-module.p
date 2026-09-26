include "module-error.h";

function main() -> int {
    try {
        module_failure();
    } catch (ModuleError err) {
        println(err.message);
        println(err.code);
    }
    return 0;
}
