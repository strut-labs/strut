#include "strut/project.h"
#include <fstream>
#include <sstream>
#include "json.h"
#include "strut/version.h"

namespace strut {
namespace {
bool one_of(const std::string& v, std::initializer_list<const char*> values) {
    for (const char* x : values) if (v == x) return true;
    return false;
}
}

bool parse_build_config(const std::string& text, BuildConfig& out, std::string& error) {
    json::Document doc; json::ParseDiagnostic diagnostic;
    if (!json::Document::parse(text, doc, diagnostic)) {
        error = "build config JSON: " + diagnostic.message + " at " + std::to_string(diagnostic.line) + ":" + std::to_string(diagnostic.column); return false;
    }
    if (doc.type != json::Type::Object) { error = "build config root must be an object"; return false; }
    BuildConfig cfg;
    auto field=[&](const char* name,std::string& dst)->bool{if(!doc.has(name))return true;const auto& v=doc[name];if(v.type!=json::Type::String){error=std::string("build config field '")+name+"' must be a string";return false;}dst=v.string;return true;};
    if(!field("entrypoint",cfg.entrypoint)||!field("output",cfg.output)||!field("target",cfg.target)||!field("mode",cfg.mode)||!field("linking",cfg.linking)||!field("incremental",cfg.incremental))return false;
    if(cfg.entrypoint.empty()||std::filesystem::path(cfg.entrypoint).is_absolute()){error="entrypoint must be a non-empty relative path";return false;}
    if(cfg.output.empty()||std::filesystem::path(cfg.output).is_absolute()){error="output must be a non-empty relative path";return false;}
    if(!one_of(cfg.mode,{"debug","release"})){error="mode must be 'debug' or 'release'";return false;}
    if(!one_of(cfg.linking,{"dynamic","static","mixed"})){error="linking must be 'dynamic', 'static' or 'mixed'";return false;}
    if(!one_of(cfg.incremental,{"modified"})){error="incremental must currently be 'modified'";return false;}
    out=std::move(cfg);return true;
}

bool load_build_config(const std::filesystem::path& path, BuildConfig& out, std::string& error){std::ifstream f(path);if(!f){error="unable to open "+path.string();return false;}std::ostringstream s;s<<f.rdbuf();return parse_build_config(s.str(),out,error);}
bool write_build_config(const std::filesystem::path& path,const BuildConfig& cfg,std::string& error){std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){error=ec.message();return false;}json::Document d=json::Document::make_object();d["entrypoint"]=cfg.entrypoint;d["output"]=cfg.output;d["target"]=cfg.target;d["mode"]=cfg.mode;d["linking"]=cfg.linking;d["incremental"]=cfg.incremental;std::ofstream f(path);if(!f){error="unable to write "+path.string();return false;}f<<d.dump(2)<<'\n';return static_cast<bool>(f);}
bool init_project_build_state(const std::filesystem::path& root,std::string& error){const auto dir=root/".strut";const auto cfg=dir/"config.json";if(std::filesystem::exists(cfg)){error=".strut/config.json already exists";return false;}BuildConfig c;if(std::filesystem::exists(root/"src/main.p"))c.entrypoint="src/main.p";else if(std::filesystem::exists(root/"main.p"))c.entrypoint="main.p";c.output=std::filesystem::path(c.entrypoint).stem().string();return write_build_config(cfg,c,error);}
std::filesystem::path find_project_root(std::filesystem::path start){std::error_code ec;if(start.empty())start=std::filesystem::current_path();start=std::filesystem::absolute(start,ec);if(ec)return {};if(std::filesystem::is_regular_file(start,ec))start=start.parent_path();for(auto p=start;!p.empty();p=p.parent_path()){if(std::filesystem::exists(p/".strut/config.json")||std::filesystem::exists(p/"strut.json"))return p;if(p==p.root_path())break;}return start;}
} // namespace strut


namespace strut {
std::string build_fingerprint(const BuildConfig& c, bool release) {
    return std::string(version) + "|" + c.target + "|" + (release ? "release" : c.mode) + "|" + c.linking + "|c++20";
}
bool write_object_build_info(const std::filesystem::path& path,const ObjectBuildInfo& info,std::string& error){
    json::Document d=json::Document::make_object();d["format"]=1;d["source"]=info.source;d["object"]=info.object;d["compiler-version"]=info.compiler_version;d["target"]=info.target;d["mode"]=info.mode;d["fingerprint"]=info.fingerprint;json::Document deps=json::Document::make_array();for(const auto& x:info.dependencies)deps.array.push_back(json::Document(x));d["dependencies"]=deps;std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){error=ec.message();return false;}std::ofstream f(path);if(!f){error="unable to write "+path.string();return false;}f<<d.dump(2)<<'\n';return static_cast<bool>(f);
}
bool load_object_build_info(const std::filesystem::path& path,ObjectBuildInfo& out,std::string& error){
    std::ifstream f(path);if(!f){error="unable to open "+path.string();return false;}std::ostringstream ss;ss<<f.rdbuf();json::Document d;json::ParseDiagnostic pd;if(!json::Document::parse(ss.str(),d,pd)){error="object metadata JSON: "+pd.message;return false;}if(d.type!=json::Type::Object||!d.has("format")||d["format"].type!=json::Type::Number||d["format"].num!=1||!d.has("dependencies")||d["dependencies"].type!=json::Type::Array){error="object metadata is invalid or from an older format";return false;}ObjectBuildInfo x;auto sf=[&](const char* n,std::string& dst)->bool{if(!d.has(n)||d[n].type!=json::Type::String){error=std::string("object metadata missing '")+n+"'";return false;}dst=d[n].string;return true;};if(!sf("source",x.source)||!sf("object",x.object)||!sf("compiler-version",x.compiler_version)||!sf("target",x.target)||!sf("mode",x.mode)||!sf("fingerprint",x.fingerprint))return false;for(const auto& v:d["dependencies"].array){if(v.type!=json::Type::String){error="object metadata has invalid dependency";return false;}x.dependencies.push_back(v.string);}out=std::move(x);return true;
}
} // namespace strut
