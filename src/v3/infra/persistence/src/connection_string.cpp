// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/connection_string.hpp"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace pvpgn::infra::persistence {

namespace {

core::Error invalid(std::string msg) {
    return core::make_error(core::StatusCode::InvalidArgument, std::move(msg));
}

}  // namespace

core::Result<ParsedConnectionString, core::Error>
parse_connection_string(std::string_view cs) {
    if (cs.empty()) {
        return core::fail(invalid("connection string is empty"));
    }

    const auto slash = cs.rfind('/');
    if (slash == std::string_view::npos) {
        return core::fail(invalid("connection string missing '/database'"));
    }
    const std::string_view host_and_port = cs.substr(0, slash);
    const std::string_view database      = cs.substr(slash + 1);

    if (host_and_port.empty()) {
        return core::fail(invalid("connection string has empty host"));
    }
    if (database.empty()) {
        return core::fail(invalid("connection string has empty database"));
    }

    ParsedConnectionString out;
    out.port = 0;

    const auto colon = host_and_port.rfind(':');
    if (colon != std::string_view::npos && colon + 1 < host_and_port.size()) {
        const std::string_view port_text = host_and_port.substr(colon + 1);
        std::uint32_t          port_val  = 0;
        const auto*            first     = port_text.data();
        const auto*            last      = first + port_text.size();
        const auto             result    = std::from_chars(first, last, port_val);
        if (result.ec == std::errc{} && result.ptr == last) {
            if (port_val == 0 || port_val > 65535) {
                return core::fail(invalid(
                    "connection string port out of range (1..65535)"));
            }
            out.port = static_cast<std::uint16_t>(port_val);
            out.host.assign(host_and_port.substr(0, colon));
        } else {
            return core::fail(invalid(
                "connection string has non-numeric port"));
        }
    } else if (colon != std::string_view::npos) {
        return core::fail(invalid("connection string ends in ':'"));
    } else {
        out.host.assign(host_and_port);
    }

    if (out.host.empty()) {
        return core::fail(invalid("connection string has empty host"));
    }

    out.database.assign(database);
    return out;
}

}  // namespace pvpgn::infra::persistence
