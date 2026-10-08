/* Native C library for the Strut->native checked-error direction. */
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t code; const char* type; size_t type_len; const char* message; size_t message_len; } native_error;
static void set_err(native_error* e, int32_t code, const char* type, size_t tl, const char* msg, size_t ml){ e->code=code;e->type=type;e->type_len=tl;e->message=msg;e->message_len=ml; }
int32_t native_parse(int32_t x, int32_t* out_value, native_error* out_error){
    if(x < 0){ set_err(out_error,7,"ParseError",10,"bad",3); return 1; }
    *out_value = x * 10; return 0;
}
int32_t native_pick(int32_t x, int32_t* out_value, native_error* out_error){
    if(x == 1){ set_err(out_error,1,"ParseError",10,"one",3); return 1; }
    if(x == 2){ set_err(out_error,2,"OtherError",10,"two",3); return 1; }
    *out_value = x; return 0;
}
int32_t native_bad(int32_t x, int32_t* out_value, native_error* out_error){
    (void)x; (void)out_value; set_err(out_error,99,"Mystery",7,"m",1); return 1;
}
