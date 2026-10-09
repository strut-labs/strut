#!/usr/bin/env python3
"""FFI-7 Direction A: an exported Strut retained_callback<sig> is delivered to native as an
opaque C handle with retain/release/invoke. The handle owns an atomic C refcount and one
internal retained_callback value; C refcount and Strut control block are independent.
Includes invoke-after-origin-call, capture escape, ownership-domain independence,
retain/release stress, and a two-module header-coexistence strict TU."""
from pathlib import Path
import os, platform, re, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_retained_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "retained ok\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-retained-") as td:
    td = Path(td)

    def build(name):
        header = td / (name + ".h")
        ext = ".dll" if os.name == "nt" else (".dylib" if platform.system() == "Darwin" else ".so")
        lib = td / ("lib" + name + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), name + ".p"], check=True, cwd=str(root))
        return header, lib, header.read_text()

    header, lib, text = build("export_retained")
    assert "typedef struct strut_ffi_export_retained_eda5fa3b2eb3dc5060e65a11a93c86ac_retained_cb" in text, "opaque retained typedef missing"
    assert "retained_cb_7b5fe6280318fd17_retain(" in text and "retained_cb_7b5fe6280318fd17_release(" in text and "retained_cb_7b5fe6280318fd17_invoke(" in text, "retain/release/invoke symbols missing"
    assert "std::function" not in text and "shared_ptr" not in text and "strut_retained_callback" not in text, "C++ internals leaked into C header"
    assert "make_adder(" in text and "apply_retained(" in text, "export signatures missing"

    env = os.environ.copy()
    key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
    env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
    if os.name == "nt":
        dest_exe = td / "host_c.exe"
        subprocess.run(["cl", "/nologo", "/Wall", "/WX", "/wd5045", "/wd4820", "/wd4668", "/wd5105", "/I", str(td), str(root / "export_retained_host.c"),
                        "/Fe:" + str(dest_exe), "/link", str(td / "libexport_retained.lib")], check=True)
        out = subprocess.check_output([str(dest_exe)], text=True, env=env)
    else:
        cc = os.environ.get("CC", "cc")
        strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic"]
        if os.environ.get("STRUT_TEST_SANITIZE"):
            strict += ["-fsanitize=address", "-g"]
        dest_exe = td / "host_c"
        subprocess.run([cc, *strict, "-I", str(td), str(root / "export_retained_host.c"),
                        "-L", str(td), "-lexport_retained", "-o", str(dest_exe)], check=True)
        out = subprocess.check_output([str(dest_exe)], text=True, env=env)
    if out != expected:
        raise SystemExit(f"unexpected Direction-A host output: {out!r}")

    header2, lib2, text2 = build("export_retained2")
    def macro_names(h):
        cb = re.search(r'#define (\S*RETAINED_CB_\S+)', h).group(1)
        rl = re.search(r'#define (\S*RETAINED_RELEASE_\S+)', h).group(1)
        iv = re.search(r'#define (\S*RETAINED_INVOKE_\S+)', h).group(1)
        return cb, rl, iv
    a = macro_names(text); b = macro_names(text2)
    assert a[0] != b[0] and a[1] != b[1] and a[2] != b[2], "module-local retained names collided"
    coexist = td / "coexist.c"
    coexist.write_text(f'''#include "{header.name}"
#include "{header2.name}"
#include <stdio.h>
#define CHECK(c) do {{ if (!(c)) {{ printf("FAIL line %d\\n", __LINE__); return 1; }} }} while (0)
int main(void) {{
    {a[0]}* h = make_adder(20);
    {b[0]}* k = make_adder2(30);
    CHECK({a[2]}(h, 1) == 21);
    CHECK({b[2]}(k, 1) == 31);
    {a[1]}(h); {b[1]}(k);
    return 0;
}}
''')
    env[key] = str(td) + os.pathsep + (env[key] if env.get(key) else "") + (os.pathsep + str(td) if False else "")
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/Wall", "/WX", "/wd5045", "/wd4820", "/wd4668", "/wd5105", "/I", str(td), str(coexist),
                        "/Fe:" + str(td / "coexist_c.exe"),
                        "/link", str(td / "libexport_retained.lib"), str(td / "libexport_retained2.lib")], check=True)
        subprocess.check_output([str(td / "coexist_c.exe")], text=True, env=env)
    else:
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, *strict, "-I", str(td), str(coexist),
                        "-L", str(td), "-lexport_retained", "-lexport_retained2", "-o", str(td / "coexist_c")], check=True)
        subprocess.check_output([str(td / "coexist_c")], text=True, env=env)

print("export C retained Direction-A lifecycle + two-module coexistence passed (opaque handle; independent ownership domains)")
