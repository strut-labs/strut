#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>

#include "strut/api_registry.h"
#include "strut/operator.h"

namespace { void require(bool value,const char* message){if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}} }

int main(){
    using namespace strut;
    std::set<std::string> keys;
    require(!api_callables().empty(),"registry is populated");
    for(const auto& callable:api_callables()){
        require(keys.insert(callable.owner+"::"+callable.name).second,"callable identity is unique");
        require(!callable.overloads.empty(),"callable has an overload");
        for(const auto& overload:callable.overloads){require(static_cast<bool>(overload.return_type),"return TypeId is valid");require(!type_spelling(overload.return_type).empty(),"return type is printable");for(const auto& parameter:overload.parameters)require(static_cast<bool>(parameter.type),"parameter TypeId is valid");}
        for(auto component:callable.runtime_components)require(runtime_component(component)!=nullptr,"runtime component resolves");
        for(const auto& error:callable.checked_errors)require(std::find(api_named_types().begin(),api_named_types().end(),error)!=api_named_types().end(),"checked error is a known type");
        const auto leaf=callable.owner.empty()?callable.name:callable.name.substr(callable.name.find('.')+1);
        require(api_callable(leaf,callable.owner)==&callable,"registry lookup is stable");
    }
    require(api_callable("http_get")!=nullptr,"free function lookup");
    require(api_callable("query","sqlite_db")!=nullptr,"method lookup");
    require(api_callable("stop","http_server")!=nullptr,"HTTP stop method lookup");
    require(api_callable("running","http_server")!=nullptr,"HTTP running method lookup");
    require(api_callable("listen_tls","http_server")!=nullptr,"HTTPS listen method lookup");
    require(api_callable("get_async","http_server")!=nullptr&&api_callable("post_async","http_server")!=nullptr,"async HTTP routes are registered");
    require(type_spelling(api_callable("get_async","http_server")->overloads[0].parameters[1].type)=="function<(http_request)->future<http_server_response>>","async HTTP handler signature");
    require(api_callable("static","http_server")!=nullptr,"source-level HTTP static method is registered");
    require(api_callable("receive","channel<int>")!=nullptr&&type_spelling(api_callable("receive","channel<int>")->overloads[0].return_type)=="T?","generic method lookup");
    require(api_callable("accept_async","tcp_listener")!=nullptr&&api_callable("transaction","sqlite_db")!=nullptr,"backend methods are registered");
    require(!api_callable("read","process_out")->overloads[0].parameters[0].optional&&type_spelling(api_callable("read","process_out")->overloads[0].parameters[0].type)=="int_32","process read matches the generated runtime");
    require(api_callable("listen","http_server")->overloads[0].parameters.back().optional,"HTTP max_requests is optional");
    require(api_callable("listen_tls","http_server")->overloads[0].parameters.back().optional,"HTTPS max_requests is optional");
    require(api_field("status","http_response")&&type_spelling(api_field("status","http_response")->type)=="int_32","HTTP fields are registered");
    require(api_field("out","process")&&type_spelling(api_field("out","process")->type)=="process_out","process fields are registered");
    require(api_callable("limits","http_server")->checked_errors==std::vector<std::string>{"NetworkError"},"HTTP limits checked error metadata");
    require(api_callable("slice","bytes")&&type_spelling(api_callable("slice","bytes")->overloads[0].return_type)=="bytes","bytes slice is registered");
    require(api_callable("from_string","bytes")&&api_callable("to_string","bytes"),"explicit bytes string conversions are registered");
    require(api_callable("bytes")&&api_callable("bytes")->overloads.size()==2,"bytes constructors are registered");
    require(api_callable("write_file")->overloads.size()==2&&type_spelling(api_callable("write_file")->overloads[1].parameters[1].type)=="bytes","filesystem bytes overload is registered");
    require(api_callable("read_bytes","istream")&&type_spelling(api_callable("read_bytes","istream")->overloads[0].return_type)=="bytes","binary input contract is registered");
    require(api_callable("write_bytes","ostream")&&api_callable("write_bytes","ofstream"),"binary output contract and file adapter are registered");
    require(api_callable("read_all_bytes","process_out")&&api_callable("write_bytes","process_in"),"process binary adapters are registered");
    require(api_callable("process")->overloads.size()==2&&type_spelling(api_callable("process")->overloads[1].parameters[2].type)=="cancellation_token","process cancellation binding is registered");
    require(api_callable("read_all_bytes","ifstream")->overloads[0].parameters[0].optional,"binary read-all limit is optional");
    require(api_callable("token","cancellation_source")&&api_callable("cancel","cancellation_source"),"cancellation source contract is registered");
    require(api_callable("cancelled","cancellation_token")&&api_callable("wait","cancellation_token"),"cancellation observer contract is registered");
    require(api_callable("throw_if_cancelled","cancellation_token")->checked_errors==std::vector<std::string>{"CancellationError"},"cancellation checked error metadata");
    require(api_callable("cancel","cancellation_source")->runtime_components==std::vector<RuntimeComponentId>{RuntimeComponentId::cancellation},"cancellation methods request their runtime component");
    require(api_matches(*api_callable("http_get"),"http"),"module filtering");
    require(api_matches(*api_callable("http_get"),"checked-errors"),"checked-error filtering");
    require(overloadable_operator("++",OperatorFixity::prefix),"prefix increment metadata");
    require(overloadable_operator("++",OperatorFixity::postfix),"postfix increment metadata");
    require(overloadable_operator("--",OperatorFixity::prefix)&&overloadable_operator("--",OperatorFixity::postfix),"decrement fixity metadata");
    return 0;
}
