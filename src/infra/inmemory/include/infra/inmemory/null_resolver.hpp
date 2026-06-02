// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file null_resolver.hpp
/// Configurable resolver stub for unit tests.
///
/// Returns a fixed IP address for any hostname; `reverse_lookup()` always
/// returns "localhost". Construct with a custom IP to test different paths.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "domain/shared/ports/resolver.hpp"


namespace pvpgn::infra::inmemory {

/// A configurable resolver for tests: returns a fixed address for any hostname.
class NullResolver final : public application::ports::IResolver {
public:
    explicit NullResolver(std::string fixed_ip = "127.0.0.1")
        : fixed_ip_{std::move(fixed_ip)} {}

    core::Status<std::vector<application::ports::ResolvedAddress>>
    resolve(std::string_view /*hostname*/, std::uint16_t port) override {
        return std::vector<application::ports::ResolvedAddress>{{fixed_ip_, port}};
    }

    std::string reverse_lookup(std::string_view /*ip*/) override {
        return "localhost";
    }

private:
    std::string fixed_ip_;
};

}  // namespace pvpgn::infra::inmemory
