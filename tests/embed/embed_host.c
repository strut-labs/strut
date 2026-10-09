/* FFI-8 embedding: an ordinary C host drives Strut in-process through the opaque
 * strut_embed_* surface (create/destroy, source load, invoke, structured errors).
 * No CLI spawn, no C++ types across the boundary. */
#include "strut/embed.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); ++failures; } } while (0)

int main(void) {
    strut_embed_context* ctx = strut_embed_context_create();
    CHECK(ctx != 0);

    static const char* src =
        "export \"C\" function add(int_32 a, int_32 b) -> int_32 { return a + b; }\n"
        "export \"C\" function greet(string name) -> string { return \"hi \" + name; }\n"
        "export \"C\" function neg(int_32 x) -> int_32 { return -x; }\n"
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
    const char* epath = "/tmp/opencode/embed_src.p";
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
    strut_embed_context_destroy(fctx);

    strut_embed_context_destroy(ctx);
    if (failures == 0) printf("embed ok\n");
    return failures == 0 ? 0 : 1;
}