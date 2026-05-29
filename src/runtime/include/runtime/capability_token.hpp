// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// JWT-like Capability Token System
///
/// Provides HMAC-SHA256 signed capability tokens for inter-service authorization.
/// Tokens contain claims about issuer, subject, capabilities, and expiration.

#include "core/result.hpp"
#include "core/error.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <string_view>

using pvpgn::core::Result;
using pvpgn::core::Error;

namespace pvpgn::runtime {

/// Token claims structure
struct TokenClaims {
    /// Issuing service name
    std::string issuer;
    
    /// Subject (target service)
    std::string subject;
    
    /// List of capabilities (e.g., "character.read", "character.write")
    std::vector<std::string> capabilities;
    
    /// Token issued at time
    std::chrono::system_clock::time_point issued_at;
    
    /// Token expiration time
    std::chrono::system_clock::time_point expires_at;
};

/// JWT-like capability token with HMAC-SHA256 signing
class CapabilityToken {
public:
    /// Sign a token with HMAC-SHA256
    /// @param claims Token claims to sign
    /// @param secret Shared secret for HMAC
    /// @return Signed JWT token string or error
    static Result<std::string, Error> sign(const TokenClaims& claims, std::string_view secret);
    
    /// Verify and decode a token
    /// @param token JWT token string
    /// @param secret Shared secret for HMAC verification
    /// @return Decoded claims or error
    static Result<TokenClaims, Error> verify(std::string_view token, std::string_view secret);
    
    /// Check if token has a specific capability
    /// @param cap Capability name to check
    /// @return True if token has the capability
    bool has_capability(std::string_view cap) const;
    
    /// Get the claims from this token
    const TokenClaims& claims() const { return claims_; }

private:
    TokenClaims claims_;
    
    /// Encode data to base64url format
    static std::string base64url_encode(const std::string& data);
    
    /// Decode data from base64url format
    static Result<std::string, Error> base64url_decode(std::string_view data);
    
    /// Compute HMAC-SHA256
    static Result<std::string, Error> hmac_sha256(std::string_view data, std::string_view secret);
};

} // namespace pvpgn::runtime
