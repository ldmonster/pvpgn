// SPDX-License-Identifier: GPL-2.0-or-later
//
// Westwood Online password hash (one-way) as used by the legacy WOL
// implementation in `src/common/wolhash.cpp`. Input is up to 8 bytes
// of password; output is an 8-character ASCII string drawn from a
// fixed 64-character alphabet.
//
// This is the v3 port. Bit-for-bit compatible with the legacy
// algorithm; verified via parity tests against `pvpgn::wol_hash`.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::v3::infra::crypto {

// Maximum supported input length (legacy WOL constraint).
inline constexpr std::size_t kWolHashMaxInputBytes = 8;

// 64-character alphabet used by `wol_hash`. Same as the legacy
// WOL_HASH_CHAR table from the original pvpgn `wolhash`.
inline constexpr std::string_view kWolHashAlphabet =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

// Computes the WOL one-way hash of up to 8 bytes of input. Returns
// the 8-character hash string. Throws `std::invalid_argument` if
// `data.size() > kWolHashMaxInputBytes`.
[[nodiscard]] std::string wol_hash(std::span<const std::uint8_t> data);

}  // namespace pvpgn::v3::infra::crypto
