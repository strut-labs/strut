extern "C" function static_part() -> int;
extern "C" function dynamic_part() -> int;
function main() -> void {
    unsafe {
        print(static_part() + dynamic_part());
    }
    return;
}
