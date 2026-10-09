/* FFI-7 fallible Direction A: a Strut retained_callback<(int_32)->int_32 : RetainedErr> is
 * invoked from C through the opaque handle. invoke() returns an FFI-6 status; on failure the
 * descriptor is backed by per-thread module storage so it remains valid AFTER invoke() returns
 * (copy immediately). No shared mutable backing: concurrent threads see only their own errors. */
#include "export_retained_f.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

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
        if (i % 2 == 0) {
            if (RI(w->handle, i, &out, &e) != 0 || out != i * 2) w->bad = 1;   /* success */
        } else {
            if (RI(w->handle, -1, &out, &e) == 0) w->bad = 1;                  /* failure */
            else if (e.type_len != strlen("RetainedErr") ||
                     memcmp(e.type_data, "RetainedErr", e.type_len) != 0 ||
                     e.message_len != strlen("neg") ||
                     memcmp(e.message_data, "neg", e.message_len) != 0) w->bad = 1;
        }
    }
    return 0;
}

int main(void) {
    RB* h = make_fallible();
    CHECK(h != 0);
    RB* r = RR(h);              /* retained: second owned C reference */
    RL(h);                      /* original owner released while workers will invoke */
    int32_t out; CE e;
    CHECK(RI(r, 21, &out, &e) == 0 && out == 42);
    CHECK(RI(r, -1, &out, &e) != 0);                                   /* declared failure */
    CHECK(e.type_len == strlen("RetainedErr") && memcmp(e.type_data, "RetainedErr", e.type_len) == 0);
    CHECK(e.message_len == strlen("neg") && memcmp(e.message_data, "neg", e.message_len) == 0);
    CHECK(RI(r, 5, &out, &e) == 0 && out == 10);   /* invoke again; descriptor per-invocation/TLS */
    pthread_t th[8];
    worker_ctx w[8];
    for (int i = 0; i < 8; ++i) { w[i].handle = RR(r); w[i].bad = 0; pthread_create(&th[i], 0, worker, &w[i]); }
    RL(r);                      /* main releases its ref while workers hold their own */
    for (int i = 0; i < 8; ++i) { pthread_join(th[i], 0); RL(w[i].handle); }
    for (int i = 0; i < 8; ++i) CHECK(w[i].bad == 0);
    printf("fallible A ok\n");
    return 0;
}
