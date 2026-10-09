/* FFI-7 Direction B: a native-owned callback (fn + ctx + retain_ctx + release_ctx) becomes a
 * native-backed retained_callback inside Strut. The Strut shared state calls retain_ctx once at
 * construction and release_ctx once at final control-block death; ordinary Strut copies never
 * re-trigger native retain/release. After native drops its originating owner and returns, the
 * Strut global remains the sole owner and can still invoke. */
#include "export_retained_b.h"
#include <stdio.h>

typedef struct { int refs; int retain_count; int release_count; int invoke_count; int destroyed; } ctx_t;

static int32_t cb_fn(void* c, int32_t x) { ((ctx_t*)c)->invoke_count++; return x + 100; }
static void retain_ctx(void* c) { ctx_t* k = (ctx_t*)c; k->retain_count++; k->refs++; }
static void release_ctx(void* c) { ctx_t* k = (ctx_t*)c; k->release_count++; if (--k->refs == 0) k->destroyed++; }

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    ctx_t ctx = {1, 0, 0, 0, 0};       /* native original owner: refs = 1 */
    CHECK(adopt_and_store(cb_fn, &ctx, retain_ctx, release_ctx, 5) == 106);
    CHECK(ctx.retain_count == 1);      /* retain_ctx EXACTLY ONCE at state construction */
    CHECK(ctx.invoke_count == 1);      /* invoked during the originating call */
    release_ctx(&ctx);                 /* native drops its original owner after returning */
    CHECK(ctx.refs == 1);              /* State remains the sole owner via Strut global */
    CHECK(run_stored(3) == 206);       /* invoke later through Strut copies */
    CHECK(ctx.retain_count == 1);      /* ordinary Strut copies did NOT call retain_ctx */
    CHECK(ctx.invoke_count == 3);      /* two more invocations through copies */
    CHECK(ctx.release_count == 1);     /* only native's own release so far */
    CHECK(ctx.destroyed == 0);         /* state still alive */
    clear_stored();                    /* replacement destroys the last owner -> release_ctx once */
    CHECK(ctx.release_count == 2);     /* native origin + state destruction */
    CHECK(ctx.refs == 0 && ctx.destroyed == 1);

    ctx_t ctx2 = {1, 0, 0, 0, 0};      /* a second native-backed callback */
    CHECK(adopt_and_store(cb_fn, &ctx2, retain_ctx, release_ctx, 1) == 102);
    CHECK(ctx2.retain_count == 1);
    release_ctx(&ctx2);                /* 2 -> 1 */
    CHECK(run_stored(4) == 208);       /* Strut global still owns and invokes later */
    CHECK(ctx2.invoke_count == 3);
    clear_stored();
    CHECK(ctx2.refs == 0 && ctx2.destroyed == 1 && ctx2.release_count == 2);

    printf("native retained ok\n");
    return 0;
}
