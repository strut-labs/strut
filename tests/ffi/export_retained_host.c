/* FFI-7 Direction A: an exported Strut retained_callback<sig> crosses to native as an
 * opaque C handle (own atomic C refcount + one internal retained_callback value).
 * retain/release manage the C refcount only; invoke requires an already-live owned handle. */
#include "export_retained.h"
#include <stdio.h>

#define RB EXPORT_RETAINED_RETAINED_CB_7B5FE6280318FD17
#define RR EXPORT_RETAINED_RETAINED_RETAIN_7B5FE6280318FD17
#define RL EXPORT_RETAINED_RETAINED_RELEASE_7B5FE6280318FD17
#define RI EXPORT_RETAINED_RETAINED_INVOKE_7B5FE6280318FD17
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    RB* h = make_adder(10);
    CHECK(h != 0);
    CHECK(RI(h, 5) == 15);           /* invoke after the exporting Strut frame returned */
    CHECK(RI(h, 1) == 11);           /* captured base survives the originating scope */
    RB* r = RR(h);                   /* retain -> second owned C reference */
    CHECK(r == h);
    CHECK(RI(h, 2) == 12 && RI(r, 2) == 12);
    CHECK(RI(RR(r), 100) == 110);    /* nested retain/invoke without leaking the ref */
    CHECK(apply_retained(r, 7) == 17); /* pass the C-held handle back into a Strut frame */
    RL(h);                           /* drop the original owned reference */
    CHECK(RI(r, 3) == 13);           /* the remaining owned reference still invokes */
    for (int i = 0; i < 10000; ++i) CHECK(RI(r, i) == 10 + i);   /* invoke stress */
    for (int i = 0; i < 10000; ++i) RL(RR(r));                   /* retain/release stress */
    RL(r);                           /* final release destroys the callback state once */
    printf("retained ok\n");
    return 0;
}
