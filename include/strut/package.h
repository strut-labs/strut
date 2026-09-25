#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace strut {

struct PackageManifest {
    std::string name;
    std::string version;
    std::string entry;
    std::map<std::string, std::string> dependencies;
};

bool valid_version_requirement(const std::string& value);
bool parse_package_manifest(const std::string& text, PackageManifest& out, std::string& error);
std::filesystem::path package_cache_root();

} // namespace strut
