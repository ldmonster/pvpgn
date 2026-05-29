// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection.hpp
/// Abstract TCP connection port.
///
/// Implementations live in `infra/net/`.
/// Null/fake implementations for tests live in `infra/inmemory/`.

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "core/result.hpp"

namespace pvpgn::application::ports {

using ReadCallback  = std::function<void(std::span<const std::byte> data)>;
using ErrorCallback = std::function<void(std::string_view reason)>;

class IConnection {
public:
    virtual ~IConnection() = default;

    /// Send raw bytes; returns error if connection is closed.
    virtual core::Status<void> write(std::span<const std::byte> data) = 0;

    /// Register a callback for incoming data.
    virtual void on_read(ReadCallback cb) = 0;

    /// Register a callback for connection errors / disconnects.
    virtual void on_error(ErrorCallback cb) = 0;

    /// Close the connection.
    virtual void close() noexcept = 0;

    /// Returns true if the connection is open.
    virtual bool is_open() const noexcept = 0;

    /// Remote endpoint info.
    virtual std::string    remote_ip()   const          = 0;
    virtual std::uint16_t  remote_port() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
