extern "C" function strut_nift_add25_i32(int_32 seed, ptr<int> out) -> int_32;
extern "C" function strut_nift_add25_bad() -> int_32;
extern "C" function strut_nift_same_engine_recovery() -> int_32;
extern "C" function strut_nift_parse_i32_check() -> int_32;

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
        if (rc == 0) { print("UNEXPECTED"); } else if (rc == 4) { print("FAILED"); } else { print("OTHER"); }
        rc = strut_nift_add25_i32(17, p);
        if (rc != 0) { print("ERR"); } else { print(*owner); }
        rc = strut_nift_same_engine_recovery();
        if (rc == 0) { print("RECOVERED"); } else { print("BADREC"); }
        rc = strut_nift_parse_i32_check();
        if (rc != 0) { print("PARSE-OK"); } else { print("PARSE-BAD"); }
    }
    return;
}
