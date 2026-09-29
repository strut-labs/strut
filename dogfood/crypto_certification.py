#!/usr/bin/env python3
"""Certify public cryptographic and Base64 runtime primitives."""

from pathlib import Path
import subprocess
import sys
import tempfile


def compile_program(compiler, root, name, source):
    path = root / f"{name}.p"
    executable = root / (f"{name}.exe" if sys.platform == "win32" else name)
    path.write_text(source, encoding="utf-8")
    subprocess.run([compiler, path, "-o", executable], cwd=root, check=True)
    return executable


def run_program(executable, root, expected):
    result = subprocess.run([executable], cwd=root, text=True, capture_output=True, check=False)
    if result.returncode != 0 or result.stdout != expected or result.stderr:
        raise RuntimeError(
            f"{executable.name} failed: exit={result.returncode} "
            f"stdout={result.stdout!r} stderr={result.stderr!r}"
        )


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    vectors = r'''include <encoding>;
include <crypto>;

function main() -> int : (CryptoError, EncodingError) {
    bytes empty := bytes();
    if (base64_encode(empty) != "" || base64url_encode(empty) != "") { return 1; }
    if (base64_encode(bytes.from_string("f")) != "Zg==") { return 2; }
    if (base64_encode(bytes.from_string("fo")) != "Zm8=") { return 3; }
    if (base64_encode(bytes.from_string("foo")) != "Zm9v") { return 4; }
    if (base64_encode(bytes.from_string("foob")) != "Zm9vYg==") { return 5; }
    if (base64_encode(bytes.from_string("fooba")) != "Zm9vYmE=") { return 6; }
    if (base64_encode(bytes.from_string("foobar")) != "Zm9vYmFy") { return 7; }
    bytes binary := [0, 255, 128];
    if (base64_encode(binary) != "AP+A" || base64url_encode(binary) != "AP-A") { return 8; }
    if (base64_decode("AP+A") != binary || base64url_decode("AP-A") != binary) { return 9; }

    bytes expected_sha := [227, 176, 196, 66, 152, 252, 28, 20, 154, 251, 244, 200, 153, 111, 185, 36, 39, 174, 65, 228, 100, 155, 147, 76, 164, 149, 153, 27, 120, 82, 184, 85];
    if (!constant_time_equal(sha256(empty), expected_sha)) { return 10; }
    bytes expected_hmac := [247, 188, 131, 244, 48, 83, 132, 36, 177, 50, 152, 230, 170, 111, 177, 67, 239, 77, 89, 161, 73, 70, 23, 89, 151, 71, 157, 188, 45, 26, 60, 216];
    bytes key := bytes.from_string("key");
    bytes message := bytes.from_string("The quick brown fox jumps over the lazy dog");
    if (!constant_time_equal(hmac_sha256(key, message), expected_hmac)) { return 11; }
    if (constant_time_equal(expected_sha, expected_hmac) || constant_time_equal([0], [0, 0])) { return 12; }
    if (!constant_time_equal(empty, empty)) { return 13; }
    if (secure_random_bytes(0).length() != 0 || secure_random_bytes(64).length() != 64) { return 14; }
    print("crypto vectors passed");
    return 0;
}
'''
    malformed = r'''include <encoding>;
include <crypto>;

function bad_base64(string value) -> bool {
    try { base64_decode(value); }
    catch (EncodingError caught) { return true; }
    return false;
}
function bad_base64url(string value) -> bool {
    try { base64url_decode(value); }
    catch (EncodingError caught) { return true; }
    return false;
}
function bad_random() -> bool {
    try { secure_random_bytes(-1); }
    catch (CryptoError caught) { return true; }
    return false;
}
function main() -> int {
    if (!bad_base64("A") || !bad_base64("AAA") || !bad_base64("====")) { return 1; }
    if (!bad_base64("A===") || !bad_base64("AA=A") || !bad_base64("AB==")) { return 2; }
    if (!bad_base64("AAB=") || !bad_base64("AA-A") || !bad_base64("AA A")) { return 3; }
    if (!bad_base64url("A") || !bad_base64url("AA=") || !bad_base64url("AA+A")) { return 4; }
    if (!bad_base64url("AB") || !bad_base64url("AAB") || !bad_base64url("AA/")) { return 5; }
    if (!bad_random()) { return 6; }
    print("malformed crypto inputs rejected");
    return 0;
}
'''

    with tempfile.TemporaryDirectory(prefix="strut-crypto-certification-") as temporary:
        root = Path(temporary)
        run_program(compile_program(compiler, root, "crypto-vectors", vectors), root, "crypto vectors passed\n")
        run_program(compile_program(compiler, root, "crypto-malformed", malformed), root, "malformed crypto inputs rejected\n")

    print("Crypto certification: known-answer, binary, random-length, strict Base64 and malformed-input checks passed")


if __name__ == "__main__":
    main()
