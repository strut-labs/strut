extern "C" function probe_bytes(ptr<uint_8> data, int_32 len) -> int_32;

function main() -> void {
    unsafe {
        b := bytes("abc");
        p := ptr(b.data);
        rc := probe_bytes(p, 3);
        print(rc);
    }
    return;
}
