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
    require(api_callable("http_request_stream")&&api_callable("http_request_stream_async"),"outbound HTTP streaming APIs are registered");
    require(api_callable("http_request_stream")->overloads.size()==2&&type_spelling(api_callable("http_request_stream")->overloads[0].return_type)=="http_response_head","streaming HTTP returns final metadata");
    require(type_spelling(api_callable("http_request_stream")->overloads[0].parameters[3].type)=="function<(int_64)->bytes>?"&&type_spelling(api_callable("http_request_stream")->overloads[0].parameters[4].type)=="function<(bytes)->bool>?","streaming HTTP uses nullable bytes callbacks");
    require(api_callable("http_request_stream")->runtime_components==std::vector<RuntimeComponentId>{RuntimeComponentId::http_client_streaming},"streaming HTTP owns its sliced runtime component");
    require(api_callable("query","sqlite_db")!=nullptr,"method lookup");
    require(api_callable("stop","http_server")!=nullptr,"HTTP stop method lookup");
    require(api_callable("running","http_server")!=nullptr,"HTTP running method lookup");
    require(api_callable("listen_tls","http_server")!=nullptr,"HTTPS listen method lookup");
    require(api_callable("websocket","http_server")!=nullptr,"WebSocket route lookup");
    require(api_callable("websocket","http_server")->overloads[0].parameters[1].type==intern_type("function<(http_request,websocket)->void>"),"WebSocket handler signature");
    require(api_callable("websocket","http_server")->runtime_components==std::vector<RuntimeComponentId>{RuntimeComponentId::http_websocket},"WebSocket route owns its sliced runtime component");
    require(api_callable("accept","websocket")&&api_callable("accept","websocket")->overloads[0].parameters[0].optional,"WebSocket acceptance has an optional subprotocol");
    require(api_callable("accept","websocket")->checked_errors==std::vector<std::string>{"NetworkError"},"WebSocket acceptance uses the network checked error");
    require(api_callable("read","websocket")&&type_spelling(api_callable("read","websocket")->overloads[0].return_type)=="websocket_message?","WebSocket message read is nullable");
    require(api_callable("write_text","websocket")&&api_callable("write_bytes","websocket")&&api_callable("ping","websocket")&&api_callable("close","websocket"),"WebSocket frame methods are registered");
    require(api_callable("read","websocket")->checked_errors==std::vector<std::string>({"WebSocketError","NetworkError"}),"WebSocket protocol and transport errors are distinct");
    require(api_callable("close","websocket")->overloads.size()==3&&api_callable("ping","websocket")->overloads.size()==2,"WebSocket control overloads are registered");
    require(api_callable("websocket_limits","http_server")&&api_callable("websocket_limits","http_server")->checked_errors==std::vector<std::string>{"NetworkError"},"WebSocket limits are registered");
    require(api_field("kind","websocket_message")&&!api_field("kind","websocket_message")->writable&&api_field("text","websocket_message")->type==intern_type("string?")&&api_field("data","websocket_message")->type==intern_type("bytes?"),"WebSocket message tag and payloads are read-only");
    require(std::find(api_named_types().begin(),api_named_types().end(),"WebSocketError")!=api_named_types().end(),"WebSocketError is a named checked error");
    require(api_callable("get_async","http_server")!=nullptr&&api_callable("post_async","http_server")!=nullptr,"async HTTP routes are registered");
    require(type_spelling(api_callable("get_async","http_server")->overloads[0].parameters[1].type)=="function<(http_request)->future<http_server_response>>","async HTTP handler signature");
    require(api_callable("get_stream","http_server")&&api_callable("post_stream","http_server"),"streaming HTTP routes are registered");
    require(api_callable("get_stream","http_server")->overloads[0].parameters[1].type==intern_type("function<(http_request,http_response_writer)->void>"),"streaming HTTP handler signature");
    require(api_callable("write_bytes","http_response_writer")&&api_callable("finish","http_response_writer"),"HTTP response writer contract is registered");
    require(api_callable("write_bytes","http_response_writer")->checked_errors==std::vector<std::string>{"NetworkError"},"HTTP response writer uses the network checked error");
    require(api_callable("post_request_stream","http_server")&&api_callable("read_bytes","http_request_body"),"HTTP request streaming contract is registered");
    require(api_callable("post_request_stream","http_server")->overloads[0].parameters[1].type==intern_type("function<(http_request,http_request_body,http_response_writer)->void>"),"request streaming handler signature");
    require(api_callable("read_all_bytes","http_request_body")->overloads[0].parameters[0].optional,"request body read-all limit is optional");
    require(api_field("cancellation","http_request")&&api_field("cancellation","http_request")->type==intern_type("cancellation_token"),"HTTP request cancellation token is registered");
    require(!api_field("cancellation","http_request")->writable,"HTTP request cancellation token is read-only");
    require(api_field("query_values","http_request")&&api_field("query_values","http_request")->type==intern_type("http_values")&&!api_field("query_values","http_request")->writable,"lossless HTTP query values are registered read-only");
    require(api_field("cookies","http_request")&&api_field("cookies","http_server_response"),"request and response cookie surfaces are registered");
    require(api_callable("values","http_values")&&api_callable("form","http_request")&&api_callable("cookie","http_response_writer"),"HTTP application helper methods are registered");
    require(api_callable("form","http_request")->overloads[0].parameters[0].optional&&api_callable("json","http_request")->overloads[0].parameters[0].optional,"buffered HTTP helper limits are optional");
    require(api_callable("http_redirect")&&api_callable("http_redirect")->overloads[0].parameters.back().optional,"redirect status is optional");
    require(api_callable("http_serve_file")&&api_callable("http_serve_file")->overloads[0].parameters.back().optional,"HTTP file response content type is optional");
    require(api_callable("http_serve_file")->checked_errors==std::vector<std::string>({"FilesystemError","NetworkError"}),"HTTP file response checked errors are registered");
    require(api_callable("http_serve_file")->runtime_components==std::vector<strut::RuntimeComponentId>({strut::RuntimeComponentId::http_file_response}),"HTTP file response owns its runtime capability");
    require(api_callable("http_write_ndjson")&&api_callable("http_write_ndjson")->checked_errors==std::vector<std::string>({"HttpError","NetworkError"}),"NDJSON record helper and checked errors are registered");
    require(api_callable("http_write_ndjson")->runtime_components==std::vector<strut::RuntimeComponentId>({strut::RuntimeComponentId::http_ndjson}),"NDJSON helper owns its runtime capability");
    require(api_callable("static","http_server")!=nullptr,"source-level HTTP static method is registered");
    require(api_callable("receive","channel<int>")!=nullptr&&type_spelling(api_callable("receive","channel<int>")->overloads[0].return_type)=="T?","generic method lookup");
    require(api_callable("accept_async","tcp_listener")!=nullptr&&api_callable("transaction","sqlite_db")!=nullptr,"backend methods are registered");
    require(!api_callable("read","process_out")->overloads[0].parameters[0].optional&&type_spelling(api_callable("read","process_out")->overloads[0].parameters[0].type)=="int_32","process read matches the generated runtime");
    require(api_callable("listen","http_server")->overloads[0].parameters.back().optional,"HTTP max_requests is optional");
    require(api_callable("listen_tls","http_server")->overloads[0].parameters.back().optional,"HTTPS max_requests is optional");
    require(api_field("status","http_response")&&type_spelling(api_field("status","http_response")->type)=="int_32","HTTP fields are registered");
    require(api_field("status","http_response_head")&&api_field("headers","http_response_head"),"streaming HTTP response metadata is registered");
    require(api_field("out","process")&&type_spelling(api_field("out","process")->type)=="process_out","process fields are registered");
    require(api_callable("limits","http_server")->checked_errors==std::vector<std::string>{"NetworkError"},"HTTP limits checked error metadata");
    require(api_callable("slice","bytes")&&type_spelling(api_callable("slice","bytes")->overloads[0].return_type)=="bytes","bytes slice is registered");
    require(api_callable("from_string","bytes")&&api_callable("to_string","bytes"),"explicit bytes string conversions are registered");
    require(api_callable("bytes")&&api_callable("bytes")->overloads.size()==2,"bytes constructors are registered");
    require(api_callable("base64_encode")&&api_callable("base64_decode")&&api_callable("base64url_encode")&&api_callable("base64url_decode"),"Base64 encoding APIs are registered");
    require(api_callable("base64_decode")->checked_errors==std::vector<std::string>{"EncodingError"},"Base64 decoding has a checked error");
    require(api_callable("secure_random_bytes")&&api_callable("sha256")&&api_callable("hmac_sha256")&&api_callable("constant_time_equal"),"crypto APIs are registered");
    require(api_callable("sha256")->checked_errors==std::vector<std::string>{"CryptoError"}&&type_spelling(api_callable("sha256")->overloads[0].return_type)=="bytes","SHA-256 has a binary checked API");
    require(api_callable("constant_time_equal")->checked_errors.empty(),"constant-time comparison cannot fail");
    require(std::find(standard_modules().begin(),standard_modules().end(),"crypto")!=standard_modules().end()&&std::find(standard_modules().begin(),standard_modules().end(),"encoding")!=standard_modules().end(),"crypto and encoding modules are registered");
    require(api_callable("write_file")->overloads.size()==2&&type_spelling(api_callable("write_file")->overloads[1].parameters[1].type)=="bytes","filesystem bytes overload is registered");
    require(api_callable("read_bytes","istream")&&type_spelling(api_callable("read_bytes","istream")->overloads[0].return_type)=="bytes","binary input contract is registered");
    require(api_callable("write_bytes","ostream")&&api_callable("write_bytes","ofstream"),"binary output contract and file adapter are registered");
    require(api_callable("read_all_bytes","process_out")&&api_callable("write_bytes","process_in"),"process binary adapters are registered");
    require(api_callable("process")->overloads.size()==4&&type_spelling(api_callable("process")->overloads[1].parameters[2].type)=="json"&&type_spelling(api_callable("process")->overloads[2].parameters[2].type)=="cancellation_token"&&type_spelling(api_callable("process")->overloads[3].parameters[3].type)=="cancellation_token","process options and cancellation bindings are registered");
    require(api_callable("pty_spawn")&&api_callable("pty_spawn")->overloads.size()==4&&api_callable("pty_spawn")->checked_errors==std::vector<std::string>{"PtyError"},"PTY spawn overloads and named error are registered");
    require(api_callable("read_bytes","pty")&&api_callable("write_bytes","pty")&&api_callable("resize","pty")&&api_callable("interrupt","pty")&&api_callable("terminate","pty")&&api_callable("kill","pty")&&api_callable("hangup","pty")&&api_callable("wait","pty")&&api_callable("close","pty"),"PTY binary I/O, geometry, signal, and lifecycle methods are registered");
    require(api_callable("pty_spawn")->runtime_components==std::vector<RuntimeComponentId>{RuntimeComponentId::pty},"PTY API owns its sliced runtime component");
    require(api_callable("pipe_exec")->overloads[0].parameters.size()==4,"pipeline registry matches its argv-safe runtime signature");
    require(api_callable("read_all_bytes","ifstream")->overloads[0].parameters[0].optional,"binary read-all limit is optional");
    require(api_callable("token","cancellation_source")&&api_callable("cancel","cancellation_source"),"cancellation source contract is registered");
    require(api_callable("cancelled","cancellation_token")&&api_callable("wait","cancellation_token"),"cancellation observer contract is registered");
    require(api_callable("throw_if_cancelled","cancellation_token")->checked_errors==std::vector<std::string>{"CancellationError"},"cancellation checked error metadata");
    require(api_callable("cancel","cancellation_source")->runtime_components==std::vector<RuntimeComponentId>{RuntimeComponentId::cancellation},"cancellation methods request their runtime component");
    require(api_matches(*api_callable("http_get"),"http"),"module filtering");
    require(api_matches(*api_callable("http_get"),"checked-errors"),"checked-error filtering");
    require(api_matches(*api_callable("sha256"),"crypto"),"crypto module filtering");
    require(api_callable("embed_dir")&&api_required_modules(*api_callable("embed_dir")).empty(),"embed_dir has no caller-required module");
    require(api_callable("embed_dir")&&std::find(api_signature_modules(*api_callable("embed_dir")).begin(),api_signature_modules(*api_callable("embed_dir")).end(),"map")!=api_signature_modules(*api_callable("embed_dir")).end(),"embed_dir supplies signature module map");
    require(api_callable("sha256")&&api_required_modules(*api_callable("sha256"))==std::vector<std::string>{"crypto"},"sha256 requires crypto");
    require(api_callable("sha256")&&api_signature_modules(*api_callable("sha256")).empty(),"sha256 supplies no signature module");
    require(api_callable("base64_encode")&&api_required_modules(*api_callable("base64_encode"))==std::vector<std::string>{"encoding"},"base64 requires encoding");
    require(api_callable("exists")&&api_required_modules(*api_callable("exists"))==std::vector<std::string>{"filesystem"},"filesystem requires filesystem");
    require(api_callable("sqlite_open")&&api_required_modules(*api_callable("sqlite_open")).empty(),"sqlite_open is auto-available with no caller-required module");
    require(api_callable("http_get")&&api_required_modules(*api_callable("http_get")).empty(),"http_get is auto-available with no caller-required module");
    require(overloadable_operator("++",OperatorFixity::prefix),"prefix increment metadata");
    require(overloadable_operator("++",OperatorFixity::postfix),"postfix increment metadata");
    require(overloadable_operator("--",OperatorFixity::prefix)&&overloadable_operator("--",OperatorFixity::postfix),"decrement fixity metadata");
    return 0;
}
