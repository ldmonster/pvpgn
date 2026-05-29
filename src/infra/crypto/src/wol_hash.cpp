// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/crypto/wol_hash.hpp"

#include <array>
#include <cstring>
#include <stdexcept>

namespace pvpgn::v3::infra::crypto {

std::string wol_hash(std::span<const std::uint8_t> data)
{
    if (data.size() > kWolHashMaxInputBytes) {
        throw std::invalid_argument{
            "wol_hash: input exceeds 8 bytes"};
    }

    // Two 9-byte scratch buffers, exactly as in the legacy port.
    // The last byte serves as a sentinel `*(pwd1 + esi)` access.
    std::array<std::uint8_t, kWolHashMaxInputBytes + 1> pwd1{};
    std::array<std::uint8_t, kWolHashMaxInputBytes + 1> pwd2{};

    if (!data.empty()) {
        std::memcpy(pwd1.data(), data.data(), data.size());
    }

    std::uint8_t esi = static_cast<std::uint8_t>(data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        std::uint8_t edx;
        if (pwd1[i] & 1u) {
            edx = static_cast<std::uint8_t>(pwd1[i] << 1);
            edx &= pwd1[esi];
        } else {
            edx = static_cast<std::uint8_t>(pwd1[i] ^ pwd1[esi]);
        }
        pwd2[i] = edx;
        --esi;
    }

    std::string result;
    result.reserve(kWolHashMaxInputBytes);
    for (std::size_t i = 0; i < kWolHashMaxInputBytes; ++i) {
        const std::uint8_t idx = pwd2[i] & 0x3fu;
        result.push_back(kWolHashAlphabet[idx]);
    }
    return result;
}

}  // namespace pvpgn::v3::infra::crypto
