#pragma once

#include <string>
#include <string_view>
#include "core/result.hpp"

namespace pvpgn::infra::scripting {

/// HTTP response structure
struct HttpResponse {
    int status_code;
    std::string body;
    std::string content_type;
};

/// Simple blocking HTTP client for Lua plugins
class SimpleHttpClient {
public:
    /// Perform a GET request
    static core::Result<HttpResponse> get(std::string_view url);
    
    /// Perform a POST request
    static core::Result<HttpResponse> post(
        std::string_view url,
        std::string_view body,
        std::string_view content_type = "application/json"
    );
    
private:
    SimpleHttpClient() = default;
};

} // namespace pvpgn::infra::scripting
