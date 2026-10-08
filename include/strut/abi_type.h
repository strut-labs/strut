#pragma once
#include <string>
#include <vector>
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
// FFI-2 release symbols are module-qualified (`<module>_ffi_free_string`/`_bytes`) rather
// than generic (`strut_ffi_free_string`). Two independently built Strut shared libraries
// loaded into one host would otherwise each export the same generic name: that collides on
// the MSVC import-library link step and interposes on ELF/mach-o, so a buffer from library A
// could be released by library B. The module slug is derived deterministically from the
// source module name (the same for codegen and header generation). It must be overridable
// (e.g. --ffi-module) once packages/modules land.
inline std::string abi_module_slug(const std::string& source_path){
    std::size_t slash=source_path.find_last_of("/\\");
    std::string base=(slash==std::string::npos)?source_path:source_path.substr(slash+1);
    std::size_t dot=base.find_last_of('.');
    if(dot!=std::string::npos)base=base.substr(0,dot);
    std::string out;
    for(char c:base)out+=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')?c:'_';
    if(out.empty()||(out[0]>='0'&&out[0]<='9'))out="m_"+out;
    return out;
}
inline std::string abi_release_symbol(const std::string& module,const std::string& kind){
    return module+"_ffi_free_"+kind;
}

// FFI-3 aggregate ABI rule. An aggregate is ABI-safe for `export "C"` only if it is a plain
// value POD: no generics, no bases, no private fields, and every field is an ABI primitive
// (fixed-width int/uint or IEEE float). string/bytes fields (nested ownership), bool
// (deferred), nested aggregates, pointers/references, and collections are deliberately NOT
// permitted yet -- they raise the nested-ownership/ABI questions that belong to a later step.
struct AbiFieldInfo { std::string name; std::string type_name; std::string c_type; };
inline bool abi_aggregate_safe(const std::vector<AbiFieldInfo>& fields, std::string& why){
    if(fields.empty()){ why="aggregate has no fields"; return false; }
    for(const auto& f:fields){
        if(abi_is_transport(f.type_name)){ why="field '"+f.name+"' has type '"+f.type_name+"' (string/bytes ownership in aggregates is not ABI-safe yet)"; return false; }
        if(f.type_name=="void" || !abi_type_info(f.type_name).supported){ why="field '"+f.name+"' has unsupported ABI type '"+f.type_name+"' (only fixed-width int/uint and float fields are allowed)"; return false; }
    }
    return true;
}
// Deterministic, module-qualified C type name for an exported aggregate (avoids collisions
// between same-named structs in different modules once modules land).
inline std::string abi_aggregate_type_name(const std::string& module,const std::string& name){
    return "strut_ffi_"+module+"_"+name;
}
} // namespace strut
