#include "strut/package.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <cstdint>
#include <random>
#include <set>
#include <sstream>
#include <string_view>

#include "json.h"

#define write_lockfile write_lockfile_legacy
#define cache_local_package cache_local_package_legacy
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
namespace strut {
namespace {
class Sha256 {
public:
    void add(const void* data,std::size_t size){const auto* p=static_cast<const std::uint8_t*>(data);bits_+=static_cast<std::uint64_t>(size)*8;while(size){auto n=std::min(size,64-used_);std::copy(p,p+n,block_.begin()+static_cast<std::ptrdiff_t>(used_));used_+=n;p+=n;size-=n;if(used_==64){transform();used_=0;}}}
    void add(std::string_view value){add(value.data(),value.size());}
    std::string finish(){const auto original=bits_;std::uint8_t one=0x80;add_padding(&one,1);std::uint8_t zero=0;while(used_!=56)add_padding(&zero,1);std::uint8_t length[8];for(unsigned i=0;i<8;++i)length[7-i]=static_cast<std::uint8_t>(original>>(i*8));add_padding(length,8);std::ostringstream out;out<<"sha256:"<<std::hex<<std::setfill('0');for(auto x:h_)out<<std::setw(8)<<x;return out.str();}
private:
    static std::uint32_t rotr(std::uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
    void add_padding(const void* data,std::size_t size){auto saved=bits_;add(data,size);bits_=saved;}
    void transform(){static constexpr std::uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};std::uint32_t w[64];for(unsigned i=0;i<16;++i)w[i]=(static_cast<std::uint32_t>(block_[i*4])<<24)|(static_cast<std::uint32_t>(block_[i*4+1])<<16)|(static_cast<std::uint32_t>(block_[i*4+2])<<8)|block_[i*4+3];for(unsigned i=16;i<64;++i){auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3),s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}auto a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],hh=h_[7];for(unsigned i=0;i<64;++i){auto s1=rotr(e,6)^rotr(e,11)^rotr(e,25),ch=(e&f)^((~e)&g),t1=hh+s1+ch+k[i]+w[i],s0=rotr(a,2)^rotr(a,13)^rotr(a,22),maj=(a&b)^(a&c)^(b&c),t2=s0+maj;hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=hh;}
    std::array<std::uint32_t,8> h_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};std::array<std::uint8_t,64> block_{};std::size_t used_=0;std::uint64_t bits_=0;
};
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
            if(item.second.type==json::Type::String){if(!valid_version_requirement(item.second.string)){error="invalid dependency requirement for '"+item.first+"'";return false;}parsed.dependencies.emplace(item.first,item.second.string);continue;}
            if(item.second.type!=json::Type::Object||!item.second.has("version")||item.second["version"].type!=json::Type::String||!valid_version_requirement(item.second["version"].string)||!item.second.has("git")||item.second["git"].type!=json::Type::String||!item.second.has("rev")||item.second["rev"].type!=json::Type::String){error="remote dependency '"+item.first+"' requires string fields version, git, and rev";return false;}
            PackageSource source;source.kind="git";source.url=item.second["git"].string;source.revision=item.second["rev"].string;
            const auto valid_revision=source.revision.size()>=40&&source.revision.size()<=64&&std::all_of(source.revision.begin(),source.revision.end(),[](unsigned char c){return std::isxdigit(c);});
            if(source.url.empty()||source.url.front()=='-'||source.url.find_first_of("\r\n")!=std::string::npos||!valid_revision){error="remote dependency '"+item.first+"' must use a safe Git URL and exact hexadecimal commit revision";return false;}
            parsed.dependencies.emplace(item.first,item.second["version"].string);parsed.dependency_sources.emplace(item.first,std::move(source));
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
    json::Document deps=json::Document::make_object(); for(const auto& d:manifest.dependencies){auto source=manifest.dependency_sources.find(d.first);if(source==manifest.dependency_sources.end())deps[d.first]=d.second;else{json::Document remote=json::Document::make_object();remote["version"]=d.second;remote["git"]=source->second.url;remote["rev"]=source->second.revision;deps[d.first]=remote;}} doc["dependencies"]=deps;
    std::ofstream output(path); if(!output){error="unable to write " + path.string();return false;} output<<doc.dump(2)<<'\n'; return static_cast<bool>(output);
}

std::optional<std::filesystem::path> resolve_cached_package(const std::string& name,const std::string& requirement,std::string* error) {
    const auto base=package_cache_root()/name;std::error_code ec;if(!std::filesystem::is_directory(base,ec))return std::nullopt;std::vector<std::pair<std::array<unsigned,3>,std::filesystem::path>> candidates;
    for(const auto&e:std::filesystem::directory_iterator(base,ec)){if(ec||!e.is_directory())continue;const auto version=e.path().filename().string();if(!satisfies(version,requirement))continue;bool ok=false;auto parts=semver_parts(version,ok);if(!ok)continue;if(std::filesystem::exists(e.path()/"strut.json")){candidates.emplace_back(parts,e.path());continue;}for(const auto&content:std::filesystem::directory_iterator(e.path(),ec)){if(ec||!content.is_directory())continue;candidates.emplace_back(parts,content.path());}}
    std::sort(candidates.begin(),candidates.end(),[](const auto&a,const auto&b){if(a.first!=b.first)return a.first>b.first;return a.second.filename().string()<b.second.filename().string();});for(const auto&candidate:candidates){std::string checksum,why;if(!verify_cached_package(candidate.second,checksum,why)){if(error&&error->empty())*error=why;continue;}const auto identity=candidate.second.filename().string();if(candidate.second.parent_path().filename().string()==name)return candidate.second;if(identity==checksum.substr(7))return candidate.second;if(error&&error->empty())*error="corrupted package cache entry "+candidate.second.string()+": directory identity does not match "+checksum;}return std::nullopt;
}

bool cache_local_package(const std::filesystem::path& source_root, std::filesystem::path& cached_root, PackageManifest& manifest, std::string& error) {
    if(!load_package_manifest_file(source_root/"strut.json",manifest,error))return false;std::string checksum;if(!package_content_checksum(source_root,checksum,error))return false;const auto version_root=package_cache_root()/manifest.name/manifest.version;cached_root=version_root/checksum.substr(7);std::error_code ec;std::filesystem::create_directories(version_root,ec);if(ec){error="unable to create package cache: "+ec.message();return false;}if(std::filesystem::exists(cached_root,ec)){std::string actual;if(!verify_cached_package(cached_root,actual,error))return false;if(actual!=checksum){error="checksum mismatch for existing cache entry "+cached_root.string();return false;}return true;}const auto staging_root=package_cache_root()/".staging";std::filesystem::create_directories(staging_root,ec);if(ec){error="unable to create package staging directory: "+ec.message();return false;}std::random_device random;std::filesystem::path stage;for(unsigned attempt=0;attempt<32;++attempt){std::ostringstream id;id<<manifest.name<<'-'<<manifest.version<<'-'<<std::hex<<random()<<random();stage=staging_root/id.str();if(!std::filesystem::exists(stage,ec))break;stage.clear();}if(stage.empty()){error="unable to allocate unique package staging directory";return false;}std::filesystem::copy(source_root,stage,std::filesystem::copy_options::recursive,ec);if(ec){std::filesystem::remove_all(stage,ec);error="unable to stage package: "+ec.message();return false;}std::string staged;if(!package_content_checksum(stage,staged,error)||staged!=checksum){std::filesystem::remove_all(stage,ec);if(error.empty())error="package changed while it was being staged";return false;}std::filesystem::rename(stage,cached_root,ec);if(ec){if(std::filesystem::exists(cached_root)){std::filesystem::remove_all(stage,ec);std::string actual;if(verify_cached_package(cached_root,actual,error)&&actual==checksum)return true;}std::filesystem::remove_all(stage,ec);error="unable to atomically promote package into cache: "+ec.message();return false;}return true;
}

bool package_content_checksum(const std::filesystem::path& root,std::string& checksum,std::string& error){std::error_code ec;if(!std::filesystem::is_directory(root,ec)){error="package root is not a directory: "+root.string();return false;}std::vector<std::filesystem::path> files;for(std::filesystem::recursive_directory_iterator i(root,ec),e;i!=e&&!ec;i.increment(ec)){const auto rel=std::filesystem::relative(i->path(),root,ec);if(ec)break;if(!rel.empty()&&*rel.begin()==".git"){if(i->is_directory())i.disable_recursion_pending();continue;}if(i->is_symlink()){error="package contains unsupported symbolic link: "+rel.generic_string();return false;}if(i->is_regular_file())files.push_back(rel);}if(ec){error="unable to inspect package contents: "+ec.message();return false;}std::sort(files.begin(),files.end());Sha256 hash;for(const auto& rel:files){const auto name=rel.generic_string();hash.add(name);const char separator='\0';hash.add(&separator,1);std::ifstream in(root/rel,std::ios::binary);if(!in){error="unable to read package file: "+rel.generic_string();return false;}std::array<char,8192> buffer{};while(in){in.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=in.gcount();if(count>0)hash.add(buffer.data(),static_cast<std::size_t>(count));}hash.add(&separator,1);}checksum=hash.finish();return true;}

bool verify_cached_package(const std::filesystem::path& cached_root,std::string& checksum,std::string& error){PackageManifest manifest;if(!load_package_manifest_file(cached_root/"strut.json",manifest,error)){error="corrupted package cache entry "+cached_root.string()+": "+error;return false;}if(!package_content_checksum(cached_root,checksum,error)){error="corrupted package cache entry "+cached_root.string()+": "+error;return false;}const auto parent=cached_root.parent_path();const bool content_addressed=parent.parent_path().parent_path()==package_cache_root();if(content_addressed&&cached_root.filename().string()!=checksum.substr(7)){error="corrupted package cache entry "+cached_root.string()+": checksum mismatch (computed "+checksum+")";return false;}return true;}

bool validate_package_lock(const PackageLock& lock,const PackageManifest* manifest,std::string& error){if(lock.schema_version!=2){error="unsupported strut.lock.json schema version "+std::to_string(lock.schema_version)+"; regenerate it with `strut install`";return false;}std::map<std::string,const LockedPackage*> by_name;for(const auto& p:lock.packages){if(!valid_package_name(p.name)||!valid_version_requirement(p.version)||p.source_kind.empty()||p.source.empty()||p.revision.empty()||p.checksum.rfind("sha256:",0)!=0){error="invalid locked package record for '"+p.name+"'";return false;}if(!by_name.emplace(p.name,&p).second){error="duplicate locked package '"+p.name+"'";return false;}}if(manifest){for(const auto& d:manifest->dependencies){auto i=by_name.find(d.first);if(i==by_name.end()){error="lockfile is stale: dependency '"+d.first+"' has no locked entry\nhelp: run `strut install` to regenerate strut.lock.json";return false;}if(!i->second->direct||i->second->requested!=d.second||!satisfies(i->second->version,d.second)){error="lockfile is stale for dependency '"+d.first+"'\nhelp: run `strut install` to regenerate strut.lock.json";return false;}}for(const auto& p:lock.packages)if(p.direct&&!manifest->dependencies.count(p.name)){error="lockfile is stale: removed direct dependency '"+p.name+"' remains locked";return false;}}std::map<std::string,int> state;std::function<bool(const std::string&,std::vector<std::string>&)> visit=[&](const std::string& name,std::vector<std::string>& stack){auto found=by_name.find(name);if(found==by_name.end()){error="locked package graph references missing dependency '"+name+"'";return false;}if(state[name]==2)return true;if(state[name]==1){stack.push_back(name);error="cyclic package dependency: ";for(std::size_t i=0;i<stack.size();++i){if(i)error+=" -> ";error+=stack[i];}return false;}state[name]=1;stack.push_back(name);for(const auto& d:found->second->dependencies){auto child=by_name.find(d.first);if(child==by_name.end()){error="locked package '"+name+"' references missing dependency '"+d.first+"'";return false;}if(child->second->version!=d.second){error="locked dependency edge '"+name+"' -> '"+d.first+"' expects "+d.second+" but record resolves "+child->second->version;return false;}if(!visit(d.first,stack))return false;}stack.pop_back();state[name]=2;return true;};for(const auto& p:lock.packages){std::vector<std::string> stack;if(!visit(p.name,stack))return false;}return true;}

bool parse_package_lock(const std::string& text,PackageLock& out,std::string& error){json::Document d;json::ParseDiagnostic pd;if(!json::Document::parse(text,d,pd)){error="lockfile JSON: "+pd.message+" at "+std::to_string(pd.line)+":"+std::to_string(pd.column);return false;}if(d.type!=json::Type::Object){error="lockfile root must be a JSON object";return false;}if(d.has("version")||d.has("dependencies")){error="legacy strut.lock.json format is not reproducible because it contains machine-specific cache paths\nhelp: remove it and run `strut install` to generate schema version 2";return false;}if(!d.has("schema_version")||d["schema_version"].type!=json::Type::Number||!d.has("packages")||d["packages"].type!=json::Type::Array){error="lockfile must contain numeric schema_version and packages array";return false;}PackageLock lock;lock.schema_version=static_cast<unsigned>(d["schema_version"].num);auto field=[&](const json::Document& item,const char* key,std::string& value)->bool{if(!item.has(key)||item[key].type!=json::Type::String){error=std::string("locked package field '")+key+"' must be a string";return false;}value=item[key].string;return true;};for(const auto& item:d["packages"].array){if(item.type!=json::Type::Object){error="lockfile package entries must be objects";return false;}LockedPackage p;if(!field(item,"name",p.name)||!field(item,"requested",p.requested)||!field(item,"version",p.version)||!field(item,"source_kind",p.source_kind)||!field(item,"source",p.source)||!field(item,"revision",p.revision)||!field(item,"checksum",p.checksum))return false;if(!item.has("direct")||item["direct"].type!=json::Type::Boolean){error="locked package field 'direct' must be a boolean";return false;}p.direct=item["direct"].boolean;if(!item.has("dependencies")||item["dependencies"].type!=json::Type::Object){error="locked package dependencies must be an object";return false;}for(const auto& dep:item["dependencies"].object){if(dep.second.type!=json::Type::String){error="locked dependency versions must be strings";return false;}p.dependencies.emplace(dep.first,dep.second.string);}lock.packages.push_back(std::move(p));}std::sort(lock.packages.begin(),lock.packages.end(),[](const auto&a,const auto&b){return a.name<b.name;});if(!validate_package_lock(lock,nullptr,error))return false;out=std::move(lock);return true;}

bool load_package_lock_file(const std::filesystem::path& path,PackageLock& out,std::string& error){std::ifstream in(path);if(!in){error="unable to open "+path.string();return false;}std::ostringstream text;text<<in.rdbuf();return parse_package_lock(text.str(),out,error);}

bool write_package_lock_file(const std::filesystem::path& path,const PackageLock& input,std::string& error){PackageLock lock=input;std::sort(lock.packages.begin(),lock.packages.end(),[](const auto&a,const auto&b){return a.name<b.name;});if(!validate_package_lock(lock,nullptr,error))return false;json::Document root=json::Document::make_object();root["schema_version"]=static_cast<int>(lock.schema_version);json::Document packages=json::Document::make_array();for(const auto&p:lock.packages){json::Document item=json::Document::make_object();item["name"]=p.name;item["requested"]=p.requested;item["version"]=p.version;item["source_kind"]=p.source_kind;item["source"]=p.source;item["revision"]=p.revision;item["checksum"]=p.checksum;item["direct"]=p.direct;json::Document deps=json::Document::make_object();for(const auto&d:p.dependencies)deps[d.first]=d.second;item["dependencies"]=deps;packages.push_back(item);}root["packages"]=packages;std::ofstream output(path,std::ios::binary|std::ios::trunc);if(!output){error="unable to write "+path.string();return false;}output<<root.dump(2)<<'\n';return static_cast<bool>(output);}

bool write_lockfile(const std::filesystem::path& project_root,const PackageManifest& manifest,std::string& error){PackageLock lock;std::map<std::string,std::string> requested=manifest.dependencies;std::set<std::string> active,complete;std::function<bool(const std::string&,const std::string&,bool)> resolve=[&](const std::string& name,const std::string& requirement,bool direct){if(active.count(name)){error="cyclic package dependency involving '"+name+"'";return false;}if(complete.count(name)){auto i=std::find_if(lock.packages.begin(),lock.packages.end(),[&](const auto&p){return p.name==name;});if(i==lock.packages.end()||!satisfies(i->version,requirement)){error="incompatible requirements for package '"+name+"'";return false;}i->direct=i->direct||direct;return true;}auto root=resolve_cached_package(name,requirement);if(!root){error="dependency '"+name+"' is not present in the package cache\nnote: required version '"+requirement+"'; searched "+(package_cache_root()/name).string()+"\nhelp: run `strut add <local-package-path>` to populate the cache";return false;}PackageManifest package;std::string metadata_error;if(!load_package_manifest_file(*root/"strut.json",package,metadata_error)||package.name!=name||package.version!=root->filename().string()){error="cached package '"+name+"' at "+root->string()+" has stale or invalid metadata"+(metadata_error.empty()?std::string{}:": "+metadata_error)+"\nhelp: remove that cache entry and run `strut add <local-package-path>` again";return false;}LockedPackage p;p.name=name;p.requested=requirement;p.version=package.version;p.source_kind="local-cache";p.source="local:"+name;p.revision=package.version;p.direct=direct;for(const auto&d:package.dependencies){auto child=resolve_cached_package(d.first,d.second);if(!child){error="transitive dependency '"+d.first+"' required by '"+name+"' is not present in the package cache";return false;}p.dependencies[d.first]=child->filename().string();}if(!package_content_checksum(*root,p.checksum,error))return false;lock.packages.push_back(p);active.insert(name);for(const auto&d:package.dependencies)if(!resolve(d.first,d.second,false))return false;active.erase(name);complete.insert(name);return true;};for(const auto&d:manifest.dependencies)if(!resolve(d.first,d.second,true))return false;if(!validate_package_lock(lock,&manifest,error))return false;return write_package_lock_file(project_root/"strut.lock.json",lock,error);}

#undef write_lockfile
#undef cache_local_package
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
namespace {
std::string shell_quote(const std::filesystem::path& value){
#ifdef _WIN32
    std::string out="\"";for(char c:value.string()){if(c=='\"')out+='\\';out+=c;}return out+'\"';
#else
    std::string out="'";for(char c:value.string()){if(c=='\'')out+="'\\''";else out+=c;}return out+'\'';
#endif
}
}

bool cache_local_package(const std::filesystem::path& source_root,std::filesystem::path& cached_root,PackageManifest& manifest,std::string& error) {
    if(!load_package_manifest_file(source_root/"strut.json",manifest,error))return false;
    std::string checksum;if(!package_content_checksum(source_root,checksum,error))return false;
    const auto version_root=package_cache_root()/manifest.name/manifest.version;
    cached_root=version_root/checksum.substr(7);
    std::error_code ec;std::filesystem::create_directories(version_root,ec);
    if(ec){error="unable to create package cache: "+ec.message();return false;}
    if(std::filesystem::exists(cached_root,ec)){
        std::string actual,why;
        if(verify_cached_package(cached_root,actual,why)&&actual==checksum)return true;
        std::filesystem::remove_all(cached_root,ec);
        if(ec){error=why+"; unable to remove corrupted entry: "+ec.message();return false;}
    }
    const auto staging_root=package_cache_root()/".staging";std::filesystem::create_directories(staging_root,ec);
    if(ec){error="unable to create package staging directory: "+ec.message();return false;}
    const auto stale_before=std::filesystem::file_time_type::clock::now()-std::chrono::hours(24);
    for(std::filesystem::directory_iterator i(staging_root,ec),end;i!=end&&!ec;i.increment(ec)){
        std::error_code time_error;const auto modified=std::filesystem::last_write_time(i->path(),time_error);
        if(!time_error&&modified<stale_before){std::error_code cleanup_error;std::filesystem::remove_all(i->path(),cleanup_error);}
    }
    ec.clear();
    std::random_device random;std::filesystem::path stage;
    for(unsigned attempt=0;attempt<32;++attempt){std::ostringstream id;id<<manifest.name<<'-'<<manifest.version<<'-'<<std::hex<<random()<<random();stage=staging_root/id.str();if(!std::filesystem::exists(stage,ec))break;stage.clear();}
    if(stage.empty()){error="unable to allocate unique package staging directory";return false;}
    std::filesystem::copy(source_root,stage,std::filesystem::copy_options::recursive,ec);
    if(ec){const auto why=ec.message();std::filesystem::remove_all(stage,ec);error="unable to stage package: "+why;return false;}
    std::string staged;
    if(!package_content_checksum(stage,staged,error)||staged!=checksum){std::filesystem::remove_all(stage,ec);if(error.empty())error="package changed while it was being staged";return false;}
    std::filesystem::rename(stage,cached_root,ec);
    if(ec){
        const auto promotion_error=ec.message();
        if(std::filesystem::exists(cached_root)){std::string actual,why;if(verify_cached_package(cached_root,actual,why)&&actual==checksum){std::filesystem::remove_all(stage,ec);return true;}}
        std::filesystem::remove_all(stage,ec);error="unable to atomically promote package into cache: "+promotion_error;return false;
    }
    return true;
}

bool acquire_git_package(const PackageSource& source,std::filesystem::path& cached_root,PackageManifest& manifest,std::string& error){
    if(source.kind!="git"||source.url.empty()||source.url.front()=='-'||source.url.find_first_of("\r\n")!=std::string::npos){error="invalid Git package source";return false;}
    if(source.revision.size()<40||source.revision.size()>64||!std::all_of(source.revision.begin(),source.revision.end(),[](unsigned char c){return std::isxdigit(c);})){error="Git package source requires an exact commit revision";return false;}
    const auto acquisition_root=package_cache_root()/".acquire";std::error_code ec;std::filesystem::create_directories(acquisition_root,ec);if(ec){error="unable to create acquisition directory: "+ec.message();return false;}
    std::random_device random;std::filesystem::path checkout;for(unsigned attempt=0;attempt<32;++attempt){std::ostringstream id;id<<"git-"<<std::hex<<random()<<random();checkout=acquisition_root/id.str();if(!std::filesystem::exists(checkout,ec))break;checkout.clear();}if(checkout.empty()){error="unable to allocate Git acquisition directory";return false;}
    const auto hooks=acquisition_root/"disabled-hooks";std::filesystem::create_directories(hooks,ec);
    const auto git_config="git -c core.hooksPath="+shell_quote(hooks);
    const auto clone=git_config+" clone --quiet --no-checkout -- "+shell_quote(source.url)+" "+shell_quote(checkout);
    if(std::system(clone.c_str())!=0){std::filesystem::remove_all(checkout,ec);error="unable to clone Git package source '"+source.url+"'";return false;}
    const auto checkout_command=git_config+" -C "+shell_quote(checkout)+" checkout --quiet --detach "+source.revision;
    if(std::system(checkout_command.c_str())!=0){std::filesystem::remove_all(checkout,ec);error="Git package revision '"+source.revision+"' is unavailable from '"+source.url+"'";return false;}
    const auto revision_file=checkout/".strut-resolved-revision";
    const auto resolve_command=git_config+" -C "+shell_quote(checkout)+" rev-parse HEAD > "+shell_quote(revision_file);
    if(std::system(resolve_command.c_str())!=0){std::filesystem::remove_all(checkout,ec);error="unable to verify acquired Git revision";return false;}
    std::ifstream revision_input(revision_file);std::string resolved;revision_input>>resolved;std::filesystem::remove(revision_file,ec);
    if(resolved!=source.revision){std::filesystem::remove_all(checkout,ec);error="Git revision mismatch: requested "+source.revision+" but acquired "+resolved;return false;}
    std::filesystem::remove_all(checkout/".git",ec);if(ec){std::filesystem::remove_all(checkout,ec);error="unable to remove Git metadata from acquired package: "+ec.message();return false;}
    const bool ok=cache_local_package(checkout,cached_root,manifest,error);std::filesystem::remove_all(checkout,ec);return ok;
}

bool write_lockfile(const std::filesystem::path& project_root,const PackageManifest& manifest,std::string& error) {
    PackageLock lock;std::set<std::string> active,complete;std::map<std::string,PackageSource> sources=manifest.dependency_sources;
    std::function<bool(const std::string&,const std::string&,bool)> resolve;
    resolve=[&](const std::string& name,const std::string& requirement,bool direct) {
        if(active.count(name)){error="cyclic package dependency involving '"+name+"'";return false;}
        auto existing=std::find_if(lock.packages.begin(),lock.packages.end(),[&](const auto&p){return p.name==name;});
        if(complete.count(name)){if(existing==lock.packages.end()||!satisfies(existing->version,requirement)){error="incompatible requirements for package '"+name+"'";return false;}existing->direct=existing->direct||direct;return true;}
        std::string cache_error;auto root=resolve_cached_package(name,requirement,&cache_error);
        if(!root){auto remote=sources.find(name);if(remote!=sources.end()){std::filesystem::path acquired;PackageManifest acquired_manifest;if(!acquire_git_package(remote->second,acquired,acquired_manifest,cache_error)){error=cache_error;return false;}if(acquired_manifest.name!=name||!satisfies(acquired_manifest.version,requirement)){error="acquired Git package metadata does not satisfy dependency '"+name+"' "+requirement;return false;}root=acquired;}}
        if(!root){error=cache_error.empty()?"dependency '"+name+"' is not present in the package cache\nnote: required version '"+requirement+"'; searched "+(package_cache_root()/name).string()+"\nhelp: run `strut add <local-package-path>` to populate the cache":cache_error;return false;}
        PackageManifest package;
        if(!load_package_manifest_file(*root/"strut.json",package,cache_error)||package.name!=name||!satisfies(package.version,requirement)){error="cached package '"+name+"' at "+root->string()+" has stale or invalid metadata"+(cache_error.empty()?std::string{}:": "+cache_error);return false;}
        LockedPackage locked;locked.name=name;locked.requested=requirement;locked.version=package.version;locked.source_kind="local-cache";locked.source="local:"+name;locked.revision=package.version;locked.direct=direct;
        for(const auto& source:package.dependency_sources){auto inserted=sources.emplace(source.first,source.second);if(!inserted.second&&(inserted.first->second.url!=source.second.url||inserted.first->second.revision!=source.second.revision)){error="conflicting immutable sources for package '"+source.first+"'";return false;}}
        for(const auto& dependency:package.dependencies){std::string child_error;auto child=resolve_cached_package(dependency.first,dependency.second,&child_error);if(!child){auto remote=sources.find(dependency.first);if(remote!=sources.end()){std::filesystem::path acquired;PackageManifest acquired_manifest;if(acquire_git_package(remote->second,acquired,acquired_manifest,child_error))child=acquired;}}PackageManifest child_manifest;if(!child||!load_package_manifest_file(*child/"strut.json",child_manifest,child_error)){error="transitive dependency '"+dependency.first+"' required by '"+name+"' is unavailable"+(child_error.empty()?std::string{}:": "+child_error);return false;}locked.dependencies[dependency.first]=child_manifest.version;}
        if(!package_content_checksum(*root,locked.checksum,error))return false;
        lock.packages.push_back(std::move(locked));active.insert(name);
        for(const auto& dependency:package.dependencies)if(!resolve(dependency.first,dependency.second,false))return false;
        active.erase(name);complete.insert(name);return true;
    };
    for(const auto& dependency:manifest.dependencies)if(!resolve(dependency.first,dependency.second,true))return false;
    for(auto& package:lock.packages){auto source=sources.find(package.name);if(source!=sources.end()){package.source_kind="git";package.source=source->second.url;package.revision=source->second.revision;}}
    if(!validate_package_lock(lock,&manifest,error))return false;
    return write_package_lock_file(project_root/"strut.lock.json",lock,error);
}
} // namespace strut
