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
// FFI-2 ABI transport types. `string`/`bytes` are NOT primitives and do NOT map to a
// single C type; they are exchanged as an explicit borrowed data/length pair on input and
// an explicit owned data/length out-parameter pair on output. These are ABI plumbing for
// FFI-2 only -- NOT general Strut pointers (FFI-4) or user aggregates (FFI-3).
inline bool abi_is_string(const std::string& name){ return name=="string"; }
inline bool abi_is_bytes(const std::string& name){ return name=="bytes"; }
inline bool abi_is_transport(const std::string& name){ return abi_is_string(name)||abi_is_bytes(name); }
// Element type used for the borrowed/owned data pointer of a transport type.
inline const char* abi_transport_c_element(const std::string& name){
    if(abi_is_bytes(name))return "uint8_t";
    return "char";
}
// Whether a type may appear in an export "C" signature. string/bytes are accepted only
// through the explicit FFI-2 transport lowering above. bool remains deliberately deferred.
inline bool abi_type_supported(const std::string& name){
    return abi_type_info(name).supported || abi_is_transport(name);
}
} // namespace strut
