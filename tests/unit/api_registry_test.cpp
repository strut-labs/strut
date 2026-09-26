#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>

#include "strut/api_registry.h"

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
    require(api_matches(*api_callable("http_get"),"http"),"module filtering");
    require(api_matches(*api_callable("http_get"),"checked-errors"),"checked-error filtering");
    return 0;
}
