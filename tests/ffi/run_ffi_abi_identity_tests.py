#!/usr/bin/env python3
"""FFI ABI identity tests:

1. Reproducibility: the SAME logical module built under DIFFERENT absolute directories must
   produce IDENTICAL generated C ABI names (typedefs, release symbols). Identity is the
   logical source path as provided, never the build machine's absolute path.
2. Coexistence: two generated headers that BOTH define `struct Pair` (and owned results) must
   compile together -- their readable aliases/typedefs/helpers are module-qualified.
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_ffi_abi_identity_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()

src = ('struct Pair { int_32 a; int_32 b; }\n'
       'export "C" function ident_pair(int_32 x, int_32 y) -> Pair { return Pair { a: x, b: y }; }\n'
       'export "C" function ident_str() -> string { return "id"; }\n')

with tempfile.TemporaryDirectory(prefix="strut-ffi-ident-") as td:
    td = Path(td)
    sh = "dylib" if platform.system() == "Darwin" else ("dll" if os.name == "nt" else "so")
    # ---- 1. reproducibility across different absolute roots ----
    dirs = [td / "a", td / "b" / "nested" / "deep"]
    headers = []
    for d in dirs:
        d.mkdir(parents=True)
        (d / "prog.p").write_text(src)
        h = d / "prog.h"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(h),
                        "-o", str(d / ("libprog." + sh)), "prog.p"], check=True, cwd=str(d))
        headers.append(h.read_text())
    assert headers[0] == headers[1], "absolute build path leaked into generated ABI names"
    assert "#define PROG_Pair " in headers[0], "expected module-qualified aggregate alias"

    # ---- 1b. equivalent logical path spellings -> same identity ----
    d = dirs[0]
    (d / "sub").mkdir(exist_ok=True)
    spellings = ["prog.p", "./prog.p", "sub/../prog.p"]
    eq = []
    for k, sp in enumerate(spellings):
        h = d / ("eq%d.h" % k)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(h),
                        "-o", str(d / ("libeq%d." % k + sh)), sp], check=True, cwd=str(d))
        eq.append(h.read_text())
    assert eq[0] == eq[1] == eq[2], "equivalent logical path spellings produced different ABI names"

    # ---- 2. two headers with the same struct short name coexist ----
    pkgs = td / "pkgs"
    for mod, fn, x, y in [("module_a", "a_pair", 1, 2), ("module_b", "b_pair", 3, 4)]:
        (pkgs / mod).mkdir(parents=True, exist_ok=True)
        (pkgs / mod / "util.p").write_text(
            'struct Pair { int_32 a; int_32 b; }\n'
            f'export "C" function {fn}() -> Pair {{ return Pair {{ a: {x}, b: {y} }}; }}\n')
    ha = td / "a.h"; hb = td / "b.h"
    subprocess.run([str(compiler), "--shared", "--emit-c-header", str(ha),
                    "-o", str(td / ("liba." + sh)), "module_a/util.p"], check=True, cwd=str(pkgs))
    subprocess.run([str(compiler), "--shared", "--emit-c-header", str(hb),
                    "-o", str(td / ("libb." + sh)), "module_b/util.p"], check=True, cwd=str(pkgs))
    ta, tb = ha.read_text(), hb.read_text()
    assert "#define MODULE_A_UTIL_Pair " in ta, "module_a alias not module-qualified"
    assert "#define MODULE_B_UTIL_Pair " in tb, "module_b alias not module-qualified"
    host = td / "coexist.c"
    host.write_text('#include "a.h"\n#include "b.h"\n'
                    'int main(void){ MODULE_A_UTIL_Pair a = a_pair(); MODULE_B_UTIL_Pair b = b_pair();'
                    ' return (a.a == 1 && b.a == 3) ? 0 : 1; }\n')
    cc = os.environ.get("CC", "cc")
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/W4", "/WX", "/c", "/I", str(td), str(host),
                        "/Fo:" + str(td / "coexist.obj")], check=True)
    else:
        subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(td),
                        "-c", str(host), "-o", str(td / "coexist.o")], check=True)

print("ABI identity passed (checkout-independent names; colliding struct short names coexist)")
