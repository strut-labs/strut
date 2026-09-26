#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace strut {

struct PackageSource {
    std::string kind;
    std::string url;
    std::string revision;
};

struct PackageManifest {
    std::string name;
    std::string version;
    std::string entry;
    std::map<std::string, std::string> dependencies;
    std::map<std::string, PackageSource> dependency_sources;
    std::string description;
    std::string license;
    std::string repository;
    std::vector<std::string> sources;
    std::vector<std::string> include_dirs;
};

struct LockedPackage {
    std::string name;
    std::string requested;
    std::string version;
    std::string source_kind;
    std::string source;
    std::string revision;
    std::string checksum;
    bool direct = false;
    std::map<std::string, std::string> dependencies;
};

struct PackageLock {
    unsigned schema_version = 2;
    std::vector<LockedPackage> packages;
};

bool valid_version_requirement(const std::string& value);
bool parse_package_manifest(const std::string& text, PackageManifest& out, std::string& error);
std::filesystem::path package_cache_root();
bool valid_package_name(const std::string& value);
bool valid_package_relative_path(const std::string& value);
bool load_package_manifest_file(const std::filesystem::path& path, PackageManifest& out, std::string& error);
bool write_package_manifest_file(const std::filesystem::path& path, const PackageManifest& manifest, std::string& error);
std::optional<std::filesystem::path> resolve_cached_package(const std::string& name, const std::string& requirement, std::string* error = nullptr);
bool verify_cached_package(const std::filesystem::path& cached_root, std::string& checksum, std::string& error);
bool cache_local_package(const std::filesystem::path& source_root, std::filesystem::path& cached_root, PackageManifest& manifest, std::string& error);
bool acquire_git_package(const PackageSource& source, std::filesystem::path& cached_root, PackageManifest& manifest, std::string& error);
bool parse_package_lock(const std::string& text, PackageLock& out, std::string& error);
bool load_package_lock_file(const std::filesystem::path& path, PackageLock& out, std::string& error);
bool write_package_lock_file(const std::filesystem::path& path, const PackageLock& lock, std::string& error);
bool validate_package_lock(const PackageLock& lock, const PackageManifest* manifest, std::string& error);
bool package_content_checksum(const std::filesystem::path& root, std::string& checksum, std::string& error);
bool write_lockfile(const std::filesystem::path& project_root, const PackageManifest& manifest, std::string& error);
bool install_packages(const std::filesystem::path& project_root, bool offline, bool update, PackageLock& lock, std::string& error);

} // namespace strut
