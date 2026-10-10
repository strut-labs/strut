extern "C" function strut_nift_add25_i32(int_32 seed, ptr<int> out) -> int_32;
extern "C" function strut_nift_add25_bad() -> int_32;

function main() -> void {
    unsafe {
        owner := new(9);
        ptr<int> p := ptr(owner);
        rc := strut_nift_add25_i32(17, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        rc = strut_nift_add25_i32(0, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        rc = strut_nift_add25_i32(-25, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        rc = strut_nift_add25_i32(100, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        rc = strut_nift_add25_i32(-128, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        // failure + recovery: failed evaluation must not corrupt the integration.
        rc = strut_nift_add25_i32(17, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        rc = strut_nift_add25_bad();
        if (rc == 0) { print("NOFAIL"); } else { print("FAILED"); }
        rc = strut_nift_add25_i32(17, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
    }
    return;
}
