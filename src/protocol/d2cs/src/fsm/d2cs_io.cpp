// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2cs_io.cpp
/// D2CSSessionFsm — private I/O helper methods.
///
/// Low-level buffer read/write primitives used by both the handler and
/// builder sub-TUs:
///   read_cstring()  — read a null-terminated string from a byte buffer
///   read_u32le()    — read a 4-byte little-endian uint32
///   read_u8()       — read a single byte
///   read_u16le()    — read a 2-byte little-endian uint16
///   push_u32le()    — append a 4-byte little-endian uint32 to a vector
///   push_header()   — append the 3-byte D2CS packet header

#include "protocol/d2cs/fsm.hpp"

namespace pvpgn::protocol::d2cs {

bool D2CSSessionFsm::read_cstring(
    const uint8_t* buf, size_t len, size_t& offset, std::string& out)
{
    const size_t start = offset;
    while (offset < len && buf[offset] != 0x00) {
        ++offset;
    }
    if (offset >= len) {
        // No null terminator found
        return false;
    }
    out.assign(reinterpret_cast<const char*>(buf + start), offset - start);
    ++offset;  // skip null terminator
    return true;
}

bool D2CSSessionFsm::read_u32le(
    const uint8_t* buf, size_t len, size_t& offset, uint32_t& out)
{
    if (offset + 4 > len) {
        return false;
    }
    out = static_cast<uint32_t>(buf[offset])
        | (static_cast<uint32_t>(buf[offset + 1]) << 8)
        | (static_cast<uint32_t>(buf[offset + 2]) << 16)
        | (static_cast<uint32_t>(buf[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool D2CSSessionFsm::read_u8(
    const uint8_t* buf, size_t len, size_t& offset, uint8_t& out)
{
    if (offset + 1 > len) {
        return false;
    }
    out = buf[offset];
    offset += 1;
    return true;
}

bool D2CSSessionFsm::read_u16le(
    const uint8_t* buf, size_t len, size_t& offset, uint16_t& out)
{
    if (offset + 2 > len) {
        return false;
    }
    out = static_cast<uint16_t>(buf[offset])
        | (static_cast<uint16_t>(buf[offset + 1]) << 8);
    offset += 2;
    return true;
}

void D2CSSessionFsm::push_u16le(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void D2CSSessionFsm::push_u32le(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void D2CSSessionFsm::push_u32be(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>(val & 0xFF));
}

void D2CSSessionFsm::push_header(
    std::vector<uint8_t>& v, uint16_t total_len, D2CSPacketType type)
{
    v.push_back(static_cast<uint8_t>(total_len & 0xFF));
    v.push_back(static_cast<uint8_t>((total_len >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>(type));
}

}  // namespace pvpgn::protocol::d2cs
