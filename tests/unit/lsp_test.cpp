#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "strut/lsp.h"
#include "strut/package.h"
#include "temp_directory.h"

namespace {
void request(std::stringstream& in,const std::string& body){in<<"Content-Length: "<<body.size()<<"\r\n\r\n"<<body;}
void require(bool value,const char* message,const std::string& output){if(!value){std::cerr<<"FAIL: "<<message<<'\n'<<output;std::exit(1);}}
std::string quoted(std::string value){std::string out="\"";for(char c:value){if(c=='\\'||c=='\"'){out+='\\';out+=c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else out+=c;}return out+'\"';}
}

int main(){
    TestTempDirectory temp("strut-lsp-project");const auto root=temp.path();
    const auto home=root/"package-home";
#ifdef _WIN32
    _putenv_s("STRUT_HOME",home.string().c_str());
#else
    setenv("STRUT_HOME",home.string().c_str(),1);
#endif
    const auto package_source=root/"package-source";std::filesystem::create_directories(package_source);std::ofstream(package_source/"strut.json")<<R"({"name":"local","version":"1.0.0","entry":"main.p"})";std::ofstream(package_source/"main.p")<<"function package_symbol(string value) -> int { return 2; }\n";std::filesystem::path cached;strut::PackageManifest package;std::string package_error;require(strut::cache_local_package(package_source,cached,package,package_error),"cache LSP package",package_error);
    strut::PackageManifest project;project.name="lsp-test";project.version="0.1.0";project.dependencies["local"]="1.0.0";require(strut::write_package_manifest_file(root/"strut.json",project,package_error),"write LSP manifest",package_error);require(strut::write_lockfile(root,project,package_error),"write LSP lock",package_error);
    std::ofstream(root/"helper.p")<<"function helper(string value) -> int { return 1; }\n";
    const auto uri="file://"+(root/"main.p").generic_string();
    const std::string source="function main() -> int : HttpError {\n  http_response response := http_get(\"https://example.test\");\n  response.;\n  result := exec(\"echo\", [\"ok\"]);\n  helper(\"x\");\n  package_symbol(\"x\");\n  return 0;\n}\n";
    std::stringstream in,out,err;
    request(in,R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    request(in,"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+",\"text\":"+quoted(source)+"}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":2,\"character\":11}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/signatureHelp\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":3,\"character\":31}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":1,\"character\":31}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":4,\"character\":4}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":5,\"character\":2}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/documentSymbol\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":5,\"character\":5}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":5,\"character\":5}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"textDocument/signatureHelp\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":5,\"character\":20}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"contentChanges\":[{\"text\":\"function main( {\\n  pri\\n\"}]}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":1,\"character\":5}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"}}}");
    request(in,R"({"jsonrpc":"2.0","id":9,"method":"shutdown","params":{}})");request(in,R"({"jsonrpc":"2.0","method":"exit","params":{}})");
    require(strut::run_lsp(in,out,err)==0,"server exit",out.str()+err.str());const auto s=out.str();
    require(s.find("signatureHelpProvider")!=std::string::npos,"signature capability",s);
    require(s.find("documentSymbolProvider")!=std::string::npos,"document symbol capability",s);
    require(s.find("json() -> json")!=std::string::npos,"TypeId method completion",s);
    require(s.find("exec(string program, string[] args, json options?) -> exec_result")!=std::string::npos,"signature help",s);
    require(s.find("\"activeParameter\": 1")!=std::string::npos,"active argument",s);
    require(s.find("Checked errors: HttpError")!=std::string::npos,"checked error hover",s);
    require(s.find("helper.p")!=std::string::npos,"project definition",s);
    require(s.find(cached.generic_string()+"/main.p")!=std::string::npos,"package definition",s);
    require(s.find("\"label\": \"package_symbol\"")!=std::string::npos,"package completion",s);
    require(s.find("function package_symbol(string value) -> int")!=std::string::npos,"package hover and signature",s);
    require(s.find("additionalTextEdits")!=std::string::npos,"module import edit",s);
    require(s.find("\"code\":")!=std::string::npos,"structured diagnostics",s);
    require(s.find("\"label\": \"print\"")!=std::string::npos,"incomplete source recovery",s);
    require(s.find("\"diagnostics\": []")!=std::string::npos,"didClose clears diagnostics",s);
    std::filesystem::remove_all(cached);std::stringstream missing_in,missing_out,missing_err;request(missing_in,"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+",\"text\":"+quoted(source)+"}}}");request(missing_in,R"({"jsonrpc":"2.0","id":11,"method":"shutdown","params":{}})");request(missing_in,R"({"jsonrpc":"2.0","method":"exit","params":{}})");require(strut::run_lsp(missing_in,missing_out,missing_err)==0,"missing package server exit",missing_out.str()+missing_err.str());require(missing_out.str().find("package `local` is locked but not present in the verified cache")!=std::string::npos,"missing package diagnostic",missing_out.str());require(missing_out.str().find("run `strut install`")!=std::string::npos,"missing package help",missing_out.str());
    const std::string server_source="function main() -> void : NetworkError {\n  app := http_server();\n  app.;\n  return;\n}\n";std::stringstream server_in,server_out,server_err;request(server_in,"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+",\"text\":"+quoted(server_source)+"}}}");request(server_in,"{\"jsonrpc\":\"2.0\",\"id\":14,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":2,\"character\":6}}}");request(server_in,R"({"jsonrpc":"2.0","id":15,"method":"shutdown","params":{}})");request(server_in,R"({"jsonrpc":"2.0","method":"exit","params":{}})");require(strut::run_lsp(server_in,server_out,server_err)==0,"HTTP lifecycle LSP exit",server_out.str()+server_err.str());require(server_out.str().find("\"label\": \"stop\"")!=std::string::npos&&server_out.str().find("\"label\": \"running\"")!=std::string::npos&&server_out.str().find("\"label\": \"listen_tls\"")!=std::string::npos,"HTTP lifecycle completion",server_out.str());
    return 0;
}
