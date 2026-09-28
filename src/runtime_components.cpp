#include "strut/runtime_components.h"
#include "strut/api_registry.h"

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <unordered_map>

namespace strut { namespace {

using Id = RuntimeComponentId;
const std::vector<RuntimeComponent>& registry() {
    static const std::vector<RuntimeComponent> value = {
        {Id::core,"core",{}, {"cstdint","iostream","stdexcept"},{}},
        {Id::strings,"strings",{Id::core},{"string","charconv","algorithm","cctype"},{}},
        {Id::collections,"collections",{Id::core},{"vector","array","map","unordered_map","set","unordered_set","queue","stack","deque","list","tuple"},{}},
        {Id::bytes,"bytes",{Id::strings,Id::collections},{},{}},
        {Id::io,"io",{Id::strings,Id::bytes},{"fstream","sstream"},{}},
        {Id::cancellation,"cancellation",{Id::core},{"atomic","condition_variable","functional","memory","mutex","thread","unordered_map","vector"},{}},
        {Id::json,"json",{Id::strings,Id::collections},{"json.h"},{}},
        {Id::filesystem,"filesystem",{Id::strings,Id::collections,Id::io},{"filesystem"},{}},
        {Id::environment,"environment",{Id::strings},{"cstdlib"},{}},
        {Id::time,"time",{Id::core},{"chrono","thread"},{}},
        {Id::process,"process",{Id::strings,Id::collections,Id::io,Id::threading,Id::cancellation},{"cerrno","cstring"},{}},
        {Id::safe_pointer,"safe_pointer",{Id::core},{"memory"},{}},
        {Id::weak_pointer,"weak_pointer",{Id::safe_pointer},{},{}},
        {Id::raw_pointer,"raw_pointer",{Id::safe_pointer},{},{}},
        {Id::threading,"threading",{Id::core,Id::safe_pointer},{"thread","functional"},{}},
        {Id::channels,"channels",{Id::threading,Id::collections},{"mutex","condition_variable"},{}},
        {Id::mutex,"mutex",{Id::threading},{"mutex"},{}},
        {Id::atomics,"atomics",{Id::threading},{"atomic"},{}},
        {Id::async,"async",{Id::threading,Id::channels},{"future"},{}},
        {Id::networking,"networking",{Id::strings,Id::safe_pointer},{},{}},
        {Id::http_client,"http_client",{Id::networking,Id::json},{"curl/curl.h"},{"curl"}},
        {Id::http_server,"http_server",{Id::networking,Id::collections,Id::io,Id::threading,Id::mutex,Id::cancellation},{},{}},
        {Id::http_server_tls,"http_server_tls",{Id::http_server},{"openssl/ssl.h","openssl/err.h"},{"ssl","crypto"}},
        {Id::sqlite,"sqlite",{Id::json,Id::safe_pointer},{"sqlite3.h"},{"sqlite3"}},
        {Id::embedded_assets,"embedded_assets",{Id::filesystem,Id::collections},{},{}},
        {Id::ffi,"ffi",{Id::core},{},{}},
        {Id::full_fallback,"full_fallback",{Id::core},{},{}},
    };
    return value;
}
void request_type(std::vector<Id>& out,TypeId type) {
    auto add=[&](Id id){out.push_back(id);};
    if(!type)return;
    const auto& node=type_node(type);
    if(node.kind==TypeNodeKind::safe_pointer)add(Id::safe_pointer);
    if(node.kind==TypeNodeKind::weak_pointer)add(Id::weak_pointer);
    if(node.kind==TypeNodeKind::raw_pointer)add(Id::raw_pointer);
    if(node.kind==TypeNodeKind::vector||node.kind==TypeNodeKind::fixed_array||node.kind==TypeNodeKind::tuple)add(Id::collections);
    if(node.kind==TypeNodeKind::primitive&&node.name=="json")add(Id::json);
    if(node.kind==TypeNodeKind::generic){
        static const std::set<std::string> collections={"map","ordered_map","set","ordered_set","list","deque","queue","stack","priority_queue"};
        if(collections.count(node.name))add(Id::collections);
        if(node.name=="future")add(Id::async);
        if(node.name=="channel")add(Id::channels);
        if(node.name=="atomic")add(Id::atomics);
        if(node.name.rfind("http_",0)==0)add(Id::networking);
    }
    if(node.kind==TypeNodeKind::named){if(node.name=="bytes")add(Id::bytes);if(node.name=="cancellation_source"||node.name=="cancellation_token")add(Id::cancellation);if(node.name=="http_request"||node.name=="http_request_body"||node.name=="http_server_response"||node.name=="http_response_writer"||node.name=="http_server")add(Id::http_server);if(node.name=="http_response")add(Id::http_client);if(node.name=="sqlite_db")add(Id::sqlite);if(node.name=="process")add(Id::process);if(node.name=="thread")add(Id::threading);}
    for(auto child:node.children)request_type(out,child);
}
void request_expr(std::vector<Id>& out,const IRExpr* e);
void request_stmt(std::vector<Id>& out,const IRStmt* s) {
    if(!s)return;
    request_type(out,s->type_id);request_type(out,s->return_type_id);request_type(out,s->alias_target_id);
    if(s->is_async)out.push_back(Id::async);
    if(s->is_extern_c)out.push_back(Id::ffi);
    for(const auto& p:s->parameters)request_type(out,p.type.type_id?p.type.type_id:intern_type(p.type.name));
    for(const auto& f:s->fields)request_type(out,f.type.type_id?f.type.type_id:intern_type(f.type.name));
    request_expr(out,s->value.get());request_expr(out,s->target.get());request_expr(out,s->condition.get());request_expr(out,s->increment.get());request_stmt(out,s->initializer.get());
    for(const auto& c:s->body)request_stmt(out,c.get());
    for(const auto& c:s->else_body)request_stmt(out,c.get());
    for(const auto& c:s->switch_cases){request_expr(out,c.value.get());for(const auto& x:c.body)request_stmt(out,x.get());}
    for(const auto& c:s->catches)for(const auto& x:c.body)request_stmt(out,x.get());
}
void request_expr(std::vector<Id>& out,const IRExpr* e) {
    if(!e)return;
    request_type(out,e->type_id);
    if(e->lambda_async)out.push_back(Id::async);
    if(e->kind==IRExpr::Kind::identifier){
        static const std::unordered_map<std::string,Id> operations={
            {"in",Id::io},{"out",Id::io},{"err",Id::io},{"endl",Id::io},{"istream",Id::io},{"ostream",Id::io},{"sstream",Id::io},{"ifstream",Id::io},{"ofstream",Id::io},
            {"thread",Id::threading},{"mutex",Id::mutex},
        };
        if(auto it=operations.find(e->text);it!=operations.end())out.push_back(it->second);
        if(const auto* callable=api_callable(e->text))out.insert(out.end(),callable->runtime_components.begin(),callable->runtime_components.end());
    }
    if(e->kind==IRExpr::Kind::member&&e->left){if(const auto* callable=api_callable(e->text,e->left->type_name))out.insert(out.end(),callable->runtime_components.begin(),callable->runtime_components.end());}
    request_expr(out,e->left.get());request_expr(out,e->right.get());request_expr(out,e->lambda_expression.get());for(const auto& a:e->arguments)request_expr(out,a.get());for(const auto& s:e->lambda_body)request_stmt(out,s.get());
}
} // namespace

const RuntimeComponent* runtime_component(RuntimeComponentId id){for(const auto& c:registry())if(c.id==id)return &c;return nullptr;}
bool RuntimeResolution::contains(RuntimeComponentId id) const{return std::find(ordered.begin(),ordered.end(),id)!=ordered.end();}
std::vector<std::string> RuntimeResolution::link_libraries() const{std::vector<std::string> out;for(auto id:ordered)if(auto c=runtime_component(id))for(auto lib:c->link_libraries)if(std::find(out.begin(),out.end(),lib)==out.end())out.emplace_back(lib);return out;}
std::string RuntimeResolution::describe() const{std::ostringstream o;for(std::size_t i=0;i<ordered.size();++i){if(i)o<<',';if(auto c=runtime_component(ordered[i]))o<<c->name;}return o.str();}
RuntimeResolution resolve_runtime_component_graph(const std::vector<RuntimeComponent>& graph,const std::vector<RuntimeComponentId>& requested){
    RuntimeResolution r;std::unordered_map<Id,unsigned> state;state.reserve(graph.size());auto find=[&](Id id)->const RuntimeComponent*{for(const auto& c:graph)if(c.id==id)return &c;return nullptr;};
    std::function<bool(Id)> visit=[&](Id id){auto component=find(id);if(!component){r.error="unknown runtime component";return false;}auto& mark=state[id];if(mark==2)return true;if(mark==1){r.error="runtime component dependency cycle";return false;}mark=1;for(auto dep:component->dependencies)if(!visit(dep))return false;mark=2;r.ordered.push_back(id);return true;};
    auto roots=requested;std::sort(roots.begin(),roots.end(),[](Id a,Id b){return static_cast<int>(a)<static_cast<int>(b);});roots.erase(std::unique(roots.begin(),roots.end()),roots.end());for(auto id:roots)if(!visit(id))break;r.fallback=r.contains(Id::full_fallback);return r;
}
RuntimeResolution resolve_runtime_components(const std::vector<RuntimeComponentId>& requested){return resolve_runtime_component_graph(registry(),requested);}
RuntimeResolution analyze_runtime_components(const IRProgram& program){std::vector<Id> requested={Id::core,Id::strings};for(const auto& s:program.statements)request_stmt(requested,s.get());return resolve_runtime_components(requested);}

} // namespace strut
