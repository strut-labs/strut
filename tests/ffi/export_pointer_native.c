/* Native C library for the Strut->native pointer direction (CP68 unsafe extern "C"). */
#include <stdint.h>
void native_inc_raw(int32_t* p){ if(p) *p += 1; }
void native_inc_ref(int32_t* p){ *p += 1; }
int32_t native_read(int32_t* p){ return p ? *p : -1; }
