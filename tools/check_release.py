#!/usr/bin/env python3
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]

cmake = (root / "CMakeLists.txt").read_text()
header = (root / "include/strut/version.h").read_text()
notes = (root / "RELEASE_NOTES.md").read_text()

if len(sys.argv) > 1:
    expected = sys.argv[1].removeprefix("v")
else:
    match = re.search(r"project\(strut VERSION ([0-9]+\.[0-9]+\.[0-9]+)", cmake)
    expected = match.group(1) if match else "0.0.3"

checks = {
    "CMake project version": rf"project\(strut VERSION {re.escape(expected)}\b",
    "compiler version": rf'version = "{re.escape(expected)}"',
}
texts = [cmake, header]
if len(sys.argv) > 1:
    # Explicit tag: require the immutable release heading.
    checks["release notes heading"] = rf"^# Strut {re.escape(expected)}$"
    texts.append(notes)
else:
    # Development build: accept an Unreleased section or the release heading.
    if re.search(r"^# Unreleased", notes, re.MULTILINE) is None and re.search(rf"^# Strut {re.escape(expected)}$", notes, re.MULTILINE) is None:
        checks["unreleased or release notes heading"] = rf"^# (Unreleased|Strut {re.escape(expected)})$"
        texts.append(notes)
failed = [name for (name, pattern), text in zip(checks.items(), texts)
          if re.search(pattern, text, re.MULTILINE) is None]
if failed:
    print("release version mismatch: " + ", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print(f"release metadata is consistent for {expected}")
