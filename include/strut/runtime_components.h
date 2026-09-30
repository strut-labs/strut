#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "strut/ir.h"

namespace strut {

enum class RuntimeComponentId {
    core,
    strings,
    collections,
    bytes,
    encoding,
    openssl_crypto,
    crypto,
    io,
    cancellation,
    json,
    filesystem,
    environment,
    time,
    process,
    pty,
    safe_pointer,
    weak_pointer,
    raw_pointer,
    threading,
    channels,
    mutex,
    atomics,
    async,
    networking,
    http_client,
    http_client_streaming,
    http_server,
    http_websocket,
    http_file_response,
    http_ndjson,
    http_server_tls,
    sqlite,
    embedded_assets,
    ffi,
    full_fallback,
};

struct RuntimeComponent {
    RuntimeComponentId id;
    std::string_view name;
    std::vector<RuntimeComponentId> dependencies;
    std::vector<std::string_view> headers;
    std::vector<std::string_view> link_libraries;
};

struct RuntimeResolution {
    std::vector<RuntimeComponentId> ordered;
    std::string error;
    bool fallback = false;
    bool ok() const { return error.empty(); }
    bool contains(RuntimeComponentId id) const;
    std::vector<std::string> link_libraries() const;
    std::string describe() const;
};

const RuntimeComponent* runtime_component(RuntimeComponentId id);
RuntimeResolution resolve_runtime_components(const std::vector<RuntimeComponentId>& requested);
RuntimeResolution resolve_runtime_component_graph(const std::vector<RuntimeComponent>& graph, const std::vector<RuntimeComponentId>& requested);
RuntimeResolution analyze_runtime_components(const IRProgram& program);

} // namespace strut
