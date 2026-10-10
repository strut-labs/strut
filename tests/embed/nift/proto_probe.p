extern "C" function probe_raw(raw_ptr<uint_8> data, int_32 len) -> int_32;

function main() -> void {
    unsafe {
        b := bytes.from_string("abc");
        raw_ptr<uint_8> rp := b.data();
        rc := probe_raw(rp, 3);
        print(rc);
    }
    return;
}
