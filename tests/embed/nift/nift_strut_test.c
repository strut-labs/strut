// FFI-10 Nift <-> Strut vertical slice: one in-process host exercises BOTH public C ABIs.
//   Direction A (Nift host -> Strut): a real Strut module exports add() and is compiled+invoked
//     through libstrut_embed; its result (42) is bound into a Nift engine, and a real Nift
//     expression evaluates against it and returns 42 via nift_script_result_value_json.
//   Direction B (Strut <- Nift): a real Nift expression computes 40 + 2; its numeric result is
//     fed back as the argument to a second Strut invocation (add(0, niftValue)) and the result
//     is verified as 42.
// No subprocess, no shell: both directions go in-process through nift/c_abi.h (ABI 1.3) and
// strut/embed.h (FFI-8).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "strut/embed.h"
#include "nift/c_abi.h"

static int failed = 0;
#define CHECK(x, m) do { if (!(x)) { printf("NIFT-STRUT FAIL: %s\n", m); failed = 1; } } while (0)

static const char* SRC =
    "export \"C\" function add(int_32 a, int_32 b) -> int_32 { return a + b; }\n"
    "export \"C\" function idp(int_32 x) -> int_32 { return x; }\n";

int main(void) {
    /* --- Strut side (FFI-8 embedding) --- */
    strut_embed_context* ctx = strut_embed_context_create();
    CHECK(ctx != NULL, "strut context create");
    strut_embed_error* se = NULL;
    CHECK(strut_embed_context_load_source(ctx, SRC, strlen(SRC), &se) == 0, "strut load source");
    if (se) { printf("load diag: %s\n", se->message ? se->message : ""); strut_embed_error_release(ctx, se); }

    strut_embed_value args[2] = {{.kind = STRUT_EMBED_VALUE_INT, .i = 20},
                                 {.kind = STRUT_EMBED_VALUE_INT, .i = 22}};
    strut_embed_value out = {0};
    CHECK(strut_embed_invoke(ctx, "add", args, 2, &out, &se) == 0 && out.kind == STRUT_EMBED_VALUE_INT && out.i == 42,
          "strut add(20,22)=42");

    /* --- Nift side (C ABI 1.3) --- */
    nift_engine* ne = nift_engine_new();
    CHECK(ne != NULL, "nift engine new");
    if (ne) {
        /* Direction A: bind the Strut-computed value into Nift and evaluate against it. */
        nift_engine_set_int(ne, "answer", 6, (int32_t)out.i);
        nift_script_result* r = NULL;
        const char* exprA = "answer + 0";
        nift_status st = nift_engine_evaluate(ne, exprA, strlen(exprA), &r);
        CHECK(st == NIFT_OK && r != NULL, "nift evaluate A");
        int okA = r ? nift_script_result_ok(r) : 0;
        CHECK(okA == 1, "nift result A ok");
        if (r) {
            nift_string v = {0}; nift_status vj = nift_script_result_value_json(r, &v);
            CHECK(vj == NIFT_OK && v.length == 2 && memcmp(v.data, "42", 2) == 0, "nift A json == 42");
            nift_script_result_free(r);
        }

        /* Direction B: Nift computes 40 + 2; feed that number back into Strut idp() and verify 42. */
        nift_script_result* rb = NULL;
        const char* exprB = "40 + 2";
        st = nift_engine_evaluate(ne, exprB, strlen(exprB), &rb);
        CHECK(st == NIFT_OK && rb != NULL, "nift evaluate B");
        if (rb) {
            nift_string v = {0}; nift_status vj = nift_script_result_value_json(rb, &v);
            int niftValue = -1;
            if (vj == NIFT_OK && v.data) {
                char buf[32] = {0};
                size_t n = v.length < 31 ? v.length : 31;
                memcpy(buf, v.data, n);
                niftValue = atoi(buf);
            }
            CHECK(niftValue == 42, "nift B computes 42");
            strut_embed_value a1[] = {{.kind = STRUT_EMBED_VALUE_INT, .i = niftValue}};
            strut_embed_value outb = {0};
            strut_embed_error* eb = NULL;
            CHECK(strut_embed_invoke(ctx, "idp", a1, 1, &outb, &eb) == 0 && outb.kind == STRUT_EMBED_VALUE_INT && outb.i == 42,
                  "strut idp accepts nift value -> 42");
            nift_script_result_free(rb);
        }
        nift_engine_free(ne);
    }

    /* --- cleanup --- */
    strut_embed_value v = {0}; v.kind = STRUT_EMBED_VALUE_INT; strut_embed_value_free(ctx, &v); /* no-op */
    CHECK(strut_embed_context_destroy(ctx) == 0, "strut context destroy");
    if (failed) { printf("nift strut FAILED\n"); return 1; }
    printf("nift strut ok\n");
    return 0;
}