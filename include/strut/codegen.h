#pragma once
#include <filesystem>
#include <string>
#include "strut/ir.h"
namespace strut {
struct CodegenResult { std::string cpp; std::string error; bool ok() const { return error.empty(); } };
class CppBackend {
public:
    CodegenResult generate(const IRProgram& program) const;
    bool compile(const IRProgram& program, const std::filesystem::path& output, std::string& error) const;
};
}
