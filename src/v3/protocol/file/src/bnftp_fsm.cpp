// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/file/bnftp_fsm.hpp"

#include "protocol/bnet/session_context.hpp"

namespace pvpgn::protocol::file {

core::Status<> BnftpFsm::on_bytes(std::span<const std::byte> bytes) {
    if (!ctx_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                          "session context unavailable"));
    }

    // Stub: Parse BNFTP protocol messages
    // Expected: 0xFF 0x01 header + file request
    // For now, no-op
    return core::ok();
}

void BnftpFsm::on_close() {
    // Cleanup if needed
}

core::Status<> BnftpFsm::handle_file_request(std::string_view filename,
                                             std::uint32_t start_offset) {
    // Stub: Open file at files_dir_/filename and stream to client
    (void)filename;
    (void)start_offset;
    return core::ok();
}

core::Status<> BnftpFsm::send_file_chunk(std::string_view filename,
                                         std::uint32_t offset,
                                         std::uint32_t length) {
    // Stub: Read chunk from file and send via ctx_
    (void)filename;
    (void)offset;
    (void)length;
    return core::ok();
}

}  // namespace pvpgn::protocol::file
