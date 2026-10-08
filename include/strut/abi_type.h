#pragma once
#include <string>
namespace strut {
// Single source of truth for the FFI-1 export C ABI primitive type table.
// Used by semantic validation (supported) and C-header generation (c_type) so the
// accepted set and the emitted host declaration cannot drift.
struct AbiTypeInfo { bool supported=false; const char* c_type="void"; };
inline AbiTypeInfo abi_type_info(const std::string& name){
    if(name=="void")return {true,"void"};
    if(name=="int"||name=="int_32")return {true,"int32_t"};
    if(name=="int_8")return {true,"int8_t"};
    if(name=="int_16")return {true,"int16_t"};
    if(name=="int_64")return {true,"int64_t"};
    if(name=="uint"||name=="uint_32")return {true,"uint32_t"};
    if(name=="uint_8")return {true,"uint8_t"};
    if(name=="uint_16")return {true,"uint16_t"};
    if(name=="uint_64")return {true,"uint64_t"};
    if(name=="double_32")return {true,"float"};
    if(name=="double_64")return {true,"double"};
    return {false,"void"};
}
} // namespace strut
