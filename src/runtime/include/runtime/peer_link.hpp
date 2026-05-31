// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// Inter-Service Communication (PeerLink)
/// 
/// Enables secure communication between services (d2cs <-> d2dbs, etc.)
/// using TLS and JWT capability tokens for authorization.
///
/// Architecture:
/// - Each service exposes a PeerLink server on a private port
/// - Services authenticate using JWT tokens with specific capabilities
/// - Communication is encrypted with TLS
/// - Supports request/response and streaming patterns
///
/// Example usage:
///
///   // In d2cs:
///   PeerLinkServer server("d2cs", config.peer_link_port);
///   server.register_handler("character.lock", [](const Request& req) {
///       return handle_character_lock(req);
///   });
///   server.start();
///
///   // In d2dbs:
///   PeerLinkClient client("d2dbs", "d2cs", config.d2cs_peer_link_addr);
///   auto result = client.call("character.lock", request);

#include "core/result.hpp"
#include "core/error.hpp"
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>

using pvpgn::core::Result;
using pvpgn::core::Error;

namespace pvpgn::runtime {

// Forward declaration
class PeerLinkServerImpl;

/// Request/response message for inter-service communication
struct PeerMessage {
    /// Message type (e.g., "character.lock", "character.unlock")
    std::string type;
    
    /// Request/response payload (JSON)
    std::string payload;
    
    /// Request ID for correlation
    std::string request_id;
    
    /// Timestamp (ISO 8601)
    std::string timestamp;
    
    /// Capability token (JWT)
    std::string token;
};

/// Capability token for inter-service authorization
struct CapabilityToken {
    /// Issuing service name
    std::string issuer;
    
    /// Subject (target service)
    std::string subject;
    
    /// List of capabilities (e.g., "character.read", "character.write")
    std::vector<std::string> capabilities;
    
    /// Token expiration time (Unix timestamp)
    long expiration = 0;
    
    /// Encode token to JWT format
    std::string encode() const;
    
    /// Decode token from JWT format
    static Result<CapabilityToken, std::string> decode(const std::string& jwt);
};

/// PeerLink server for receiving inter-service requests
class PeerLinkServer {
public:
    using RequestHandler = std::function<Result<PeerMessage, std::string>(const PeerMessage&)>;
    
    /// Create a PeerLink server
    /// @param service_name Name of this service
    /// @param port Port to listen on
    /// @param tls_cert Path to TLS certificate
    /// @param tls_key Path to TLS private key
    PeerLinkServer(const std::string& service_name, 
                   int port,
                   const std::string& tls_cert = "",
                   const std::string& tls_key = "");
    
    ~PeerLinkServer();
    
    /// Register a request handler
    void register_handler(const std::string& message_type, RequestHandler handler);
    
    /// Start the server
    Result<void, std::string> start();
    
    /// Stop the server
    void stop();
    
    /// Get server status
    std::string status() const;
    
private:
    std::string service_name_;
    int port_;
    std::string tls_cert_;
    std::string tls_key_;
    std::map<std::string, RequestHandler> handlers_;
    bool running_ = false;
    std::unique_ptr<PeerLinkServerImpl> impl_;
};

/// PeerLink client for making inter-service requests
class PeerLinkClient {
public:
    /// Create a PeerLink client
    /// @param service_name Name of this service (for authentication)
    /// @param target_service Name of target service
    /// @param target_address Address of target service (host:port)
    /// @param token Capability token for authentication
    PeerLinkClient(const std::string& service_name,
                   const std::string& target_service,
                   const std::string& target_address,
                   const CapabilityToken& token);
    
    ~PeerLinkClient();
    
    /// Make a synchronous request to the target service
    Result<PeerMessage, std::string> call(const std::string& message_type,
                                          const std::string& payload);
    
    /// Make an asynchronous request
    Result<std::string, Error> call_async(const std::string& message_type,
                                          const std::string& payload);
    
    /// Get response for async request
    Result<PeerMessage, std::string> get_response(const std::string& request_id);
    
private:
    std::string service_name_;
    std::string target_service_;
    std::string target_address_;
    CapabilityToken token_;
    bool connected_ = false;
};

} // namespace pvpgn::runtime
