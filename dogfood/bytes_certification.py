#!/usr/bin/env python3
"""Certify first-class bytes behavior and binary filesystem round trips."""

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


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    values = ", ".join(str(value) for value in range(256))
    source = f"""include <filesystem>;
function main() -> int : FilesystemError {{
    bytes empty := bytes();
    if (!empty.empty() || empty.length() != 0) {{ return 1; }}
    bytes all := [{values}];
    if (all.length() != 256 || all[0] != 0 || all[255] != 255) {{ return 2; }}
    bytes copy := all;
    copy[0] = 99;
    if (all[0] != 0 || copy == all) {{ return 3; }}
    bytes middle := all.slice(0, 256);
    if (middle != all) {{ return 4; }}
    string raw := all.to_string();
    bytes decoded := bytes.from_string(raw);
    if (decoded != all || decoded[0] != 0 || decoded[255] != 255) {{ return 5; }}
    bytes large := bytes(1048576);
    large[1048575] = 255;
    if (large.length() != 1048576 || large[1048575] != 255) {{ return 6; }}
    write_file("bytes.bin", all);
    append_file("bytes.bin", all);
    bytes stored := read_bytes("bytes.bin");
    if (stored.length() != 512 || stored.slice(0, 256) != all || stored.slice(256, 512) != all) {{ return 7; }}
    remove("bytes.bin");
    print("bytes certification passed");
    return 0;
}}
"""
    bounds_source = """function main() -> void {
    bytes value := [0];
    print(value[1]);
    return;
}
"""

    with tempfile.TemporaryDirectory(prefix="strut-bytes-certification-") as temporary:
        root = Path(temporary)
        executable = compile_program(compiler, root, "bytes", source)
        result = subprocess.run([executable], cwd=root, text=True, capture_output=True, check=False)
        if result.returncode != 0 or result.stdout != "bytes certification passed\n":
            raise RuntimeError(f"bytes behavior failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")

        bounds = compile_program(compiler, root, "bytes-bounds", bounds_source)
        result = subprocess.run([bounds], cwd=root, text=True, capture_output=True, check=False)
        if result.returncode == 0 or "range" not in result.stderr:
            raise RuntimeError(f"bytes bounds failure was not reported: exit={result.returncode} stderr={result.stderr!r}")

    print("Bytes certification: ownership, all byte values, conversion, filesystem round trip, large values and bounds passed")


if __name__ == "__main__":
    main()
