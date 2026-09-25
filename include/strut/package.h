#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <optional>

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
bool load_package_manifest_file(const std::filesystem::path& path, PackageManifest& out, std::string& error);
bool write_package_manifest_file(const std::filesystem::path& path, const PackageManifest& manifest, std::string& error);
std::optional<std::filesystem::path> resolve_cached_package(const std::string& name, const std::string& requirement);
bool cache_local_package(const std::filesystem::path& source_root, std::filesystem::path& cached_root, PackageManifest& manifest, std::string& error);
bool write_lockfile(const std::filesystem::path& project_root, const PackageManifest& manifest, std::string& error);

} // namespace strut
