/* FFI-7 fallible Direction A + same-thread nested TLS reentrancy + cross-thread stress.
 * invoke() reports an FFI-6 status; on failure the callback_error descriptor is backed by
 * per-thread module TLS strings valid AFTER invoke() returns (copy immediately).
 * The nested case (native -> Strut outer -> native bridge -> Strut inner) proves the inner
 * descriptor is copied before the outer failure overwrites the SAME thread TLS backing. */
#include "export_retained_f.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
typedef HANDLE strut_thr;
static strut_thr thr_create(void* (*fn)(void*), void* a) { DWORD id; return CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(void*)fn, a, 0, &id); }
static void thr_join(strut_thr h) { WaitForSingleObject(h, INFINITE); CloseHandle(h); }
#else
#include <pthread.h>
typedef pthread_t strut_thr;
static strut_thr thr_create(void* (*fn)(void*), void* a) { pthread_t t; pthread_create(&t, 0, fn, a); return t; }
static void thr_join(strut_thr t) { pthread_join(t, 0); }
#endif

#define RB EXPORT_RETAINED_F_RETAINED_CB_FC7D9C2C6D746598
#define RR EXPORT_RETAINED_F_RETAINED_RETAIN_FC7D9C2C6D746598
#define RL EXPORT_RETAINED_F_RETAINED_RELEASE_FC7D9C2C6D746598
#define RI EXPORT_RETAINED_F_RETAINED_INVOKE_FC7D9C2C6D746598
#define CE strut_ffi_export_retained_f_94441cf979056529a3b17f95b33df669_callback_error
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

typedef struct { RB* handle; volatile int bad; } worker_ctx;
static void* worker(void* a) {
    worker_ctx* w = (worker_ctx*)a;
    for (int i = 0; i < 2000; ++i) {
        int32_t out; CE e;
        if (i % 2 == 0) { if (RI(w->handle, i, &out, &e) != 0 || out != i * 2) w->bad = 1; }
        else { if (RI(w->handle, -1, &out, &e) == 0) w->bad = 1;
               else if (e.type_len != 11 || memcmp(e.type_data, "RetainedErr", 11) != 0 ||
                        e.message_len != 3 || memcmp(e.message_data, "neg", 3) != 0) w->bad = 1; }
    }
    return 0;
}

/* native bridge invoked by Strut 'outerfn' during an outer retained invocation; it performs an
 * INNER fallible retained invoke on the same thread and copies that descriptor immediately. */
static char inner_type[32], inner_msg[32];
static int inner_type_len, inner_msg_len;
static int32_t bridge_inner(void* c, int32_t x) { (void)x;
    RB* inner = (RB*)c; int32_t out; CE ei;
    if (RI(inner, -1, &out, &ei) != 0) {
        inner_type_len = ei.type_len > 31 ? 31 : ei.type_len;
        inner_msg_len = ei.message_len > 31 ? 31 : ei.message_len;
        memcpy(inner_type, ei.type_data, inner_type_len); inner_type[inner_type_len] = 0;
        memcpy(inner_msg, ei.message_data, inner_msg_len); inner_msg[inner_msg_len] = 0;
        return 0;
    }
    return 1;
}

int main(void) {
    RB* h = make_fallible();
    RB* r = RR(h); RL(h);
    int32_t out; CE e;
    CHECK(RI(r, 21, &out, &e) == 0 && out == 42);
    CHECK(RI(r, -1, &out, &e) != 0);
    CHECK(e.type_len == 11 && memcmp(e.type_data, "RetainedErr", 11) == 0 &&
          e.message_len == 3 && memcmp(e.message_data, "neg", 3) == 0);   /* read AFTER invoke returned */
    CHECK(RI(r, 5, &out, &e) == 0 && out == 10);                          /* invoke again after failure */

    /* same-thread nested TLS reentrancy: outer invoke -> Strut outerfn -> native bridge ->
     * inner fallible invoke (failure) -> bridge copies immediately -> outer continues -> outer
     * failure overwrites the same TLS backing. Both copies must remain correct. */
    RB* inner_handle = make_fallible();
    RB* outer_handle = make_reentrant(bridge_inner, inner_handle, 0, 0);
    CHECK(outer_handle != 0);
    CHECK(RI(outer_handle, 5, &out, &e) == 0 && out == 15);               /* success nesting, no error */
    CHECK(RI(outer_handle, -1, &out, &e) != 0);                           /* outer failure */
    CHECK(e.type_len == 11 && memcmp(e.type_data, "RetainedErr", 11) == 0 &&
          e.message_len == 5 && memcmp(e.message_data, "outer", 5) == 0); /* outer descriptor: overwritten TLS */
    CHECK(inner_type_len == 11 && memcmp(inner_type, "RetainedErr", 11) == 0 &&
          inner_msg_len == 3 && memcmp(inner_msg, "neg", 3) == 0);       /* inner copy survived the TLS overwrite */
    RL(outer_handle); RL(inner_handle);

    strut_thr th[8]; worker_ctx w[8];
    for (int i = 0; i < 8; ++i) { w[i].handle = RR(r); w[i].bad = 0; th[i] = thr_create(worker, &w[i]); }
    RL(r);                            /* main releases its ref while workers hold their own */
    for (int i = 0; i < 8; ++i) { thr_join(th[i]); RL(w[i].handle); }
    for (int i = 0; i < 8; ++i) CHECK(w[i].bad == 0);
    printf("fallible A ok\n");
    return 0;
}
