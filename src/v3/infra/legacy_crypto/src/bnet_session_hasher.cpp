// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/legacy_crypto/bnet_session_hasher.hpp"

#include <cstdint>
#include <cstring>

#include "common/setup_before.h"
#include "common/bnethash.h"
#include "common/setup_after.h"

namespace pvpgn::infra::legacy_crypto {

namespace {

/// Packed transcript laid out exactly as the legacy server feeds
/// it to `bnet_hash`: 4-byte little-endian ticks, then 4-byte
/// little-endian sessionkey, then 20 raw bytes of hash1.
#pragma pack(push, 1)
struct SessionHashInput {
    std::uint32_t ticks;
    std::uint32_t sessionkey;
    std::uint8_t  hash1[20];
};
#pragma pack(pop)

static_assert(sizeof(SessionHashInput) == 4 + 4 + 20,
              "SessionHashInput must be 28 bytes packed");

}  // namespace

domain::BNHash BnetSessionHasher::derive_session_hash(
    const domain::BNHash& password_hash1,
    std::uint32_t ticks,
    std::uint32_t sessionkey) const noexcept {
    SessionHashInput input{};
    input.ticks      = ticks;
    input.sessionkey = sessionkey;
    std::memcpy(input.hash1, password_hash1.bytes().data(),
                sizeof(input.hash1));

    ::pvpgn::t_hash out{};
    ::pvpgn::bnet_hash(&out, sizeof(input), &input);

    // `t_hash` is `std::uint32_t[5]` in *host* byte order. The
    // bnethash transcript uses little-endian packing; persist as
    // little-endian bytes so the BNHash matches the wire format
    // that `bnhash_to_hash` decodes from the packet.
    domain::BNHash::Bytes bytes{};
    for (std::size_t i = 0; i < 5; ++i) {
        std::uint32_t word = out[i];
        bytes[i * 4 + 0] = static_cast<std::uint8_t>(word & 0xFF);
        bytes[i * 4 + 1] = static_cast<std::uint8_t>((word >> 8) & 0xFF);
        bytes[i * 4 + 2] = static_cast<std::uint8_t>((word >> 16) & 0xFF);
        bytes[i * 4 + 3] = static_cast<std::uint8_t>((word >> 24) & 0xFF);
    }
    return domain::BNHash{bytes};
}

}  // namespace pvpgn::infra::legacy_crypto
