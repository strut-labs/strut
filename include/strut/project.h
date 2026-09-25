#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace strut {

struct BuildConfig {
    std::string entrypoint = "main.p";
    std::string output = "app";
    std::string target = "native";
    std::string mode = "debug";
    std::string linking = "dynamic";
    std::string incremental = "modified";
};

bool parse_build_config(const std::string& text, BuildConfig& out, std::string& error);
bool load_build_config(const std::filesystem::path& path, BuildConfig& out, std::string& error);
bool write_build_config(const std::filesystem::path& path, const BuildConfig& config, std::string& error);
bool init_project_build_state(const std::filesystem::path& root, std::string& error);
std::filesystem::path find_project_root(std::filesystem::path start);

} // namespace strut

namespace strut {
struct ObjectBuildInfo {
    std::string source;
    std::string object;
    std::vector<std::string> dependencies;
    std::string compiler_version;
    std::string target;
    std::string mode;
    std::string fingerprint;
};
bool load_object_build_info(const std::filesystem::path& path, ObjectBuildInfo& out, std::string& error);
bool write_object_build_info(const std::filesystem::path& path, const ObjectBuildInfo& info, std::string& error);
std::string build_fingerprint(const BuildConfig& config, bool release);
} // namespace strut
