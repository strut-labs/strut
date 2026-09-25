#pragma once
#include <filesystem>
#include <string>

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
