/* Native C library for the Strut->native checked-error direction. */
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t code; const char* type; size_t type_len; const char* message; size_t message_len; } native_error;
int32_t native_parse(int32_t x, int32_t* out_value, native_error* out_error){
    if(x < 0){ out_error->code=7; out_error->type="ParseError"; out_error->type_len=10; out_error->message="bad"; out_error->message_len=3; return 1; }
    *out_value = x * 10; return 0;
}
