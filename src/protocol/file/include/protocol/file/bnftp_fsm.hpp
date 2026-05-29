// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnftp_fsm.hpp
/// Battle.net file transfer protocol (BNFTP) handler.
///
/// Implements the BNFTP v1 request/response protocol:
///   1. Client sends CLIENT_FILE_REQ (0x0100) with filename + start_offset
///   2. Server replies with SERVER_FILE_REPLY (0x0000) header
///   3. Server streams raw file bytes (filelen - start_offset bytes)
///   4. Connection closes after transfer
///
/// The FSM accumulates bytes in an internal buffer until a complete
/// CLIENT_FILE_REQ packet is available, then serves the file.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "protocol/file/file_session_context.hpp"

namespace pvpgn::protocol::file {

/// BNFTP file transfer protocol FSM.
///
/// States:
///   AwaitingRequest  — waiting for a complete CLIENT_FILE_REQ packet
///   Serving          — reply header sent; streaming file data
///   Done             — transfer complete or error; connection closing
class BnftpFsm {
public:
    enum class State : std::uint8_t {
        AwaitingRequest,
        Serving,
        Done,
    };

    BnftpFsm(std::shared_ptr<IFileSessionContext> ctx,
             std::string_view files_dir)
        : ctx_(std::move(ctx)), files_dir_(files_dir) {}

    /// Feed raw bytes from the TCP stream into the FSM.
    /// Returns ok() on success; error causes the session to close.
    core::Status<> on_bytes(std::span<const std::byte> bytes);

    /// Called when the connection is closing (peer close or error).
    void on_close();

    /// Current FSM state (for testing).
    State state() const noexcept { return state_; }

private:
    /// Try to parse a complete CLIENT_FILE_REQ from buf_ and serve it.
    core::Status<> try_dispatch();

    /// Open the file at files_dir_/filename, send the reply header,
    /// then stream all bytes from start_offset onward.
    core::Status<> handle_file_request(std::string_view filename,
                                       std::uint32_t    ad_id,
                                       std::uint32_t    extension_tag,
                                       std::uint32_t    start_offset);

    /// Send the SERVER_FILE_REPLY header packet.
    core::Status<> send_reply_header(std::uint32_t file_len,
                                     std::uint32_t ad_id,
                                     std::uint32_t extension_tag,
                                     std::uint64_t timestamp,
                                     std::string_view filename);

    /// Stream raw file bytes from offset to EOF via ctx_->send_bytes().
    core::Status<> stream_file(const std::string& path,
                               std::uint32_t      start_offset);

    std::shared_ptr<IFileSessionContext> ctx_;
    std::string                          files_dir_;
    std::vector<std::byte>               buf_;
    State                                state_{State::AwaitingRequest};
};

}  // namespace pvpgn::protocol::file
