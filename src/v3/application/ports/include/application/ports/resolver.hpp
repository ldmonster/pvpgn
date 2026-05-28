// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file resolver.hpp
/// Abstract DNS resolver port.
///
/// Implementations live in `infra/net/`.
/// Null/fake implementations for tests live in `infra/inmemory/`.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::application::ports {

struct ResolvedAddress {
    std::string   ip;    ///< dotted-decimal IPv4 or IPv6
    std::uint16_t port;
};

class IResolver {
public:
    virtual ~IResolver() = default;

    /// Synchronously resolve hostname:port → list of addresses.
    virtual core::Status<std::vector<ResolvedAddress>>
    resolve(std::string_view hostname, std::uint16_t port) = 0;

    /// Reverse-resolve an IP address to a hostname (best-effort).
    virtual std::string reverse_lookup(std::string_view ip) = 0;
};

}  // namespace pvpgn::application::ports
