#include "strut/package.h"

#include <array>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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

namespace {
std::array<unsigned,3> semver_parts(const std::string& value, bool& ok) {
    std::array<unsigned,3> out{0,0,0}; std::size_t off=0; ok=true;
    for (int i=0;i<3;++i) { const char* first=value.data()+off; const char* last=value.data()+value.size(); auto r=std::from_chars(first,last,out[static_cast<std::size_t>(i)]); if(r.ec!=std::errc{}||r.ptr==first){ok=false;return out;} off=static_cast<std::size_t>(r.ptr-value.data()); if(i<2){if(off>=value.size()||value[off]!='.'){ok=false;return out;}++off;} }
    ok=off==value.size(); return out;
}
bool satisfies(const std::string& version, const std::string& requirement) {
    bool vok=false, rok=false; auto v=semver_parts(version,vok); std::string base=requirement; char mode=0; if(!base.empty()&&(base[0]=='^'||base[0]=='~')){mode=base[0];base.erase(base.begin());} if(requirement=="*") return true; auto r=semver_parts(base,rok); if(!vok||!rok)return false; if(!mode)return v==r; if(v<r)return false; if(mode=='^') return v[0]==r[0]; return v[0]==r[0]&&v[1]==r[1];
}
}

bool load_package_manifest_file(const std::filesystem::path& path, PackageManifest& out, std::string& error) {
    std::ifstream input(path); if(!input){error="unable to open " + path.string(); return false;} std::ostringstream text; text<<input.rdbuf(); return parse_package_manifest(text.str(),out,error);
}

bool write_package_manifest_file(const std::filesystem::path& path, const PackageManifest& manifest, std::string& error) {
    json::Document doc=json::Document::make_object(); doc["name"]=manifest.name; doc["version"]=manifest.version; if(!manifest.entry.empty())doc["entry"]=manifest.entry; if(!manifest.description.empty())doc["description"]=manifest.description; if(!manifest.license.empty())doc["license"]=manifest.license; if(!manifest.repository.empty())doc["repository"]=manifest.repository;
    json::Document deps=json::Document::make_object(); for(const auto& d:manifest.dependencies)deps[d.first]=d.second; doc["dependencies"]=deps;
    std::ofstream output(path); if(!output){error="unable to write " + path.string();return false;} output<<doc.dump(2)<<'\n'; return static_cast<bool>(output);
}

std::optional<std::filesystem::path> resolve_cached_package(const std::string& name, const std::string& requirement) {
    const auto base=package_cache_root()/name; std::error_code ec; if(!std::filesystem::is_directory(base,ec))return std::nullopt; std::optional<std::pair<std::array<unsigned,3>,std::filesystem::path>> best;
    for(const auto& e:std::filesystem::directory_iterator(base,ec)){if(ec||!e.is_directory())continue; const auto v=e.path().filename().string(); if(!satisfies(v,requirement))continue; bool ok=false;auto parts=semver_parts(v,ok); if(ok&&(!best||parts>best->first))best=std::make_pair(parts,e.path());} if(!best)return std::nullopt; return best->second;
}

bool cache_local_package(const std::filesystem::path& source_root, std::filesystem::path& cached_root, PackageManifest& manifest, std::string& error) {
    if (!load_package_manifest_file(source_root / "strut.json", manifest, error)) return false;
    cached_root = package_cache_root() / manifest.name / manifest.version;
    std::error_code ec;
    if (std::filesystem::exists(cached_root, ec)) return true;
    std::filesystem::create_directories(cached_root.parent_path(), ec);
    if (ec) { error = ec.message(); return false; }
    std::filesystem::copy(source_root, cached_root,
                          std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks, ec);
    if (ec) { error = ec.message(); return false; }
    return true;
}

bool write_lockfile(const std::filesystem::path& project_root, const PackageManifest& manifest, std::string& error) {
    json::Document root=json::Document::make_object(); root["version"]=1; json::Document deps=json::Document::make_object();
    for(const auto& d:manifest.dependencies){auto resolved=resolve_cached_package(d.first,d.second); if(!resolved){error="dependency '"+d.first+"' is not present in the package cache";return false;} json::Document item=json::Document::make_object(); item["version"]=resolved->filename().string(); item["source"]="cache"; deps[d.first]=item;} root["dependencies"]=deps; std::ofstream out(project_root/"strut.lock.json"); if(!out){error="unable to write strut.lock.json";return false;} out<<root.dump(2)<<'\n'; return static_cast<bool>(out);
}

} // namespace strut
