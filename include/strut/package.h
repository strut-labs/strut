#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace strut {

struct PackageManifest {
    std::string name;
    std::string version;
    std::string entry;
    std::map<std::string, std::string> dependencies;
    std::string description;
    std::string license;
    std::string repository;
    std::vector<std::string> sources;
    std::vector<std::string> include_dirs;
};

bool valid_version_requirement(const std::string& value);
bool parse_package_manifest(const std::string& text, PackageManifest& out, std::string& error);
std::filesystem::path package_cache_root();
bool valid_package_name(const std::string& value);
bool valid_package_relative_path(const std::string& value);

} // namespace strut
