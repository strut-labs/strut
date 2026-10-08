#pragma once
#include <string>
#include <vector>
#include <cstdint>
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
// Stable, self-defined 128-bit digest (two FNV-1a 64-bit passes over the canonical identity
// and its reverse). NOT std::hash and NOT implementation-defined; algorithm, input, and
// output length are fixed here. No cryptographic strength is required -- 128 bits makes an
// accidental ABI-namespace collision non-credible.
inline std::uint64_t abi_fnv1a64(const std::string& s,std::uint64_t basis){
    std::uint64_t h=basis;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;
}
inline std::string abi_hex64(std::uint64_t v){
    const char* H="0123456789abcdef";std::string o;o.reserve(16);
    for(int i=15;i>=0;--i){o+=H[(v>>(i*4))&0xF];}return o;
}
inline std::string abi_digest128(const std::string& canonical){
    const std::uint64_t h1=abi_fnv1a64(canonical,1469598103934665603ULL);
    const std::string rev(canonical.rbegin(),canonical.rend());
    const std::uint64_t h2=abi_fnv1a64(rev,1099511628211ULL);
    return abi_hex64(h1)+abi_hex64(h2);
}
// Canonical ABI module identity is the LOGICAL module path AS PROVIDED to the compiler
// (project/package-relative when builds invoke with relative source paths) -- NEVER the
// build machine's absolute filesystem path. This makes the same project built under different
// checkout roots / CI workspaces / package-cache paths produce the SAME public ABI names.
// The ABI namespace = readable sanitized prefix + an always-present 128-bit deterministic
// digest of that logical identity (no 32-bit surface; no sanitization-form ambiguity). This is
// a deterministic STRONGLY-DISAMBIGUATED namespace, NOT a proof of injectivity (two 64-bit
// FNV-1a passes make an accidental collision non-credible, not impossible). Hosts reference
// the readable module-qualified macro aliases emitted by the generated header, not the digest.
// Callers must pass a stable (typically relative) source path for reproducible ABI names.
inline std::string abi_sanitize(const std::string& s,bool keep_case,const char* fallback){
    std::string out;
    for(char c:s){out+=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')?c:'_';}
    if(out.empty()||(out[0]>='0'&&out[0]<='9'))out=std::string(fallback)+out;
    if(!keep_case)for(char& c:out)c=(c>='a'&&c<='z')?(char)(c-'a'+'A'):c;
    return out;
}
inline std::string abi_module_ident(const std::string& id){
    std::size_t slash=id.find_last_of("/\\");
    std::string base=(slash==std::string::npos)?id:id.substr(slash+1);
    std::size_t dot=base.find_last_of('.');
    if(dot!=std::string::npos)base=base.substr(0,dot);
    return abi_sanitize(base,true,"m_");
}
inline std::string abi_module_slug(const std::string& id){
    return abi_module_ident(id)+"_"+abi_digest128(id);
}
// Module-qualified, digest-free, CHECKOUT-INDEPENDENT macro prefix. Uses the full logical id
// (directory components included) so `pkgA/util.p` and `pkgB/util.p` -- and two headers that
// both define `Pair` -- get distinct aliases (`PKGA_UTIL_Pair` vs PKGB_UTIL_Pair`).
inline std::string abi_module_macro(const std::string& id){
    std::string raw=id;
    const std::size_t slash=raw.find_last_of("/\\");
    const std::size_t dot=raw.find_last_of('.');
    if(dot!=std::string::npos&&(slash==std::string::npos||slash<dot))raw=raw.substr(0,dot);
    return abi_sanitize(raw,false,"M_");
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

// FFI-4 borrowed pointer/reference ABI. Only `raw_ptr<T>` (borrowed, possibly null) and
// `ref<T>` (borrowed, non-null) are ABI candidates, and only when T has a direct primitive C
// representation. The C shape is `T_c*` for BOTH, but the semantic contract differs
// (raw_ptr may be null; ref must be non-null). `ptr<T>` (owning) and `weak_ptr<T>` are
// deliberately NOT representable and must be rejected. Pointer RETURNS are not supported
// (no defined lifetime/provenance yet).
inline bool abi_pointer_inner(const std::string& name,bool& is_ref,std::string& inner){
    if(name.size()>9 && name.rfind("raw_ptr<",0)==0 && name.back()=='>'){is_ref=false;inner=name.substr(8,name.size()-9);return true;}
    if(name.size()>5 && name.rfind("ref<",0)==0 && name.back()=='>'){is_ref=true;inner=name.substr(4,name.size()-5);return true;}
    return false;
}
inline bool abi_pointer_supported(const std::string& name,bool& is_ref,std::string& c_type){
    std::string inner;
    if(!abi_pointer_inner(name,is_ref,inner))return false;
    if(inner=="void"||!abi_type_info(inner).supported)return false;
    c_type=std::string(abi_type_info(inner).c_type)+"*";
    return true;
}
// True for pointer-like syntaxes that are intentionally NOT ABI-representable (so sema can
// give an ownership-specific diagnostic rather than a generic one).
inline bool abi_pointer_owning(const std::string& name){
    return (name.rfind("ptr<",0)==0&&name.back()=='>')||(name.rfind("weak_ptr<",0)==0&&name.back()=='>');
}

// FFI-5 callback ABI. A Strut function value `function<(args)->ret>` (canonical spelling) is a
// synchronous, borrowed, same-thread callback. It lowers to a C function pointer whose first
// parameter is an opaque `void* context`: `ret (*)(void* context, args...)`. Only primitive
// int/uint/float parameters and a primitive-or-void return are accepted initially; checked
// errors (` : `) and async (`future<`) callbacks are rejected. The C++ closure type never
// crosses the ABI.
inline bool abi_callback_supported(const std::string& name,std::string& ret,std::vector<std::string>& args){
    args.clear();ret.clear();
    if(name.rfind("function<(",0)!=0||name.back()!='>')return false;
    if(name.find("future<")!=std::string::npos)return false;
    if(name.find(" : ")!=std::string::npos)return false; // checked errors (FFI-6)
    const std::size_t arrow=name.rfind(")->");
    if(arrow==std::string::npos)return false;
    const std::string argpart=name.substr(10,arrow-10);
    ret=name.substr(arrow+3,name.size()-1-(arrow+3));
    if(!argpart.empty()){
        std::size_t p=0;
        while(true){std::size_t c=argpart.find(',',p);std::string a=(c==std::string::npos)?argpart.substr(p):argpart.substr(p,c-p);args.push_back(a);if(c==std::string::npos)break;p=c+1;}
    }
    for(const auto& a:args)if(a=="void"||!abi_type_info(a).supported)return false;
    if(ret!="void"&&!abi_type_info(ret).supported)return false;
    return true;
}
// Deterministic callback typedef name, derived from the callback signature (not discovery
// order), scoped by the digest-qualified module namespace.
inline std::string abi_callback_type_name(const std::string& module,const std::string& sig){
    return "strut_ffi_"+module+"_cb_"+abi_digest128(sig).substr(0,16);
}
} // namespace strut
