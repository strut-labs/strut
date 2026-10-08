#pragma once
#include <string>
#include <vector>
#include "strut/abi_type.h"
#include "strut/ir.h"
namespace strut {
// FFI-3: the set of user structs that are ABI-safe for `export "C"`, in declaration order.
// Single source of truth for both codegen (typedefs + wrapper marshalling) and C-header
// generation. Structs with generics/bases/private fields or non-primitive fields are omitted
// (and rejected at sema with a precise reason).
struct AbiAggregate { std::string name; std::vector<AbiFieldInfo> fields; };
inline std::vector<AbiAggregate> collect_abi_aggregates(const IRProgram& program){
    std::vector<AbiAggregate> out;
    for(const auto& sd:program.statements){
        if(sd->kind!=IRStmt::Kind::struct_decl)continue;
        if(!sd->generic_parameters.empty()||!sd->bases.empty())continue;
        std::vector<AbiFieldInfo> fields;bool priv=false;
        for(const auto& f:sd->fields){
            if(f.is_private)priv=true;
            fields.push_back(AbiFieldInfo{f.name,f.type.name,std::string(abi_type_info(f.type.name).c_type)});
        }
        if(priv)continue;
        std::string why;
        if(!abi_aggregate_safe(fields,why))continue;
        out.push_back(AbiAggregate{sd->name,fields});
    }
    return out;
}
} // namespace strut
