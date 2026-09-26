#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "strut/lsp.h"
#include "temp_directory.h"

namespace {
void request(std::stringstream& in,const std::string& body){in<<"Content-Length: "<<body.size()<<"\r\n\r\n"<<body;}
void require(bool value,const char* message,const std::string& output){if(!value){std::cerr<<"FAIL: "<<message<<'\n'<<output;std::exit(1);}}
std::string quoted(std::string value){std::string out="\"";for(char c:value){if(c=='\\'||c=='\"'){out+='\\';out+=c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else out+=c;}return out+'\"';}
}

int main(){
    TestTempDirectory temp("strut-lsp-project");const auto root=temp.path();
    std::ofstream(root/"strut.json")<<R"({"name":"lsp-test","version":"0.1.0","dependencies":{"local":"1.0.0"}})";
    std::ofstream(root/"helper.p")<<"function helper(string value) -> int { return 1; }\n";
    const auto uri="file://"+(root/"main.p").generic_string();
    const std::string source="function main() -> int : HttpError {\n  http_response response := http_get(\"https://example.test\");\n  response.;\n  result := exec(\"echo\", [\"ok\"]);\n  helper(\"x\");\n  return 0;\n}\n";
    std::stringstream in,out,err;
    request(in,R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    request(in,"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+",\"text\":"+quoted(source)+"}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":2,\"character\":11}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/signatureHelp\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":3,\"character\":31}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":1,\"character\":31}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":4,\"character\":4}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"},\"position\":{\"line\":5,\"character\":2}}}");
    request(in,"{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/documentSymbol\",\"params\":{\"textDocument\":{\"uri\":"+quoted(uri)+"}}}");
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
    require(s.find("additionalTextEdits")!=std::string::npos,"module import edit",s);
    require(s.find("\"code\":")!=std::string::npos,"structured diagnostics",s);
    require(s.find("\"label\": \"print\"")!=std::string::npos,"incomplete source recovery",s);
    require(s.find("\"diagnostics\": []")!=std::string::npos,"didClose clears diagnostics",s);
    return 0;
}
