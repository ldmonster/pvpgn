// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/crypto/bnet_session_hasher.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "infra/crypto/bnet_hash.hpp"
#include "infra/crypto/bnet_hash_conv.hpp"

namespace pvpgn::infra::crypto {

// The broken-SHA-1 primitives live in the pvpgn::v3::infra::crypto namespace.
namespace hash = pvpgn::v3::infra::crypto;

domain::BNHash
BnetSessionHasher::derive_session_hash(const domain::BNHash& password_hash1,
                                       std::uint32_t         ticks,
                                       std::uint32_t         sessionkey) const noexcept {
    // Original layout (handle_bnet.cpp _client_loginreq2): a 28-byte buffer of
    //   bn_int ticks ‖ bn_int sessionkey ‖ bn_int passhash1[5]
    // all little-endian. The stored BNHash bytes already ARE the 5 LE words of
    // passhash1, so they are copied verbatim after the two LE tokens.
    std::array<std::byte, 28> buf{};
    auto put_le32 = [&buf](std::size_t off, std::uint32_t v) {
        buf[off + 0] = static_cast<std::byte>(v & 0xFFu);
        buf[off + 1] = static_cast<std::byte>((v >> 8) & 0xFFu);
        buf[off + 2] = static_cast<std::byte>((v >> 16) & 0xFFu);
        buf[off + 3] = static_cast<std::byte>((v >> 24) & 0xFFu);
    };
    put_le32(0, ticks);
    put_le32(4, sessionkey);
    const auto& h1 = password_hash1.bytes();  // 20 wire-LE bytes
    for (std::size_t i = 0; i < h1.size(); ++i) {
        buf[8 + i] = static_cast<std::byte>(h1[i]);
    }

    const hash::BnetDigest digest = hash::blizzard_hash(std::span<const std::byte>{buf});
    const hash::BnetWireHash wire = hash::digest_to_wire(digest);  // 20 LE bytes

    domain::BNHash::Bytes out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = wire[i];
    }
    return domain::BNHash{out};
}

}  // namespace pvpgn::infra::crypto
