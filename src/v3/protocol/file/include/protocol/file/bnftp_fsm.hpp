// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnftp_fsm.hpp
/// Battle.net file transfer protocol (BNFTP) handler.
/// Implements the file request/response protocol.

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstddef>

#include "core/result.hpp"

// Forward declarations
namespace pvpgn::protocol::bnet {
class ISessionContext;
}

namespace pvpgn::protocol::file {

/// BNFTP file transfer protocol handler.
class BnftpFsm {
public:
    BnftpFsm(std::shared_ptr<pvpgn::protocol::bnet::ISessionContext> ctx,
             std::string_view files_dir)
        : ctx_(ctx), files_dir_(files_dir) {}

    /// Called when raw bytes are received on the socket.
    core::Status<> on_bytes(std::span<const std::byte> bytes);

    /// Called when connection is closing.
    void on_close();

private:
    /// Parse a BNFTP file request and send response.
    core::Status<> handle_file_request(std::string_view filename,
                                       std::uint32_t start_offset);

    /// Send a file chunk to the client.
    core::Status<> send_file_chunk(std::string_view filename,
                                   std::uint32_t offset,
                                   std::uint32_t length);

    std::shared_ptr<pvpgn::protocol::bnet::ISessionContext> ctx_;
    std::string files_dir_;
};

}  // namespace pvpgn::protocol::file
