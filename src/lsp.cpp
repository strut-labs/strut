#include "strut/lsp.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "json.h"
#include "strut/formatter.h"
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/sema.h"
#include "strut/source.h"

namespace strut { namespace {
struct Doc { std::string uri; std::string text; };
struct Def { std::string name; std::string detail; SourceSpan span; };
std::unordered_map<std::string, Doc> docs;

json::Document pos(std::size_t line,std::size_t col){json::Document d=json::Document::make_object();d["line"]=static_cast<int>(line?line-1:0);d["character"]=static_cast<int>(col?col-1:0);return d;}
json::Document range(const SourceSpan&s){json::Document d=json::Document::make_object();d["start"]=pos(s.begin.line,s.begin.column);d["end"]=pos(s.end.line,s.end.column);return d;}
void send(std::ostream& out,const json::Document& d){const auto s=d.dump();out<<"Content-Length: "<<s.size()<<"\r\n\r\n"<<s;out.flush();}
json::Document response(const json::Document&id,const json::Document& result){json::Document d=json::Document::make_object();d["jsonrpc"]="2.0";d["id"]=id;d["result"]=result;return d;}
json::Document null_doc(){return json::Document{};}
std::string get_string(const json::Document& d,const std::string& k){return d.is_object()&&d.has(k)?d[k].as_string():"";}
void collect_stmt(const Stmt& s,std::vector<Def>&defs){
    auto add=[&](std::string detail){if(!s.name.empty())defs.push_back({s.name,std::move(detail),s.span});};
    switch(s.kind){
      case Stmt::Kind::declaration:add(s.declared_type?s.declared_type->name:"inferred");break;
      case Stmt::Kind::function_decl:add("function");break;
      case Stmt::Kind::struct_decl:add("struct");break;
      case Stmt::Kind::enum_decl:add("enum");break;
      case Stmt::Kind::type_alias:add("type");break;
      default:break;
    }
    for(const auto& p:s.parameters)defs.push_back({p.name,p.type.name,p.span});
    for(const auto& f:s.fields)defs.push_back({f.name,f.type.name,f.span});
    for(const auto& b:s.body)collect_stmt(*b,defs);
    for(const auto& b:s.else_body)collect_stmt(*b,defs);
}
struct Analysis{std::vector<Diagnostic> diagnostics,warnings;std::vector<Def> defs;};
Analysis analyze(const std::string& uri,const std::string& text){
    Analysis a; SourceFile sf(uri,text); Lexer lx(sf);auto l=lx.lex();a.diagnostics=l.diagnostics;if(!l.ok())return a;
    Parser p(l.tokens);auto pr=p.parse();a.diagnostics.insert(a.diagnostics.end(),pr.diagnostics.begin(),pr.diagnostics.end());if(!pr.ok())return a;
    for(const auto&s:pr.program.statements)collect_stmt(*s,a.defs);
    SemanticAnalyzer sema;auto sr=sema.analyze(pr.program);a.diagnostics.insert(a.diagnostics.end(),sr.diagnostics.begin(),sr.diagnostics.end());a.warnings=sr.warnings;return a;
}
json::Document lsp_diags(const Analysis&a){json::Document arr=json::Document::make_array();auto emit=[&](const Diagnostic&x,int severity){json::Document d=json::Document::make_object();d["range"]=range(x.span);d["severity"]=severity;d["source"]="strut";d["message"]=x.message;arr.push_back(d);};for(const auto&d:a.diagnostics)emit(d,1);for(const auto&w:a.warnings)emit(w,2);return arr;}
void publish(std::ostream&out,const std::string&uri,const std::string&text){auto a=analyze(uri,text);json::Document p=json::Document::make_object();p["uri"]=uri;p["diagnostics"]=lsp_diags(a);json::Document n=json::Document::make_object();n["jsonrpc"]="2.0";n["method"]="textDocument/publishDiagnostics";n["params"]=p;send(out,n);}
std::string word_at(const std::string&t,std::size_t line0,std::size_t col0){std::size_t line=0,start=0;for(std::size_t i=0;i<t.size()&&line<line0;++i)if(t[i]=='\n'){++line;start=i+1;}std::size_t p=std::min(start+col0,t.size());auto ok=[](char c){return std::isalnum(static_cast<unsigned char>(c))||c=='_';};std::size_t a=p,b=p;while(a>start&&ok(t[a-1]))--a;while(b<t.size()&&ok(t[b]))++b;return t.substr(a,b-a);}
json::Document location(const std::string&uri,const SourceSpan&s){json::Document d=json::Document::make_object();d["uri"]=uri;d["range"]=range(s);return d;}
std::string read_message(std::istream&in){std::string line;std::size_t len=0;while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())break;if(line.rfind("Content-Length:",0)==0)len=static_cast<std::size_t>(std::stoul(line.substr(15)));}if(!len)return{};std::string body(len,'\0');in.read(body.data(),static_cast<std::streamsize>(len));return body;}
}
int run_lsp(std::istream&in,std::ostream&out,std::ostream&err){(void)err;bool running=true;while(running&&in){auto body=read_message(in);if(body.empty())break;json::Document m;std::string e;if(!json::Document::parse(body,m,e))continue;const std::string method=get_string(m,"method");const bool has_id=m.has("id");json::Document id=has_id?m["id"]:null_doc();
 if(method=="initialize"){json::Document caps=json::Document::make_object();caps["textDocumentSync"]=1;caps["definitionProvider"]=true;caps["hoverProvider"]=true;caps["completionProvider"]=json::Document::make_object();caps["documentFormattingProvider"]=true;json::Document r=json::Document::make_object();r["capabilities"]=caps;send(out,response(id,r));}
 else if(method=="shutdown"){send(out,response(id,null_doc()));}
 else if(method=="exit"){running=false;}
 else if(method=="textDocument/didOpen"||method=="textDocument/didChange"){
   const auto&p=m["params"];const auto&td=p["textDocument"];std::string uri=get_string(td,"uri"),text;
   if(method=="textDocument/didOpen")text=get_string(td,"text");else if(p.has("contentChanges")&&!p["contentChanges"].array.empty())text=get_string(p["contentChanges"][0],"text");docs[uri]={uri,text};publish(out,uri,text);
 }
 else if(method=="textDocument/definition"||method=="textDocument/hover"||method=="textDocument/completion"||method=="textDocument/formatting"){
   const auto&p=m["params"];const std::string uri=get_string(p["textDocument"],"uri");auto it=docs.find(uri);if(it==docs.end()){send(out,response(id,null_doc()));continue;}auto a=analyze(uri,it->second.text);
   if(method=="textDocument/completion"){json::Document arr=json::Document::make_array();const char* kws[]={"function","struct","enum","type","const","if","else","while","for","switch","match","try","catch","throw","return","async","await","unsafe","include"};for(auto*k:kws){json::Document x=json::Document::make_object();x["label"]=k;arr.push_back(x);}for(const auto&d:a.defs){json::Document x=json::Document::make_object();x["label"]=d.name;x["detail"]=d.detail;arr.push_back(x);}send(out,response(id,arr));}
   else if(method=="textDocument/formatting"){json::Document arr=json::Document::make_array();json::Document edit=json::Document::make_object();SourceSpan all;all.begin={0,1,1};std::size_t lines=1,col=1;for(char c:it->second.text){if(c=='\n'){++lines;col=1;}else ++col;}all.end={it->second.text.size(),lines,col};edit["range"]=range(all);edit["newText"]=format_source_text(it->second.text);arr.push_back(edit);send(out,response(id,arr));}
   else {std::size_t line=static_cast<std::size_t>(p["position"]["line"].as_int());std::size_t col=static_cast<std::size_t>(p["position"]["character"].as_int());std::string w=word_at(it->second.text,line,col);auto d=std::find_if(a.defs.begin(),a.defs.end(),[&](const Def&x){return x.name==w;});if(d==a.defs.end()){send(out,response(id,null_doc()));continue;}if(method=="textDocument/definition")send(out,response(id,location(uri,d->span)));else{json::Document h=json::Document::make_object();h["contents"]=d->detail+" "+d->name;send(out,response(id,h));}}
 }
 else if(has_id)send(out,response(id,null_doc()));
 }return 0;}
} // namespace strut
