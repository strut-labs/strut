#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "strut/runtime_components.h"
#include "strut/type.h"

namespace strut {

struct ApiParameter { std::string name; TypeId type{}; bool optional = false; };
struct ApiOverload { std::vector<ApiParameter> parameters; TypeId return_type{}; };
struct ApiCallable {
    std::string name;
    std::string category;
    std::string module;
    std::string owner;
    std::vector<ApiOverload> overloads;
    std::vector<std::string> checked_errors;
    std::vector<RuntimeComponentId> runtime_components;
    std::string summary;
    std::vector<std::string> generic_parameters;
    std::vector<std::string> platforms;
    bool deprecated = false;
    std::string reference_url;
};

struct ApiField {
    std::string name;
    std::string owner;
    TypeId type{};
    std::string summary;
};

const std::vector<ApiCallable>& api_callables();
const ApiCallable* api_callable(std::string_view name, std::string_view owner = {});
const std::vector<ApiField>& api_fields();
const ApiField* api_field(std::string_view name, std::string_view owner);
bool api_owner_matches(std::string_view schema_owner, std::string_view owner);
const std::vector<std::string>& standard_modules();
const std::vector<std::string>& api_named_types();
std::string api_signature(const ApiCallable& callable, const ApiOverload& overload);
bool api_matches(const ApiCallable& callable, std::string_view query);

} // namespace strut
