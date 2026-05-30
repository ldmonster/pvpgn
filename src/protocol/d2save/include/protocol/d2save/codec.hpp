// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Diablo II save-file (.d2s) header parser, checksum verification, and
/// helpers used by realm-side character-persistence flows.
///
/// The on-disk header layout below mirrors the v1.09 / v1.10 .d2s
/// format. We only decode the fixed-size 765-byte header; the
/// trailing variable-length sections (quests, waypoints, NPC,
/// stats, skills, items, corpse, merc-items, golem) are kept as raw
/// bytes inside `D2SaveFile::raw_data` so the file can be re-emitted
/// unchanged.

#include "core/error.hpp"
#include "core/result.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pvpgn::protocol::d2save {

inline constexpr std::uint32_t D2S_SIGNATURE   = 0xAA55AA55;
inline constexpr std::uint32_t D2S_VERSION_109 = 87;
inline constexpr std::uint32_t D2S_VERSION_110 = 96;

/// Minimum plausible .d2s file size (fixed header).
inline constexpr std::size_t MIN_FILE_SIZE = 335;

/// Decoded .d2s fixed header.
struct D2SaveHeader {
    std::uint32_t              signature        = 0;
    std::uint32_t              version          = 0;
    std::uint32_t              file_size        = 0;
    std::uint32_t              checksum         = 0;
    std::uint32_t              active_weapon    = 0;
    std::array<char, 16>       char_name{};
    std::uint8_t               char_status      = 0;
    std::uint8_t               char_progression = 0;
    std::array<std::uint8_t,2> unk1{};
    std::uint8_t               char_class       = 0;
    std::array<std::uint8_t,2> unk2{};
    std::uint8_t               char_level       = 0;
    std::array<std::uint8_t,4> unk3{};
    std::uint32_t              last_played      = 0;
    std::array<std::uint8_t,4> unk4{};
    std::array<std::uint32_t,16> assigned_skills{};
    std::uint32_t              left_skill       = 0;
    std::uint32_t              right_skill      = 0;
    std::uint32_t              left_swap_skill  = 0;
    std::uint32_t              right_swap_skill = 0;
    std::uint8_t               char_appearance[32]{};
    std::array<std::uint8_t,3> difficulty{};
    std::array<std::uint8_t,4> map_id{};
    std::array<std::uint8_t,2> unk5{};
    std::uint16_t              dead_merc        = 0;
    std::uint32_t              merc_id          = 0;
    std::uint16_t              merc_name_id     = 0;
    std::uint16_t              merc_type        = 0;
    std::uint32_t              merc_experience  = 0;
};

/// Parsed .d2s file. The raw bytes are preserved so callers can
/// re-emit the file with only the checksum patched.
struct D2SaveFile {
    D2SaveHeader              header{};
    std::vector<std::uint8_t> raw_data;
    bool                      valid = false;
};

/// Stateless façade over the .d2s parser/encoder helpers.
class D2SaveCodec {
public:
    static core::Result<D2SaveFile, core::Error>
    parse(std::span<const std::uint8_t> data);

    static bool
    verify_checksum(std::span<const std::uint8_t> data);

    static std::vector<std::uint8_t>
    fix_checksum(std::vector<std::uint8_t> data);

    static core::Result<std::string, core::Error>
    extract_char_name(std::span<const std::uint8_t> data);

    static core::Result<std::uint8_t, core::Error>
    extract_level(std::span<const std::uint8_t> data);

    static core::Result<std::uint8_t, core::Error>
    extract_class(std::span<const std::uint8_t> data);

    static core::Result<bool, core::Error>
    is_expansion(std::span<const std::uint8_t> data);

    static core::Result<bool, core::Error>
    is_hardcore(std::span<const std::uint8_t> data);
};

} // namespace pvpgn::protocol::d2save
