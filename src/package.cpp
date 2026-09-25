#include "strut/package.h"

#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string_view>

#include "json.h"

namespace strut {
namespace {
bool parse_part(std::string_view value, std::size_t& offset) {
    if (offset >= value.size()) return false;
    unsigned part = 0;
    const char* first = value.data() + offset;
    const char* last = value.data() + value.size();
    auto result = std::from_chars(first, last, part);
    if (result.ec != std::errc{} || result.ptr == first) return false;
    offset = static_cast<std::size_t>(result.ptr - value.data());
    return true;
}
bool valid_semver(std::string_view value) {
    std::size_t offset = 0;
    for (int i = 0; i < 3; ++i) {
        if (!parse_part(value, offset)) return false;
        if (i != 2) {
            if (offset >= value.size() || value[offset] != '.') return false;
            ++offset;
        }
    }
    return offset == value.size();
}
}


bool valid_package_name(const std::string& value) {
    if (value.empty()) return false;
    for (const unsigned char c : value) {
        if (!(std::islower(c) || std::isdigit(c) || c == '-' || c == '_')) return false;
    }
    return value.front() != '-' && value.front() != '_' && value.back() != '-' && value.back() != '_';
}

bool valid_package_relative_path(const std::string& value) {
    if (value.empty()) return false;
    const std::filesystem::path path(value);
    if (path.is_absolute()) return false;
    for (const auto& part : path) if (part == "..") return false;
    return true;
}

bool valid_version_requirement(const std::string& value) {
    if (value == "*") return true;
    std::string_view requirement(value);
    if (!requirement.empty() && (requirement.front() == '^' || requirement.front() == '~')) requirement.remove_prefix(1);
    return valid_semver(requirement);
}

bool parse_package_manifest(const std::string& text, PackageManifest& out, std::string& error) {
    json::Document document;
    json::ParseDiagnostic diagnostic;
    if (!json::Document::parse(text, document, diagnostic)) {
        error = "manifest JSON: " + diagnostic.message + " at " + std::to_string(diagnostic.line) + ":" + std::to_string(diagnostic.column);
        return false;
    }
    if (document.type != json::Type::Object) { error = "manifest root must be a JSON object"; return false; }
    auto string_field = [&](const char* name, bool required, std::string& target) -> bool {
        if (!document.has(name)) { if (required) error = std::string("manifest missing '") + name + "'"; return !required; }
        const auto& value = document[name];
        if (value.type != json::Type::String) { error = std::string("manifest field '") + name + "' must be a string"; return false; }
        target = value.string; return true;
    };
    PackageManifest parsed;
    if (!string_field("name", true, parsed.name) || !string_field("version", true, parsed.version) || !string_field("entry", false, parsed.entry)) return false;
    if (!valid_package_name(parsed.name)) { error = "manifest package name must use lowercase letters, digits, hyphens or underscores"; return false; }
    if (!valid_semver(parsed.version)) { error = "manifest version must use MAJOR.MINOR.PATCH"; return false; }
    string_field("description", false, parsed.description);
    string_field("license", false, parsed.license);
    string_field("repository", false, parsed.repository);
    auto path_array = [&](const char* name, std::vector<std::string>& target) -> bool {
        if (!document.has(name)) return true;
        const auto& value = document[name];
        if (value.type != json::Type::Array) { error = std::string("manifest field ") + name + " must be an array"; return false; }
        for (const auto& item : value.array) {
            if (item.type != json::Type::String || !valid_package_relative_path(item.string)) { error = std::string("invalid package path in ") + name; return false; }
            target.push_back(item.string);
        }
        return true;
    };
    if (!path_array("sources", parsed.sources) || !path_array("include_dirs", parsed.include_dirs)) return false;
    if (!parsed.entry.empty() && !valid_package_relative_path(parsed.entry)) { error = "manifest entry must be a relative package path"; return false; }
    if (document.has("dependencies")) {
        const auto& dependencies = document["dependencies"];
        if (dependencies.type != json::Type::Object) { error = "manifest dependencies must be a JSON object"; return false; }
        for (const auto& item : dependencies.object) {
            if (item.second.type != json::Type::String || !valid_version_requirement(item.second.string)) {
                error = "invalid dependency requirement for '" + item.first + "'";
                return false;
            }
            parsed.dependencies.emplace(item.first, item.second.string);
        }
    }
    out = std::move(parsed);
    return true;
}

std::filesystem::path package_cache_root() {
    if (const char* home = std::getenv("STRUT_HOME"); home && *home) return std::filesystem::path(home) / "cache" / "packages";
#ifdef _WIN32
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) return std::filesystem::path(local) / "Strut" / "Cache" / "packages";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home) return std::filesystem::path(home) / "Library" / "Caches" / "strut" / "packages";
#else
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) return std::filesystem::path(xdg) / "strut" / "packages";
    if (const char* home = std::getenv("HOME"); home && *home) return std::filesystem::path(home) / ".cache" / "strut" / "packages";
#endif
    return std::filesystem::path(".strut-cache") / "packages";
}

} // namespace strut
