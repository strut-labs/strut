#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "strut/ir.h"
namespace strut {
struct CodegenResult { std::string cpp; std::string error; bool ok() const { return error.empty(); } };
enum class NativeLinkMode { platform_default, static_link, dynamic_link };
struct NativeLibrary { std::string value; NativeLinkMode mode = NativeLinkMode::platform_default; };
struct NativeLinkOptions {
    std::vector<std::filesystem::path> search_paths;
    std::vector<NativeLibrary> libraries;
    bool fully_static = false;
    bool prefer_dynamic = false;
    bool release = false;
    std::string target = "native";
};
std::string classify_native_failure(const IRProgram& program, std::string_view phase);
class CppBackend {
public:
    CodegenResult generate(const IRProgram& program) const;
    bool compile(const IRProgram& program, const std::filesystem::path& output, std::string& error, const NativeLinkOptions& link = {}) const;
    bool compile_object(const IRProgram& program, const std::filesystem::path& object, const std::filesystem::path& generated_cpp, std::string& error, const NativeLinkOptions& link = {}) const;
    bool link_objects(const IRProgram& program, const std::vector<std::filesystem::path>& objects, const std::filesystem::path& output, std::string& error, const NativeLinkOptions& link = {}) const;
};
}
