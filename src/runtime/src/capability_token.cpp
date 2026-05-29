// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime/capability_token.hpp"

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace pvpgn::runtime {

// Base64url encoding/decoding
static constexpr const char* BASE64_CHARS = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string CapabilityToken::base64url_encode(const std::string& data)
{
    std::string result;
    int val = 0;
    int valb = 0;
    
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 6) {
            valb -= 6;
            result.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
        }
    }
    
    if (valb > 0) {
        result.push_back(BASE64_CHARS[(val << (6 - valb)) & 0x3F]);
    }
    
    return result;
}

Result<std::string, Error> CapabilityToken::base64url_decode(std::string_view data)
{
    std::string result;
    std::vector<int> T(256, -1);
    
    for (int i = 0; i < 64; i++) {
        T[static_cast<unsigned char>(BASE64_CHARS[i])] = i;
    }
    
    int val = 0;
    int valb = 0;
    
    for (unsigned char c : data) {
        if (T[c] == -1) {
            break;
        }
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 8) {
            valb -= 8;
            result.push_back(char((val >> valb) & 0xFF));
        }
    }
    
    return Result<std::string, Error>(result);
}

Result<std::string, Error> CapabilityToken::hmac_sha256(std::string_view data, std::string_view secret)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    HMAC(EVP_sha256(),
         secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()),
         hash, &hash_len);
    
    std::string result(reinterpret_cast<char*>(hash), hash_len);
    return Result<std::string, Error>(result);
}

// Simple JSON-like string builder for JWT payload
static std::string build_json_payload(const TokenClaims& claims)
{
    std::ostringstream oss;
    oss << "{\"iss\":\"" << claims.issuer << "\","
        << "\"sub\":\"" << claims.subject << "\","
        << "\"iat\":" << claims.issued_at.time_since_epoch().count() << ","
        << "\"exp\":" << claims.expires_at.time_since_epoch().count() << ","
        << "\"capabilities\":[";
    
    for (size_t i = 0; i < claims.capabilities.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << claims.capabilities[i] << "\"";
    }
    
    oss << "]}";
    return oss.str();
}

// Simple JSON parser for JWT payload
static Result<TokenClaims, Error> parse_json_payload(std::string_view json_str)
{
    TokenClaims claims;
    
    // Extract issuer
    size_t iss_pos = json_str.find("\"iss\":\"");
    if (iss_pos == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Missing 'iss' field in JWT payload"))
        );
    }
    size_t iss_start = iss_pos + 8;
    size_t iss_end = json_str.find("\"", iss_start);
    if (iss_end == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Invalid 'iss' field in JWT payload"))
        );
    }
    claims.issuer = std::string(json_str.substr(iss_start, iss_end - iss_start));
    
    // Extract subject
    size_t sub_pos = json_str.find("\"sub\":\"");
    if (sub_pos == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Missing 'sub' field in JWT payload"))
        );
    }
    size_t sub_start = sub_pos + 8;
    size_t sub_end = json_str.find("\"", sub_start);
    if (sub_end == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Invalid 'sub' field in JWT payload"))
        );
    }
    claims.subject = std::string(json_str.substr(sub_start, sub_end - sub_start));
    
    // Extract iat (issued at)
    size_t iat_pos = json_str.find("\"iat\":");
    if (iat_pos == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Missing 'iat' field in JWT payload"))
        );
    }
    size_t iat_start = iat_pos + 6;
    size_t iat_end = json_str.find(",", iat_start);
    if (iat_end == std::string::npos) {
        iat_end = json_str.find("}", iat_start);
    }
    if (iat_end == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Invalid 'iat' field in JWT payload"))
        );
    }
    try {
        long iat_val = std::stol(std::string(json_str.substr(iat_start, iat_end - iat_start)));
        claims.issued_at = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(iat_val)
        );
    } catch (...) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Failed to parse 'iat' field in JWT payload"))
        );
    }
    
    // Extract exp (expiration)
    size_t exp_pos = json_str.find("\"exp\":");
    if (exp_pos == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Missing 'exp' field in JWT payload"))
        );
    }
    size_t exp_start = exp_pos + 6;
    size_t exp_end = json_str.find(",", exp_start);
    if (exp_end == std::string::npos) {
        exp_end = json_str.find("}", exp_start);
    }
    if (exp_end == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Invalid 'exp' field in JWT payload"))
        );
    }
    try {
        long exp_val = std::stol(std::string(json_str.substr(exp_start, exp_end - exp_start)));
        claims.expires_at = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(exp_val)
        );
    } catch (...) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Failed to parse 'exp' field in JWT payload"))
        );
    }
    
    // Extract capabilities array
    size_t caps_pos = json_str.find("\"capabilities\":[");
    if (caps_pos != std::string::npos) {
        size_t caps_start = caps_pos + 16;
        size_t caps_end = json_str.find("]", caps_start);
        if (caps_end != std::string::npos) {
            std::string caps_str = std::string(json_str.substr(caps_start, caps_end - caps_start));
            
            // Parse individual capabilities
            size_t pos = 0;
            while (pos < caps_str.length()) {
                size_t quote_start = caps_str.find("\"", pos);
                if (quote_start == std::string::npos) break;
                
                size_t quote_end = caps_str.find("\"", quote_start + 1);
                if (quote_end == std::string::npos) break;
                
                std::string cap = caps_str.substr(quote_start + 1, quote_end - quote_start - 1);
                claims.capabilities.push_back(cap);
                
                pos = quote_end + 1;
            }
        }
    }
    
    return Result<TokenClaims, Error>(claims);
}

Result<std::string, Error> CapabilityToken::sign(const TokenClaims& claims, std::string_view secret)
{
    // Create header
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string header_b64 = base64url_encode(header);
    
    // Create payload
    std::string payload_str = build_json_payload(claims);
    std::string payload_b64 = base64url_encode(payload_str);
    
    // Create signature
    std::string message = header_b64 + "." + payload_b64;
    auto sig_result = hmac_sha256(message, secret);
    
    if (!sig_result) {
        return Result<std::string, Error>(core::fail(sig_result.error()));
    }
    
    std::string signature_b64 = base64url_encode(sig_result.value());
    
    // Combine all parts
    std::string token = message + "." + signature_b64;
    return Result<std::string, Error>(token);
}

Result<TokenClaims, Error> CapabilityToken::verify(std::string_view token, std::string_view secret)
{
    // Split token into parts
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    
    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument, "Invalid JWT format: missing dots"))
        );
    }
    
    std::string_view header_b64 = token.substr(0, first_dot);
    std::string_view payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string_view signature_b64 = token.substr(second_dot + 1);
    
    // Verify signature
    std::string message(token.substr(0, second_dot));
    auto sig_result = hmac_sha256(message, secret);
    
    if (!sig_result) {
        return Result<TokenClaims, Error>(core::fail(sig_result.error()));
    }
    
    std::string expected_sig_b64 = base64url_encode(sig_result.value());
    
    if (expected_sig_b64 != signature_b64) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::Unauthenticated, "JWT signature verification failed"))
        );
    }
    
    // Decode payload
    auto payload_decode = base64url_decode(payload_b64);
    if (!payload_decode) {
        return Result<TokenClaims, Error>(core::fail(payload_decode.error()));
    }
    
    // Parse payload
    auto claims_result = parse_json_payload(payload_decode.value());
    if (!claims_result) {
        return Result<TokenClaims, Error>(core::fail(claims_result.error()));
    }
    
    TokenClaims claims = claims_result.value();
    
    // Check expiration
    auto now = std::chrono::system_clock::now();
    if (now > claims.expires_at) {
        return Result<TokenClaims, Error>(
            core::fail(core::make_error(core::StatusCode::Unauthenticated, "JWT token has expired"))
        );
    }
    
    return Result<TokenClaims, Error>(claims);
}

bool CapabilityToken::has_capability(std::string_view cap) const
{
    for (const auto& c : claims_.capabilities) {
        if (c == cap) {
            return true;
        }
    }
    return false;
}

} // namespace pvpgn::runtime
