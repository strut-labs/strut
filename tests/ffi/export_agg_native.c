/* Native C library used by the Strut->native aggregate direction. Its struct layout must
 * match the generated FFI-3 ABI POD (two int32 fields) -- proving one aggregate ABI serves
 * both directions. */
#include <stdint.h>
typedef struct { int32_t a; int32_t b; } NativePair;
NativePair native_pair_make(int32_t a, int32_t b) { NativePair p; p.a = a; p.b = b; return p; }
int32_t native_pair_sum(NativePair p) { return p.a + p.b; }
