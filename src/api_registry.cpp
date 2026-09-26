#include "strut/api_registry.h"

#include <algorithm>
#include <cctype>

namespace strut { namespace {
TypeId t(std::string_view value){return intern_type(value);}
ApiOverload overload(std::initializer_list<std::pair<const char*,const char*>> parameters,const char* result){ApiOverload o;for(const auto& p:parameters)o.parameters.push_back({p.first,t(p.second),false});o.return_type=t(result);return o;}
ApiOverload optional_last(ApiOverload value){if(!value.parameters.empty())value.parameters.back().optional=true;return value;}
ApiCallable call(const char* name,const char* category,const char* module,const char* owner,ApiOverload signature,std::initializer_list<const char*> errors,std::initializer_list<RuntimeComponentId> runtime,const char* summary){ApiCallable c;c.name=name;c.category=category;c.module=module;c.owner=owner;c.overloads.push_back(std::move(signature));c.runtime_components=runtime;c.summary=summary;for(auto e:errors)c.checked_errors.emplace_back(e);return c;}
ApiCallable generic_call(const char* name,const char* module,ApiOverload signature,RuntimeComponentId runtime,const char* summary){auto c=call(name,"function",module,"",std::move(signature),{},{runtime},summary);c.generic_parameters={"T"};return c;}
std::string lower(std::string_view value){std::string out(value);std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return out;}
}

const std::vector<ApiCallable>& api_callables(){
    using R=RuntimeComponentId;
    static const std::vector<ApiCallable> items={
        generic_call("print","core",overload({{"value","T"}},"void"),R::core,"Write a value without a trailing newline."),
        generic_call("println","core",overload({{"value","T"}},"void"),R::core,"Write a value followed by a newline."),
        call("input","function","core","",overload({},"string"),{},{R::io},"Read a line from standard input."),
        generic_call("new","memory",overload({{"value","T"}},"ptr<T>"),R::safe_pointer,"Create a reference-counted safe pointer."),
        generic_call("weak","memory",overload({{"owner","ptr<T>"}},"weak_ptr<T>"),R::weak_pointer,"Create a weak pointer."),
        generic_call("ptr","memory",overload({{"owner","ptr<T>"}},"raw_ptr<T>"),R::raw_pointer,"Create a non-owning raw pointer in an unsafe block."),
        generic_call("ref","memory",overload({{"value","T"}},"ref<T>"),R::safe_pointer,"Borrow an lvalue by reference."),
        call("http_get","function","http","",overload({{"url","string"}},"http_response"),{"HttpError"},{R::http_client},"Perform a buffered HTTP GET request."),
        call("http_get_ca","function","http","",overload({{"url","string"},{"ca_file","string"}},"http_response"),{"HttpError"},{R::http_client},"Perform a verified HTTPS GET using an explicit PEM CA bundle."),
        call("http_request","function","http","",optional_last(overload({{"method","string"},{"url","string"},{"options","json"}},"http_response")),{"HttpError"},{R::http_client},"Perform a configurable buffered HTTP request."),
        call("http_get_json","function","http","",overload({{"url","string"}},"json"),{"HttpError"},{R::http_client},"Fetch and parse a JSON response."),
        call("http_get_async","function","http","",overload({{"url","string"}},"future<http_response>"),{"HttpError"},{R::http_client,R::async},"Start an asynchronous HTTP GET request."),
        call("http_request_async","function","http","",optional_last(overload({{"method","string"},{"url","string"},{"options","json"}},"future<http_response>")),{"HttpError"},{R::http_client,R::async},"Start an asynchronous HTTP request."),
        call("http_server","function","http","",overload({},"http_server"),{},{R::http_server},"Create a plain HTTP server."),
        call("http_text","function","http","",overload({{"body","string"}},"http_server_response"),{},{R::http_server},"Create a text response."),
        call("http_html","function","http","",overload({{"body","string"}},"http_server_response"),{},{R::http_server},"Create an HTML response."),
        call("http_json_response","function","http","",overload({{"body","json"}},"http_server_response"),{},{R::http_server,R::json},"Create a JSON response."),
        call("tls_connect","function","networking","",overload({{"host","string"},{"port","int"}},"tls_stream"),{"TlsError"},{R::http_client},"Open a verified client TLS stream."),
        call("tcp_connect","function","networking","",overload({{"host","string"},{"port","int"}},"tcp_socket"),{"NetworkError"},{R::networking},"Open a TCP connection."),
        call("tcp_connect_async","function","networking","",overload({{"host","string"},{"port","int"}},"future<tcp_socket>"),{"NetworkError"},{R::networking,R::async},"Open a TCP connection asynchronously."),
        call("tcp_listen","function","networking","",overload({{"host","string"},{"port","int"}},"tcp_listener"),{"NetworkError"},{R::networking},"Create a TCP listener."),
        call("sqlite_open","function","sqlite","",overload({{"path","string"}},"sqlite_db"),{"SqliteError"},{R::sqlite},"Open a SQLite database."),
        call("exec","function","process","",optional_last(overload({{"program","string"},{"args","string[]"},{"options","json"}},"exec_result")),{"ExecError"},{R::process},"Execute a program without shell interpolation."),
        call("exec_shell","function","process","",overload({{"command","string"}},"exec_result"),{"ExecError"},{R::process},"Execute an explicit shell command."),
        call("process","function","process","",overload({{"program","string"},{"args","string[]"}},"process"),{"ExecError"},{R::process},"Start a streaming child process."),
        call("pipe_exec","function","process","",overload({{"commands","string[]"}},"exec_result"),{"ExecError"},{R::process},"Execute a pipeline."),
        call("embed_file","function","embedding","",overload({{"path","string"}},"string"),{"EmbedError"},{R::embedded_assets},"Embed a file at compile time."),
        call("embed_dir","function","embedding","",overload({{"path","string"}},"map<string,string>"),{"EmbedError"},{R::embedded_assets},"Embed a directory at compile time."),
        call("env","function","system","",overload({{"name","string"}},"string?"),{},{R::environment},"Read an environment variable."),
        call("set_env","function","system","",overload({{"name","string"},{"value","string"}},"void"),{"EnvironmentError"},{R::environment},"Set an environment variable."),
        call("unset_env","function","system","",overload({{"name","string"}},"void"),{"EnvironmentError"},{R::environment},"Remove an environment variable."),
        call("sleep_ms","function","time","",overload({{"milliseconds","int_64"}},"void"),{"TimeError"},{R::time},"Sleep for a non-negative duration."),
        call("now_ms","function","time","",overload({},"int_64"),{},{R::time},"Read a monotonic millisecond clock."),
        call("unix_ms","function","time","",overload({},"int_64"),{},{R::time},"Read Unix time in milliseconds."),
        call("wait_for_shutdown_signal","function","system","",overload({},"void"),{},{R::time},"Wait safely for SIGINT or SIGTERM outside the raw signal-handler context."),
        call("exists","function","filesystem","",overload({{"path","string"}},"bool"),{"FilesystemError"},{R::filesystem},"Test whether a path exists."),
        call("is_file","function","filesystem","",overload({{"path","string"}},"bool"),{"FilesystemError"},{R::filesystem},"Test whether a path is a file."),
        call("is_dir","function","filesystem","",overload({{"path","string"}},"bool"),{"FilesystemError"},{R::filesystem},"Test whether a path is a directory."),
        call("file_size","function","filesystem","",overload({{"path","string"}},"int_64"),{"FilesystemError"},{R::filesystem},"Read a file size."),
        call("modified","function","filesystem","",overload({{"path","string"}},"int_64"),{"FilesystemError"},{R::filesystem},"Read a path modification time."),
        call("make_dir","function","filesystem","",overload({{"path","string"}},"void"),{"FilesystemError"},{R::filesystem},"Create a directory."),
        call("remove","function","filesystem","",overload({{"paths","string"}},"void"),{"FilesystemError"},{R::filesystem},"Remove one or more paths."),
        call("remove_all","function","filesystem","",overload({{"path","string"}},"void"),{"FilesystemError"},{R::filesystem},"Remove a path recursively."),
        call("copy","function","filesystem","",overload({{"source","string"},{"destination","string"}},"void"),{"FilesystemError"},{R::filesystem},"Copy paths, including ordered pairwise collections."),
        call("move","function","filesystem","",overload({{"source","string"},{"destination","string"}},"void"),{"FilesystemError"},{R::filesystem},"Move paths, including ordered pairwise collections."),
        call("touch","function","filesystem","",overload({{"path","string"}},"void"),{"FilesystemError"},{R::filesystem},"Create a file or update its timestamp."),
        call("read_file","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Read a complete UTF-8 file."),
        call("read_bytes","function","filesystem","",overload({{"path","string"}},"bytes"),{"FilesystemError"},{R::filesystem},"Read a complete binary file."),
        call("write_file","function","filesystem","",overload({{"path","string"},{"data","string"}},"void"),{"FilesystemError"},{R::filesystem},"Write a complete file."),
        call("append_file","function","filesystem","",overload({{"path","string"},{"data","string"}},"void"),{"FilesystemError"},{R::filesystem},"Append to a file."),
        call("ls","function","filesystem","",overload({{"path","string"}},"string[]"),{"FilesystemError"},{R::filesystem},"List a directory."),
        call("walk","function","filesystem","",overload({{"path","string"}},"string[]"),{"FilesystemError"},{R::filesystem},"Walk a directory recursively."),
        call("cwd","function","filesystem","",overload({},"string"),{"FilesystemError"},{R::filesystem},"Read the current working directory."),
        call("cd","function","filesystem","",overload({{"path","string"}},"void"),{"FilesystemError"},{R::filesystem},"Change the current working directory."),
        call("absolute","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Create an absolute path."),
        call("canonical","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Canonicalize a path."),
        call("parent","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Read the parent path."),
        call("filename","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Read the final path component."),
        call("extension","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Read a file extension."),
        call("stem","function","filesystem","",overload({{"path","string"}},"string"),{"FilesystemError"},{R::filesystem},"Read a file stem."),
        call("join_path","function","filesystem","",overload({{"left","string"},{"right","string"}},"string"),{"FilesystemError"},{R::filesystem},"Join path components."),
        call("atomic<T>.load","method","concurrency","atomic<T>",overload({},"T"),{},{R::atomics},"Load the value with sequentially consistent ordering."),
        call("atomic<T>.store","method","concurrency","atomic<T>",overload({{"value","T"}},"void"),{},{R::atomics},"Store a value with sequentially consistent ordering."),
        call("atomic<T>.exchange","method","concurrency","atomic<T>",overload({{"value","T"}},"T"),{},{R::atomics},"Replace and return the previous value."),
        call("atomic<T>.compare_exchange","method","concurrency","atomic<T>",overload({{"expected","T"},{"desired","T"}},"bool"),{},{R::atomics},"Replace the expected value atomically."),
        call("atomic<T>.fetch_add","method","concurrency","atomic<T>",overload({{"value","T"}},"T"),{},{R::atomics},"Add and return the previous integer value."),
        call("atomic<T>.fetch_sub","method","concurrency","atomic<T>",overload({{"value","T"}},"T"),{},{R::atomics},"Subtract and return the previous integer value."),
        call("http_response.json","method","http","http_response",overload({},"json"),{"HttpError"},{R::http_client,R::json},"Parse the response body as JSON."),
        call("http_request.json","method","http","http_request",overload({},"json"),{"HttpError"},{R::http_server,R::json},"Parse the request body as JSON."),
        call("sqlite_db.query","method","sqlite","sqlite_db",overload({{"sql","string"}},"json"),{"SqliteError"},{R::sqlite},"Execute a query and return rows as JSON."),
        call("sqlite_db.exec","method","sqlite","sqlite_db",overload({{"sql","string"}},"void"),{"SqliteError"},{R::sqlite},"Execute a SQL statement."),
        call("http_server.listen","method","http","http_server",overload({{"host","string"},{"port","int"}},"void"),{"NetworkError"},{R::http_server},"Listen for HTTP requests."),
        call("http_server.listen_tls","method","http","http_server",overload({{"host","string"},{"port","int"},{"certificate","string"},{"private_key","string"}},"void"),{"NetworkError","TlsError"},{R::http_server_tls},"Listen for HTTPS requests with a PEM certificate chain and matching private key."),
        call("http_server.stop","method","http","http_server",overload({},"void"),{},{R::http_server},"Stop accepting connections and wait for active requests up to the configured shutdown timeout."),
        call("http_server.running","method","http","http_server",overload({},"bool"),{},{R::http_server},"Report whether the server is accepting or draining requests."),
        call("http_server.timeouts","method","http","http_server",overload({{"read_ms","int"},{"write_ms","int"},{"idle_ms","int"},{"shutdown_ms","int"}},"void"),{"NetworkError"},{R::http_server},"Configure explicit request I/O, idle, and graceful-shutdown timeouts."),
        call("http_server.limits","method","http","http_server",overload({{"body_bytes","int_64"},{"header_bytes","int_64"},{"header_count","int"},{"connections","int"}},"void"),{"NetworkError"},{R::http_server},"Configure request body, header, and active-connection limits."),
        call("http_server.get","method","http","http_server",overload({{"route","string"},{"handler","function"}},"void"),{"NetworkError"},{R::http_server},"Register an HTTP GET route."),
        call("http_server.post","method","http","http_server",overload({{"route","string"},{"handler","function"}},"void"),{"NetworkError"},{R::http_server},"Register an HTTP POST route."),
        call("tls_stream.read","method","networking","tls_stream",overload({},"string"),{"TlsError"},{R::http_client},"Read from a TLS stream."),
        call("tls_stream.write","method","networking","tls_stream",overload({{"data","string"}},"void"),{"TlsError"},{R::http_client},"Write to a TLS stream."),
    };
    return items;
}
const ApiCallable* api_callable(std::string_view name,std::string_view owner){for(const auto& item:api_callables()){const auto leaf=item.owner.empty()?item.name:item.name.substr(item.name.find('.')+1);const bool owner_match=item.owner==owner||(item.owner=="atomic<T>"&&owner.rfind("atomic<",0)==0);if(leaf==name&&owner_match)return &item;}return nullptr;}
const std::vector<std::string>& standard_modules(){static const std::vector<std::string> value={"vector","map","set","ordered_map","ordered_set","queue","stack","deque","list","priority_queue","tuple","filesystem"};return value;}
const std::vector<std::string>& api_named_types(){static const std::vector<std::string> value={"http_request","http_server_response","http_server","SqliteError","sqlite_db","EmbedError","FilesystemError","StreamError","EnvironmentError","TimeError","ExecError","exec_result","process","thread","ThreadError","process_in","process_out","mutex","MutexError","NetworkError","tcp_socket","tcp_listener","TlsError","tls_stream","HttpError","http_response","istream","ostream","sstream","ifstream","ofstream","bytes"};return value;}
std::string api_signature(const ApiCallable& c,const ApiOverload& o){std::string value=c.owner.empty()?c.name:c.name.substr(c.name.find('.')+1);value+="(";for(std::size_t i=0;i<o.parameters.size();++i){if(i)value+=", ";value+=type_spelling(o.parameters[i].type)+" "+o.parameters[i].name;if(o.parameters[i].optional)value+="?";}return value+") -> "+type_spelling(o.return_type);}
bool api_matches(const ApiCallable& c,std::string_view query){if(query.empty())return true;const auto q=lower(query);if(q=="checked-errors")return !c.checked_errors.empty();return lower(c.name).find(q)!=std::string::npos||lower(c.category).find(q)!=std::string::npos||lower(c.module).find(q)!=std::string::npos||lower(c.owner).find(q)!=std::string::npos||lower(c.summary).find(q)!=std::string::npos;}
} // namespace strut
