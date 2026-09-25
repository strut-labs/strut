#include <cstdlib>
#include <iostream>
#include "strut/type.h"
namespace{void r(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}}}
int main(){using namespace strut;r(builtin_type("int_8").bits==8,"int8");r(builtin_type("uint_64").kind==TypeKind::unsigned_int,"uint64");r(builtin_type("double_32").kind==TypeKind::floating,"double32");r(infer_integer_literal("42").name=="int_32","small literal int32");r(infer_integer_literal("2147483648").name=="int_64","large literal int64");r(infer_floating_literal("2.5").name=="double_32","float defaults double32");r(integer_literal_fits("127",builtin_type("int_8")),"127 fits");r(!integer_literal_fits("128",builtin_type("int_8")),"128 does not fit");r(can_implicitly_convert(builtin_type("int_8"),builtin_type("int_32")),"widen signed");r(!can_implicitly_convert(builtin_type("int_64"),builtin_type("int_32")),"no narrowing");return 0;}
