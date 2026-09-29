#pragma once

#include <iosfwd>

namespace strut::generated_runtime {

void emit_bytes(std::ostream& out);
void emit_streams(std::ostream& out);
void emit_cancellation(std::ostream& out);
void emit_executor(std::ostream& out);
void emit_tcp(std::ostream& out, bool connect, bool async);
void emit_http_client(std::ostream& out, bool async, bool curl_global = true);
void emit_http_server_types(std::ostream& out, bool json, bool file_responses, bool ndjson = false);
void emit_http_server(std::ostream& out, bool async_handlers, bool tls);

} // namespace strut::generated_runtime
