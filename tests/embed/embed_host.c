/* FFI-8 embedding: an ordinary C host drives Strut in-process through the opaque
 * strut_embed_* surface (create/destroy, source load, invoke, structured errors).
 * No CLI spawn, no C++ types across the boundary. */
#include "strut/embed.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
typedef HANDLE embed_thr;
static embed_thr e_thr(void* (*fn)(void*), void* a) { DWORD id; return CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(void*)fn, a, 0, &id); }
static void e_join(embed_thr h) { WaitForSingleObject(h, INFINITE); CloseHandle(h); }
#else
#include <pthread.h>
typedef pthread_t embed_thr;
static embed_thr e_thr(void* (*fn)(void*), void* a) { pthread_t t; pthread_create(&t, 0, fn, a); return t; }
static void e_join(embed_thr t) { pthread_join(t, 0); }
#endif

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); ++failures; } } while (0)

struct conc_work { strut_embed_context* c; int bad; };
static void* conc_worker(void* a) {
    struct conc_work* w = (struct conc_work*)a;
    strut_embed_error* e = 0;
    for (int i = 0; i < 2000; ++i) {
        strut_embed_value n; memset(&n, 0, sizeof n); n.kind = STRUT_EMBED_VALUE_INT; n.i = i;
        strut_embed_value o;
        if (strut_embed_invoke(w->c, "neg", &n, 1, &o, &e) != 0 || o.i != -(int64_t)i) { w->bad = 1; break; }
    }
    return 0;
}

struct ret_work { strut_embed_context* c; strut_embed_value r; int bad; int result; };
static void* ret_worker(void* a) {
    struct ret_work* w = (struct ret_work*)a;
    strut_embed_error* e = 0;
    strut_embed_value x; memset(&x, 0, sizeof x); x.kind = STRUT_EMBED_VALUE_INT; x.i = 5;
    strut_embed_value o;
    if (strut_embed_retained_invoke(w->c, &w->r, &x, 1, &o, &e) != 0 || o.i != 15) w->bad = 1;
    else w->result = (int)o.i;
    return 0;
}

int main(void) {
    strut_embed_context* ctx = strut_embed_context_create();
    CHECK(ctx != 0);

    static const char* src =
        "export \"C\" function add(int_32 a, int_32 b) -> int_32 { return a + b; }\n"
        "export \"C\" function greet(string name) -> string { return \"hi \" + name; }\n"
        "export \"C\" function neg(int_32 x) -> int_32 { return -x; }\n"
        "export \"C\" function echo_bytes(bytes b) -> bytes { return b; }\n"
        "error EmbedErr { string message; }\n"
        "export \"C\" function risky(int_32 x) -> int_32 : EmbedErr { if (x < 0) { throw EmbedErr { message: \"bad\" }; } return x; }\n";
    strut_embed_error* err = 0;
    CHECK(strut_embed_context_load_source(ctx, src, strlen(src), &err) == 0);
    if (err) { strut_embed_error_release(ctx, err); err = 0; }

    strut_embed_value args[2];
    strut_embed_value out;
    memset(&out, 0, sizeof(out));
    args[0].kind = STRUT_EMBED_VALUE_INT; args[0].i = 20;
    args[1].kind = STRUT_EMBED_VALUE_INT; args[1].i = 22;
    CHECK(strut_embed_invoke(ctx, "add", args, 2, &out, &err) == 0);
    CHECK(err == 0 && out.kind == STRUT_EMBED_VALUE_INT && out.i == 42);

    strut_embed_value sarg;
    sarg.kind = STRUT_EMBED_VALUE_STRING; sarg.s.data = "stranger"; sarg.s.len = 8;
    CHECK(strut_embed_invoke(ctx, "greet", &sarg, 1, &out, &err) == 0);
    CHECK(err == 0 && out.kind == STRUT_EMBED_VALUE_STRING && out.s.len == 11 &&
          memcmp(out.s.data, "hi stranger", 11) == 0);
    strut_embed_value_free(ctx, &out);
    /* repeated string results: allocate then release through the Strut-owned path */
    for (int i = 0; i < 500; ++i) {
        strut_embed_value s2; s2.kind = STRUT_EMBED_VALUE_STRING; s2.s.data = "x"; s2.s.len = 1;
        CHECK(strut_embed_invoke(ctx, "greet", &s2, 1, &out, &err) == 0);
        CHECK(out.kind == STRUT_EMBED_VALUE_STRING && out.s.len == 4 && memcmp(out.s.data, "hi x", 4) == 0);
        strut_embed_value_free(ctx, &out);
    }

    { strut_embed_value n; n.kind = STRUT_EMBED_VALUE_INT; n.i = -7;
      CHECK(strut_embed_invoke(ctx, "neg", &n, 1, &out, &err) == 0 && out.i == 7); }

    /* runtime checked error through the embedding boundary (no unwind past C) */
    { strut_embed_value n; n.kind = STRUT_EMBED_VALUE_INT; n.i = -1;
      CHECK(strut_embed_invoke(ctx, "risky", &n, 1, &out, &err) != 0);
      CHECK(err && err->category == STRUT_EMBED_ERR_INVOKE && err->type &&
            strcmp(err->type, "EmbedErr") == 0 && strcmp(err->message, "bad") == 0);
      strut_embed_error_release(ctx, err); err = 0; }

    /* malformed source -> structured parse error, then context recovers */
    strut_embed_error* perr = 0;
    CHECK(strut_embed_context_load_source(ctx, "function main() -> {", 21, &perr) != 0);
    CHECK(perr && perr->category == STRUT_EMBED_ERR_PARSE && perr->message);
    strut_embed_error_release(ctx, perr); perr = 0;
    CHECK(strut_embed_invoke(ctx, "add", args, 2, &out, &err) == 0 && out.i == 42);

    /* semantic error -> structured semantic category */
    strut_embed_error* serr = 0;
    CHECK(strut_embed_context_load_source(ctx, "function main() -> int { return \"no\"; }", 40, &serr) != 0);
    CHECK(serr && serr->category == STRUT_EMBED_ERR_SEMANTIC);
    strut_embed_error_release(ctx, serr); serr = 0;

    /* repeated lifecycle: create/load/invoke/destroy. Each load compiles the module to native
       (~2s), so the loop is kept small; ASan runs still prove clean create/destroy cycles. */
    for (int i = 0; i < 5; ++i) {
        strut_embed_context* c2 = strut_embed_context_create();
        CHECK(c2 != 0);
        strut_embed_error* lerr = 0;
        CHECK(strut_embed_context_load_source(c2, src, strlen(src), &lerr) == 0);
        if (lerr) strut_embed_error_release(c2, lerr);
        strut_embed_value a2; a2.kind = STRUT_EMBED_VALUE_INT; a2.i = i;
        strut_embed_value o2;
        CHECK(strut_embed_invoke(c2, "add", args, 2, &o2, &lerr) == 0 && o2.i == 42);
        strut_embed_context_destroy(c2);
    }

    /* simultaneous independent contexts + load_file */
    const char* srcb = "export \"C\" function twice(int_32 x) -> int_32 { return x * 2; }\n";
    strut_embed_context* ctxA = strut_embed_context_create();
    strut_embed_context* ctxB = strut_embed_context_create();
    strut_embed_error* ab_err = 0;
    CHECK(strut_embed_context_load_source(ctxA, src, strlen(src), &ab_err) == 0);
    CHECK(strut_embed_context_load_source(ctxB, srcb, strlen(srcb), &ab_err) == 0);
    { strut_embed_value n; n.kind = STRUT_EMBED_VALUE_INT; n.i = 9;
      strut_embed_value oy;
      CHECK(strut_embed_invoke(ctxA, "neg", &n, 1, &oy, &ab_err) == 0 && oy.i == -9);
      CHECK(strut_embed_invoke(ctxB, "twice", &n, 1, &oy, &ab_err) == 0 && oy.i == 18); }
    strut_embed_context_destroy(ctxA);
    { strut_embed_value n; n.kind = STRUT_EMBED_VALUE_INT; n.i = 5; strut_embed_value oy;
      CHECK(strut_embed_invoke(ctxB, "twice", &n, 1, &oy, &ab_err) == 0 && oy.i == 10);
      CHECK(strut_embed_invoke(ctxB, "twice", &n, 1, &oy, &ab_err) == 0 && oy.i == 10); }
    strut_embed_context_destroy(ctxB);

    /* load_file: valid file, missing file, and stable logical identity */
    const char* epath = "strut_embed_src.p";
    {
        FILE* f = fopen(epath, "wb");
        fwrite(srcb, 1, strlen(srcb), f); fclose(f);
    }
    strut_embed_context* fctx = strut_embed_context_create();
    strut_embed_error* fe = 0;
    CHECK(strut_embed_context_load_file(fctx, epath, &fe) == 0);
    strut_embed_error* mfe = 0;
    CHECK(strut_embed_context_load_file(fctx, "/definitely/missing/no_such.p", &mfe) != 0);
    CHECK(mfe && mfe->category == STRUT_EMBED_ERR_LOAD);
    strut_embed_error_release(fctx, mfe);
    { const char* badp = "strut_embed_bad.p";
      const char* badsrc = "function x( -> {";
      FILE* bf = fopen(badp, "wb"); fwrite(badsrc, 1, strlen(badsrc), bf); fclose(bf);
      strut_embed_error* be = 0;
      CHECK(strut_embed_context_load_file(fctx, badp, &be) != 0 && be && be->category == STRUT_EMBED_ERR_PARSE);
      strut_embed_error_release(fctx, be);
      const char* semp = "strut_embed_sem.p";
      const char* semsrc = "function x() -> int { return \"no\"; }";
      FILE* sf = fopen(semp, "wb"); fwrite(semsrc, 1, strlen(semsrc), sf); fclose(sf);
      strut_embed_error* se2 = 0;
      CHECK(strut_embed_context_load_file(fctx, semp, &se2) != 0 && se2 && se2->category == STRUT_EMBED_ERR_SEMANTIC);
      strut_embed_error_release(fctx, se2);
      strut_embed_error* reok = 0;
      CHECK(strut_embed_context_load_file(fctx, epath, &reok) == 0);   /* recovery after failed loads */
      remove(badp); remove(semp);
    }
    remove(epath);
    strut_embed_context_destroy(fctx);

    /* bytes: empty, embedded NUL, arbitrary binary, repeated release */
    { struct { const char* p; size_t n; } cases[3] = {{NULL,0},{"a\0b",3},{"\xff\x00\xfe\x01",4}};
      for (int ci = 0; ci < 3; ++ci) {
        strut_embed_value b; memset(&b,0,sizeof b); b.kind = STRUT_EMBED_VALUE_BYTES; b.s.data = cases[ci].p; b.s.len = cases[ci].n;
        CHECK(strut_embed_invoke(ctx, "echo_bytes", &b, 1, &out, &err) == 0);
        CHECK(out.kind == STRUT_EMBED_VALUE_BYTES && out.s.len == cases[ci].n &&
              (cases[ci].n == 0 || memcmp(out.s.data, cases[ci].p, cases[ci].n) == 0));
        strut_embed_value_free(ctx, &out);
      }
      for (int i = 0; i < 200; ++i) {
        strut_embed_value b; memset(&b,0,sizeof b); b.kind = STRUT_EMBED_VALUE_BYTES; b.s.data = "bin"; b.s.len = 3;
        CHECK(strut_embed_invoke(ctx, "echo_bytes", &b, 1, &out, &err) == 0);
        strut_embed_value_free(ctx, &out);
      }
    }
    /* host-owned error released with ctx == NULL (routing is internal) */
    { strut_embed_error* he = 0;
      CHECK(strut_embed_context_load_source(ctx, "function bad( -> {", 17, &he) != 0);
      CHECK(he && he->owner == 0);
      strut_embed_error_release(NULL, he); }
    /* value_free is a safe no-op for primitives/empty and after release (double-release safe) */
    { strut_embed_value pv; memset(&pv,0,sizeof pv); pv.kind = STRUT_EMBED_VALUE_INT; pv.i = 5;
      strut_embed_value_free(ctx, &pv); CHECK(pv.kind == STRUT_EMBED_VALUE_VOID); }

    /* OUTSTANDING module-owned result across reload: payload is embedding-owned, so it
       survives the module being reloaded away (twice) and released safely. */
    { strut_embed_context* rc = strut_embed_context_create();
      strut_embed_error* e2 = 0;
      const char* sa = "export \"C\" function hi(string n) -> string { return \"A:\" + n; }\n";
      CHECK(strut_embed_context_load_source(rc, sa, strlen(sa), &e2) == 0);
      strut_embed_value so; so.kind = STRUT_EMBED_VALUE_STRING; so.s.data = "x"; so.s.len = 1;
      strut_embed_value held;
      CHECK(strut_embed_invoke(rc, "hi", &so, 1, &held, &e2) == 0 && held.s.len == 3 && memcmp(held.s.data, "A:x", 3) == 0);
      const char* sb = "export \"C\" function bye(string n) -> string { return \"B:\" + n; }\n";
      CHECK(strut_embed_context_load_source(rc, sb, strlen(sb), &e2) == 0);   /* A displaced */
      CHECK(strut_embed_context_load_source(rc, sa, strlen(sa), &e2) == 0);   /* displaced again */
      CHECK(held.kind == STRUT_EMBED_VALUE_STRING && held.s.len == 3 && memcmp(held.s.data, "A:x", 3) == 0);
      strut_embed_value_free(rc, &held);                    /* safe: embedding-owned */
      strut_embed_context_destroy(rc); }

    /* OUTSTANDING module-owned error across context destruction: error payload is
       embedding-owned, so it is safe to read and release AFTER destroy. */
    { const char* se = "error E { string message; } export \"C\" function f(int_32 x) -> int_32 : E { throw E { message: \"boom\" }; }\n";
      strut_embed_context* ec = strut_embed_context_create();
      strut_embed_error* e3 = 0;
      CHECK(strut_embed_context_load_source(ec, se, strlen(se), &e3) == 0);
      strut_embed_value iv; iv.kind = STRUT_EMBED_VALUE_INT; iv.i = 1; strut_embed_value oo;
      strut_embed_error* ee = 0;
      CHECK(strut_embed_invoke(ec, "f", &iv, 1, &oo, &ee) != 0);
      CHECK(ee && strcmp(ee->type, "E") == 0 && strcmp(ee->message, "boom") == 0);
      strut_embed_context_destroy(ec);
      CHECK(strcmp(ee->type, "E") == 0 && strcmp(ee->message, "boom") == 0);   /* still valid after destroy */
      strut_embed_error_release(NULL, ee); }

    /* reload semantics: successful load replaces; failed load keeps the previous active */
    { strut_embed_value n; n.kind = STRUT_EMBED_VALUE_INT; n.i = 4; strut_embed_value oy;
      CHECK(strut_embed_invoke(ctx, "add", args, 2, &oy, &err) == 0 && oy.i == 42);   /* A active */
      strut_embed_error* re = 0;
      CHECK(strut_embed_context_load_source(ctx, "export \"C\" function ok( -> int { return", 40, &re) != 0);
      strut_embed_error_release(ctx, re);
      CHECK(strut_embed_invoke(ctx, "add", args, 2, &oy, &err) == 0 && oy.i == 42);   /* A still active */
      const char* srcb2 = "export \"C\" function twice(int_32 x) -> int_32 { return x * 2; }\n";
      CHECK(strut_embed_context_load_source(ctx, srcb2, strlen(srcb2), &err) == 0);    /* B replaces A */
      CHECK(strut_embed_invoke(ctx, "twice", &n, 1, &oy, &err) == 0 && oy.i == 8);
      CHECK(strut_embed_invoke(ctx, "add", args, 2, &oy, &err) != 0);                 /* A no longer active */
      strut_embed_error_release(ctx, err); err = 0; }

    /* same-ID multi-context: both load the identical logical module independently */
    { strut_embed_context* c1 = strut_embed_context_create();
      strut_embed_context* c2 = strut_embed_context_create();
      strut_embed_error* e1 = 0;
      CHECK(strut_embed_context_load_source(c1, src, strlen(src), &e1) == 0);
      CHECK(strut_embed_context_load_source(c2, src, strlen(src), &e1) == 0);
      strut_embed_value n; n.kind = STRUT_EMBED_VALUE_INT; n.i = 3; strut_embed_value o1, o2;
      CHECK(strut_embed_invoke(c1, "neg", &n, 1, &o1, &e1) == 0 && o1.i == -3);
      CHECK(strut_embed_invoke(c2, "neg", &n, 1, &o2, &e1) == 0 && o2.i == -3);
      strut_embed_context_destroy(c1);
      CHECK(strut_embed_invoke(c2, "neg", &n, 1, &o2, &e1) == 0 && o2.i == -3);
      strut_embed_context_destroy(c2); }

    /* concurrency: two independent contexts on two native threads (no global embedding lock) */
    {
        strut_embed_context* ca = strut_embed_context_create();
        strut_embed_context* cb = strut_embed_context_create();
        strut_embed_error* ce = 0;
        CHECK(strut_embed_context_load_source(ca, src, strlen(src), &ce) == 0);
        CHECK(strut_embed_context_load_source(cb, src, strlen(src), &ce) == 0);
        struct cwork { strut_embed_context* c; int bad; } wa = { ca, 0 }, wb = { cb, 0 };
        embed_thr ta = e_thr(conc_worker, &wa);
        embed_thr tb = e_thr(conc_worker, &wb);
        e_join(ta);
        e_join(tb);
        CHECK(wa.bad == 0 && wb.bad == 0);
        strut_embed_context_destroy(ca);
        strut_embed_context_destroy(cb);
    }
    /* RETAINED callback + module lease lifecycle */
    {
        const char* srcA = "export \"C\" function make_rc(int_32 base) -> retained_callback<(int_32)->int_32> { return retained_callback((int_32 x) => x + base); }\n";
        strut_embed_context* lc = strut_embed_context_create();
        strut_embed_error* le = 0;
        CHECK(strut_embed_context_load_source(lc, srcA, strlen(srcA), &le) == 0);
        strut_embed_value base; memset(&base, 0, sizeof base); base.kind = STRUT_EMBED_VALUE_INT; base.i = 10;
        strut_embed_value rc;
        CHECK(strut_embed_invoke(lc, "make_rc", &base, 1, &rc, &le) == 0 && rc.kind == STRUT_EMBED_VALUE_RETAINED && rc.retained);
        strut_embed_value base2; memset(&base2, 0, sizeof base2); base2.kind = STRUT_EMBED_VALUE_INT; base2.i = 20;
        strut_embed_value rc2;
        CHECK(strut_embed_invoke(lc, "make_rc", &base2, 1, &rc2, &le) == 0 && rc2.kind == STRUT_EMBED_VALUE_RETAINED);
        strut_embed_value x; memset(&x, 0, sizeof x); x.kind = STRUT_EMBED_VALUE_INT; x.i = 5;
        strut_embed_value o;
        CHECK(strut_embed_retained_invoke(lc, &rc, &x, 1, &o, &le) == 0 && o.i == 15);
        CHECK(strut_embed_retained_invoke(lc, &rc2, &x, 1, &o, &le) == 0 && o.i == 25);
        /* A -> B -> C: A stays loaded via leases; retained callbacks keep invoking its code */
        const char* srcB = "export \"C\" function twice(int_32 v) -> int_32 { return v * 2; }\n";
        const char* srcC = "export \"C\" function thrice(int_32 v) -> int_32 { return v * 3; }\n";
        CHECK(strut_embed_context_load_source(lc, srcB, strlen(srcB), &le) == 0);
        CHECK(strut_embed_retained_invoke(lc, &rc, &x, 1, &o, &le) == 0 && o.i == 15);
        CHECK(strut_embed_context_load_source(lc, srcC, strlen(srcC), &le) == 0);
        CHECK(strut_embed_retained_invoke(lc, &rc2, &x, 1, &o, &le) == 0 && o.i == 25);
        /* failed reload: active module and retained callbacks preserved */
        strut_embed_error* fe = 0;
        CHECK(strut_embed_context_load_source(lc, "function bad( -> {", 17, &fe) != 0);
        strut_embed_error_release(lc, fe);
        CHECK(strut_embed_retained_invoke(lc, &rc, &x, 1, &o, &le) == 0 && o.i == 15);
        /* BUSY destroy: context intact, retained callbacks still invoke */
        CHECK(strut_embed_context_destroy(lc) != 0);
        CHECK(strut_embed_retained_invoke(lc, &rc, &x, 1, &o, &le) == 0 && o.i == 15);
        /* secondary-thread invoke with an owned reference */
        { struct ret_work w; w.c = lc; w.r = rc; w.bad = 0; w.result = 0;
          embed_thr th = e_thr(ret_worker, &w); e_join(th);
          CHECK(w.bad == 0 && w.result == 15); }
        /* release one: module A still leased by rc2 */
        strut_embed_value_free(lc, &rc);
        CHECK(strut_embed_retained_invoke(lc, &rc2, &x, 1, &o, &le) == 0 && o.i == 25);
        /* release the last lease: A unloads; destroy now succeeds */
        strut_embed_value_free(lc, &rc2);
        CHECK(strut_embed_context_destroy(lc) == 0);
    }
    strut_embed_context_destroy(ctx);
    if (failures == 0) printf("embed ok\n");
    return failures == 0 ? 0 : 1;
}