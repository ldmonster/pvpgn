// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_file_bridge.hpp
/// Strangler-fig observation hooks for the two `packet_create()` call sites
/// inside `file_send()` in `src/bnetd/file.cpp`.
///
/// Site 1 — packet_class_file (line ~210):
///   Creates the SERVER_FILE_REPLY header packet that carries the filename,
///   file length, ad-id, extension tag, and modification timestamp.
///   Wire layout (packet_class_file):
///     [file header 4 bytes]
///     filelen   : uint32 (little-endian)
///     adid      : uint32 (little-endian)
///     etag      : uint32 (little-endian)
///     timestamp : bn_long (8 bytes, little-endian)
///     rawname   : NUL-terminated string
///
/// Site 2 — packet_class_raw (line ~274):
///   Creates raw data packets used to stream the file body in MAX_PACKET_SIZE
///   chunks.  Each packet carries up to MAX_PACKET_SIZE bytes of raw file data.
///
/// Both bridges are observation-only (always return 0) because the v3 file
/// transfer encoder is not yet implemented.  The legacy `file_send()` path
/// runs unchanged.
///
/// Returns:
///   0  -> always (observation-only; fall back to legacy).

extern "C" {

/// Observation hook for the SERVER_FILE_REPLY header packet
/// (packet_class_file, site 1 in file_send).
///
/// @param conn_ptr   Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param packet_ptr Opaque legacy `t_packet*` (already populated with header
///                   fields and rawname string). nullptr -> returns 0.
int pvpgn_v3_observe_file_send(void* conn_ptr,
                                void const* packet_ptr) noexcept;

/// Observation hook for the raw file-body chunk packet
/// (packet_class_raw, site 2 in file_send).
///
/// @param conn_ptr   Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param packet_ptr Opaque legacy `t_packet*` (raw data chunk). nullptr -> returns 0.
int pvpgn_v3_observe_file_raw_send(void* conn_ptr,
                                    void const* packet_ptr) noexcept;

}  // extern "C"
