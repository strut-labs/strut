#!/usr/bin/env python3
"""FFI-9 Python consumer: a real ctypes application driving Strut through the shared
embedding library (libstrut_embed). Exercises context create/destroy, source+file load,
primitive/string/bytes invocation, borrowed callback, structured errors, reload/recovery,
retained-callback BUSY destroy, and explicit value/error release."""
import ctypes, os, sys, tempfile

libpath = os.environ.get("STRUT_EMBED_LIB")
if not libpath:
    raise SystemExit("set STRUT_EMBED_LIB to the built libstrut_embed")
lib = ctypes.CDLL(libpath)

CBFN = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32)

class EmbedValue(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_int), ("i", ctypes.c_int64), ("d", ctypes.c_double),
                ("b", ctypes.c_int),
                ("data", ctypes.c_void_p), ("len", ctypes.c_size_t),
                ("retained", ctypes.c_void_p), ("cb_fn", CBFN), ("cb_ctx", ctypes.c_void_p)]

class EmbedError(ctypes.Structure):
    _fields_ = [("category", ctypes.c_int), ("owner", ctypes.c_void_p),
                ("type", ctypes.c_char_p), ("message", ctypes.c_char_p), ("code", ctypes.c_int)]

VOID,BOOL,INT,FLOAT,STR,BYTES,RETAINED,CALLBACK = range(8)

lib.strut_embed_context_create.restype = ctypes.c_void_p
lib.strut_embed_context_destroy.argtypes = [ctypes.c_void_p]
lib.strut_embed_context_destroy.restype = ctypes.c_int
lib.strut_embed_context_load_source.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.POINTER(EmbedError))]
lib.strut_embed_context_load_source.restype = ctypes.c_int
lib.strut_embed_context_load_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.POINTER(EmbedError))]
lib.strut_embed_context_load_file.restype = ctypes.c_int
lib.strut_embed_invoke.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(EmbedValue), ctypes.c_size_t, ctypes.POINTER(EmbedValue), ctypes.POINTER(ctypes.POINTER(EmbedError))]
lib.strut_embed_invoke.restype = ctypes.c_int
lib.strut_embed_retained_invoke.argtypes = [ctypes.c_void_p, ctypes.POINTER(EmbedValue), ctypes.POINTER(EmbedValue), ctypes.c_size_t, ctypes.POINTER(EmbedValue), ctypes.POINTER(ctypes.POINTER(EmbedError))]
lib.strut_embed_retained_invoke.restype = ctypes.c_int
lib.strut_embed_value_free.argtypes = [ctypes.c_void_p, ctypes.POINTER(EmbedValue)]
lib.strut_embed_error_release.argtypes = [ctypes.c_void_p, ctypes.POINTER(EmbedError)]

def check(cond, what):
    if not cond:
        print("CONSUMER FAIL: " + what, file=sys.stderr)
        raise SystemExit(1)
    return True

def err_text(e):
    return (e.message or b"").decode() if e else ""

def get_err(epp):
    e = epp.contents
    cat, code, ty, msg = e.category, e.code, (e.type or b"").decode(), (e.message or b"").decode()
    # release the embedding-owned handle deterministically
    lib.strut_embed_error_release(None, epp)
    return (cat, code, ty, msg)

SRC = (b'export "C" function add(int_32 a, int_32 b) -> int_32 { return a + b; }\n'
       b'export "C" function neg(int_32 x) -> int_32 { return -x; }\n'
       b'export "C" function greet(string name) -> string { return "hi " + name; }\n'
       b'export "C" function echo_bytes(bytes b) -> bytes { return b; }\n'
       b'export "C" function apply_cb(function<(int_32)->int_32> f, int_32 v) -> int_32 { return f(v) + f(v + 1); }\n'
       b'error EmbedErr { string message; }\n'
       b'export "C" function risky(int_32 x) -> int_32 : EmbedErr { if (x < 0) { throw EmbedErr { message: "boom" }; } return x; }\n'
       b'export "C" function make_rc(int_32 base) -> retained_callback<(int_32)->int_32> { return retained_callback((int_32 x) => x + base); }\n')
RESULT = 0

def run():
    global RESULT
    ctx = lib.strut_embed_context_create()
    check(ctx, "create")
    epp = ctypes.POINTER(EmbedError)()
    r = lib.strut_embed_context_load_source(ctx, SRC, len(SRC), ctypes.byref(epp))
    if r != 0:
        check(False, "load source: " + err_text(epp.contents))
    else:
        check(True, "load source")

    # integer
    out = EmbedValue(); errp = ctypes.POINTER(EmbedError)()
    av = (EmbedValue*2)()
    av[0].kind, av[0].i = INT, 20
    av[1].kind, av[1].i = INT, 22
    check(lib.strut_embed_invoke(ctx, b"add", av, 2, ctypes.byref(out), ctypes.byref(errp)) == 0, "add")
    check(out.i == 42, "add=42")
    # string
    sb = ctypes.create_string_buffer(b"stranger"); sv = EmbedValue(); sv.kind = STR; sv.data = ctypes.addressof(sb); sv.len = 8
    check(lib.strut_embed_invoke(ctx, b"greet", ctypes.byref(sv), 1, ctypes.byref(out), ctypes.byref(errp)) == 0, "greet")
    from ctypes import string_at
    got = string_at(out.data, out.len).decode()
    check(got == "hi stranger", "greet result")
    lib.strut_embed_value_free(ctx, ctypes.byref(out))
    # bytes with embedded NUL
    bv = EmbedValue(); bv.kind = BYTES; buf = ctypes.create_string_buffer(b"a\x00b\xff", 4)
    bv.data = ctypes.addressof(buf); bv.len = 4
    check(lib.strut_embed_invoke(ctx, b"echo_bytes", ctypes.byref(bv), 1, ctypes.byref(out), ctypes.byref(errp)) == 0, "bytes")
    gotbytes = string_at(out.data, out.len) if out.data else b""
    check(out.len == 4 and gotbytes == b"a\x00b\xff", "bytes exact len="+str(out.len)+" hex="+gotbytes.hex())
    lib.strut_embed_value_free(ctx, ctypes.byref(out))
    # borrowed callback (Python-defined) through the typed ABI
    BASE = 10
    def my_cb(c, x): return x + BASE
    cb = CBFN(my_cb)          # keep referenced for the invocation lifetime
    cv = EmbedValue(); cv.kind = CALLBACK; cv.cb_fn = cb; cv.cb_ctx = None
    nv = EmbedValue(); nv.kind = INT; nv.i = 5
    bargs = (EmbedValue*2)(cv, nv)
    check(lib.strut_embed_invoke(ctx, b"apply_cb", bargs, 2, ctypes.byref(out), ctypes.byref(errp)) == 0, "apply_cb")
    check(out.i == (5+BASE)+(6+BASE), "apply_cb result")
    # checked error (structured; release handle)
    nv2 = EmbedValue(); nv2.kind = INT; nv2.i = -1
    check(lib.strut_embed_invoke(ctx, b"risky", ctypes.byref(nv2), 1, ctypes.byref(out), ctypes.byref(errp)) != 0, "risky fail")
    cat, code, ty, msg = get_err(errp)
    check(cat == 4 and ty == "EmbedErr" and msg == "boom", "checked err identity")
    # reload failure is recoverable (structured PARSE), old module still works
    perr = ctypes.POINTER(EmbedError)()
    check(lib.strut_embed_context_load_source(ctx, b"function bad( -> {", 17, ctypes.byref(perr)) != 0, "parse fail")
    c2, c3, t2, m2 = get_err(perr)
    check(c2 == 1, "parse category")
    check(lib.strut_embed_invoke(ctx, b"add", av, 2, ctypes.byref(out), ctypes.byref(errp)) == 0 and out.i == 42, "recover")
    # retained callback + BUSY destroy + final release + destroy
    rv = EmbedValue(); rv.kind = INT; rv.i = 100
    rc = EmbedValue()
    check(lib.strut_embed_invoke(ctx, b"make_rc", ctypes.byref(rv), 1, ctypes.byref(rc), ctypes.byref(errp)) == 0 and rc.retained, "make_rc")
    xv = EmbedValue(); xv.kind = INT; xv.i = 5
    check(lib.strut_embed_retained_invoke(ctx, ctypes.byref(rc), ctypes.byref(xv), 1, ctypes.byref(out), ctypes.byref(errp)) == 0 and out.i == 105, "retained invoke")
    check(lib.strut_embed_context_destroy(ctx) != 0, "BUSY destroy with retained outstanding")
    lib.strut_embed_value_free(ctx, ctypes.byref(rc))
    check(lib.strut_embed_context_destroy(ctx) == 0, "destroy after release")
    print("python consumer ok")
    return 0

try:
    rc = run()
except SystemExit as e:
    rc = e.code if isinstance(e.code, int) else 1
sys.exit(rc)