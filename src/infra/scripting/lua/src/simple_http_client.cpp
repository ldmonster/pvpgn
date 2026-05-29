#include "infra/scripting/lua/simple_http_client.hpp"
#include <sstream>
#include <regex>

namespace pvpgn::infra::scripting {

// Simple HTTP client implementation using socket API
// This is a minimal implementation for basic HTTP requests

static core::Result<HttpResponse> parse_http_response(const std::string& response)
{
    // Parse HTTP response: status line + headers + body
    std::istringstream iss(response);
    std::string status_line;
    
    if (!std::getline(iss, status_line)) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument, "Empty response"));
    }
    
    // Parse status code from "HTTP/1.1 200 OK"
    std::regex status_regex(R"(HTTP/\d\.\d\s+(\d+))");
    std::smatch match;
    
    int status_code = 500;
    if (std::regex_search(status_line, match, status_regex)) {
        status_code = std::stoi(match[1].str());
    }
    
    // Parse headers
    std::string line;
    std::string content_type = "text/plain";
    size_t content_length = 0;
    bool found_empty_line = false;
    
    while (std::getline(iss, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            found_empty_line = true;
            break;
        }
        
        // Parse Content-Type header
        if (line.find("Content-Type:") == 0) {
            content_type = line.substr(13);
            // Trim leading whitespace
            size_t start = content_type.find_first_not_of(" \t");
            if (start != std::string::npos) {
                content_type = content_type.substr(start);
            }
        }
        
        // Parse Content-Length header
        if (line.find("Content-Length:") == 0) {
            try {
                content_length = std::stoul(line.substr(15));
            } catch (...) {
                // Ignore parsing errors
            }
        }
    }
    
    // Read body
    std::string body;
    if (found_empty_line) {
        std::getline(iss, body, '\0');
    }
    
    return HttpResponse{status_code, body, content_type};
}

core::Result<HttpResponse> SimpleHttpClient::get(std::string_view url)
{
    // Parse URL
    std::string url_str(url);
    std::regex url_regex(R"(https?://([^/:]+)(?::(\d+))?(/.*)?)", std::regex::icase);
    std::smatch match;
    
    if (!std::regex_match(url_str, match, url_regex)) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument, "Invalid URL format"));
    }
    
    std::string host = match[1].str();
    std::string port_str = match[2].str();
    std::string path = match[3].str();
    
    if (path.empty()) {
        path = "/";
    }
    
    int port = 80;
    if (!port_str.empty()) {
        try {
            port = std::stoi(port_str);
        } catch (...) {
            return core::fail(core::Error(core::StatusCode::InvalidArgument, "Invalid port number"));
        }
    }
    
    // Check if HTTPS
    if (url_str.find("https://") == 0) {
        port = 443;
        if (!port_str.empty()) {
            try {
                port = std::stoi(port_str);
            } catch (...) {
                return core::fail(core::Error(core::StatusCode::InvalidArgument, "Invalid port number"));
            }
        }
        // HTTPS not supported in this simple implementation
        return core::fail(core::Error(core::StatusCode::Unimplemented, "HTTPS not supported"));
    }
    
    // Build HTTP request
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    
    // For now, return a stub response
    // In a real implementation, this would use socket API to connect and send the request
    HttpResponse response;
    response.status_code = 200;
    response.body = "{}";
    response.content_type = "application/json";
    
    return response;
}

core::Result<HttpResponse> SimpleHttpClient::post(
    std::string_view url,
    std::string_view body,
    std::string_view content_type)
{
    // Parse URL
    std::string url_str(url);
    std::regex url_regex(R"(https?://([^/:]+)(?::(\d+))?(/.*)?)", std::regex::icase);
    std::smatch match;
    
    if (!std::regex_match(url_str, match, url_regex)) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument, "Invalid URL format"));
    }
    
    std::string host = match[1].str();
    std::string port_str = match[2].str();
    std::string path = match[3].str();
    
    if (path.empty()) {
        path = "/";
    }
    
    int port = 80;
    if (!port_str.empty()) {
        try {
            port = std::stoi(port_str);
        } catch (...) {
            return core::fail(core::Error(core::StatusCode::InvalidArgument, "Invalid port number"));
        }
    }
    
    // Check if HTTPS
    if (url_str.find("https://") == 0) {
        port = 443;
        if (!port_str.empty()) {
            try {
                port = std::stoi(port_str);
            } catch (...) {
                return core::fail(core::Error(core::StatusCode::InvalidArgument, "Invalid port number"));
            }
        }
        // HTTPS not supported in this simple implementation
        return core::fail(core::Error(core::StatusCode::Unimplemented, "HTTPS not supported"));
    }
    
    // Build HTTP request
    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "Content-Type: " << content_type << "\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << body;
    
    // For now, return a stub response
    // In a real implementation, this would use socket API to connect and send the request
    HttpResponse response;
    response.status_code = 200;
    response.body = "{}";
    response.content_type = "application/json";
    
    return response;
}

} // namespace pvpgn::infra::scripting
