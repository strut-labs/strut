#!/usr/bin/env python3
"""FFI ABI name safety: distinct source/module names that normalize to the same C identifier
(e.g. `slug-a` vs `slug_a`) MUST NOT produce the same module slug, or their exported symbols
(aggregate typedefs, release functions) collide. The slug appends a deterministic hash of the
original stem whenever sanitization altered it.
"""
from pathlib import Path
import subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_ffi_module_slug_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
src = 'export "C" function f() -> string { return "x"; }\n'

with tempfile.TemporaryDirectory(prefix="strut-ffi-slug-") as td:
    td = Path(td)
    names = ["slug-a.p", "slug_a.p", "a b.p"]
    slugs = []
    for n in names:
        (td / n).write_text(src)
        header = td / (n + ".h")
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(td / (n + ".out")), str(td / n)], check=True)
        text = header.read_text()
        symbol = None
        for line in text.splitlines():
            if "_ffi_free_string" in line and "void " in line:
                symbol = line.split("void ", 1)[1].split("(", 1)[0]
        assert symbol, f"no release symbol in header for {n}"
        slugs.append(symbol)
    assert len(set(slugs)) == len(slugs), f"module slug collision: {slugs}"

print(f"module slug disambiguation passed ({slugs})")
