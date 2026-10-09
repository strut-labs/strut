/* FFI-7 fallible Direction B: a native-owned fallible callback (fn reports an FFI-6
 * callback_error descriptor; ctx + retain_ctx + release_ctx) becomes a native-backed
 * retained_callback<(int_32)->int_32 : RetainedErr>. The wrapper immediately copies the
 * descriptor (type/message) into a strut_checked_error and Strut checked-error flow handles
 * it; retain-once/release-once native ownership is unchanged. */
#include "export_retained_bf.h"
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

typedef struct { int refs; int retain_count; int release_count; int invoke_count; int destroyed; int base; } ctx_t;

static int32_t nf_fn(void* c, int32_t x, int32_t* out, strut_ffi_export_retained_bf_d5e480c66036e5b9b72f6fb2939d9bf1_callback_error* e) {
    ctx_t* k = (ctx_t*)c; k->invoke_count++;
    if (x < 0) { e->type_data = "RetainedErr"; e->type_len = 11; e->message_data = "from_native"; e->message_len = 11; e->code = 7; return 1; }
    *out = x + k->base; return 0;
}
static void retain_ctx(void* c) { ctx_t* k = (ctx_t*)c; k->retain_count++; k->refs++; }
static void release_ctx(void* c) { ctx_t* k = (ctx_t*)c; k->release_count++; if (--k->refs == 0) k->destroyed++; }

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

typedef struct { int result; } thread_out;
static void* b_worker(void* a) { thread_out* o = (thread_out*)a; o->result = run_stored_nf(7); return 0; }   /* cross-thread Direction B */

int main(void) {
    ctx_t ctx = {1, 0, 0, 0, 0, 100};
    CHECK(store_nfactory(nf_fn, &ctx, retain_ctx, release_ctx, 5) == 105);   /* success during origin */
    CHECK(ctx.retain_count == 1 && ctx.invoke_count == 1);
    release_ctx(&ctx);                                    /* native drops origin: 2 -> 1 */
    CHECK(ctx.refs == 1);
    CHECK(run_stored_nf(3) == 206);                       /* invoke later via copies, success */
    CHECK(ctx.retain_count == 1 && ctx.invoke_count == 3);
    CHECK(run_stored_nf(-3) == -1);                       /* declared error propagated through Strut */
    CHECK(ctx.invoke_count == 4);   /* a(-3) errors; b(-3) never runs */
    thread_out to; strut_thr th;
    th = thr_create(b_worker, &to);                              /* second thread invokes native-backed retained */
    thr_join(th);
    CHECK(to.result == 214);                                     /* (7 + 100) * 2 from a second thread */
    CHECK(ctx.retain_count == 1 && ctx.invoke_count == 6);       /* cross-thread, no extra native retain */
    CHECK(ctx.release_count == 1 && ctx.destroyed == 0);
    clear_stored_nf();
    CHECK(ctx.refs == 0 && ctx.destroyed == 1 && ctx.release_count == 2);
    printf("fallible B ok\n");
    return 0;
}
