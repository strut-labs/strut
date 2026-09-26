#!/usr/bin/env python3
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
expected = sys.argv[1].removeprefix("v") if len(sys.argv) > 1 else "0.0.1"

cmake = (root / "CMakeLists.txt").read_text()
header = (root / "include/strut/version.h").read_text()
notes = (root / "RELEASE_NOTES.md").read_text()

checks = {
    "CMake project version": rf"project\(strut VERSION {re.escape(expected)}\b",
    "compiler version": rf'version = "{re.escape(expected)}"',
    "release notes heading": rf"^# Strut {re.escape(expected)}$",
}
texts = [cmake, header, notes]
failed = [name for (name, pattern), text in zip(checks.items(), texts)
          if re.search(pattern, text, re.MULTILINE) is None]
if failed:
    print("release version mismatch: " + ", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print(f"release metadata is consistent for {expected}")
