// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file resolver.hpp
/// Application-layer port for hostname/address resolution.
///
/// Abstracts DNS-style lookups so the application layer does not depend on a
/// concrete resolver (a real getaddrinfo-backed one in production, a fixed
/// stub in tests).

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::application::ports {

/// A resolved network endpoint.
struct ResolvedAddress {
    std::string   ip;
    std::uint16_t port;
};

/// Resolves hostnames to addresses and performs reverse lookups.
class IResolver {
public:
    virtual ~IResolver() = default;

    /// Resolve `hostname` to one or more addresses, each carrying `port`.
    virtual core::Status<std::vector<ResolvedAddress>>
    resolve(std::string_view hostname, std::uint16_t port) = 0;

    /// Reverse-resolve an IP to a hostname (best effort).
    virtual std::string reverse_lookup(std::string_view ip) = 0;
};

}  // namespace pvpgn::application::ports
