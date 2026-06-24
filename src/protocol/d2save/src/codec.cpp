#include "protocol/d2save/codec.hpp"
#include "core/error.hpp"
#include <cstring>
#include <algorithm>

namespace pvpgn::protocol::d2save {

namespace {

// Canonical .d2s v1.09/v1.10 (v87 / v96) fixed-header byte offsets.
// Mirrors the original PvPGN constants in src/d2cs/d2charfile.h:
//   D2CHARSAVE_CHARNAME_OFFSET_109 = 0x14 (20)
//   D2CHARSAVE_STATUS_OFFSET_109   = 0x24 (36)
//   D2CHARSAVE_CLASS_OFFSET_109    = 0x28 (40)
//   level lives at status_offset_109 + 7 = 0x2B (43)
constexpr std::size_t D2S_CHARNAME_OFFSET = 0x14; // 20
constexpr std::size_t D2S_STATUS_OFFSET   = 0x24; // 36
constexpr std::size_t D2S_CLASS_OFFSET    = 0x28; // 40
constexpr std::size_t D2S_LEVEL_OFFSET    = 0x2B; // 43

// Status-byte flag bits (original d2charfile.h D2CHARINFO_STATUS_FLAG_*,
// identical to the on-disk D2 status bits):
//   INIT=0x01, HARDCORE=0x04, DEAD=0x08, EXPANSION=0x20, LADDER=0x40
constexpr uint8_t D2S_STATUS_FLAG_HARDCORE  = 0x04;
constexpr uint8_t D2S_STATUS_FLAG_EXPANSION = 0x20;

// CRC32 polynomial for D2 save file checksums
constexpr uint32_t CRC32_POLY = 0xEDB88320;

uint32_t compute_crc32(std::span<const uint8_t> data) {
    uint32_t crc = 0;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

} // namespace

core::Result<D2SaveFile, core::Error> D2SaveCodec::parse(std::span<const uint8_t> data) {
    if (data.size() < MIN_FILE_SIZE) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "D2 save file too small"
        ));
    }

    D2SaveFile result;
    result.raw_data.assign(data.begin(), data.end());

    // Parse header
    size_t offset = 0;
    std::memcpy(&result.header.signature, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (result.header.signature != D2S_SIGNATURE) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Invalid D2 save file signature"
        ));
    }

    std::memcpy(&result.header.version, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (result.header.version != D2S_VERSION_109 && result.header.version != D2S_VERSION_110) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Unsupported D2 save file version"
        ));
    }

    std::memcpy(&result.header.file_size, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&result.header.checksum, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&result.header.active_weapon, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(result.header.char_name.data(), data.data() + offset, 16);
    offset += 16;

    result.header.char_status = data[offset++];
    result.header.char_progression = data[offset++];
    result.header.unk1[0] = data[offset++];
    result.header.unk1[1] = data[offset++];
    result.header.char_class = data[offset++];
    result.header.unk2[0] = data[offset++];
    result.header.unk2[1] = data[offset++];
    result.header.char_level = data[offset++];
    result.header.unk3[0] = data[offset++];
    result.header.unk3[1] = data[offset++];
    result.header.unk3[2] = data[offset++];
    result.header.unk3[3] = data[offset++];

    std::memcpy(&result.header.last_played, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    result.header.unk4[0] = data[offset++];
    result.header.unk4[1] = data[offset++];
    result.header.unk4[2] = data[offset++];
    result.header.unk4[3] = data[offset++];

    for (std::size_t i = 0; i < 16; ++i) {
        std::memcpy(&result.header.assigned_skills[i], data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }

    std::memcpy(&result.header.left_skill, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&result.header.right_skill, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&result.header.left_swap_skill, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&result.header.right_swap_skill, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(result.header.char_appearance, data.data() + offset, 32);
    offset += 32;

    result.header.difficulty[0] = data[offset++];
    result.header.difficulty[1] = data[offset++];
    result.header.difficulty[2] = data[offset++];

    result.header.map_id[0] = data[offset++];
    result.header.map_id[1] = data[offset++];
    result.header.map_id[2] = data[offset++];
    result.header.map_id[3] = data[offset++];

    result.header.unk5[0] = data[offset++];
    result.header.unk5[1] = data[offset++];

    std::memcpy(&result.header.dead_merc, data.data() + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    std::memcpy(&result.header.merc_id, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&result.header.merc_name_id, data.data() + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    std::memcpy(&result.header.merc_type, data.data() + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    std::memcpy(&result.header.merc_experience, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    result.valid = verify_checksum(data);
    return core::Result<D2SaveFile, core::Error>(result);
}

bool D2SaveCodec::verify_checksum(std::span<const uint8_t> data) {
    if (data.size() < 12) {
        return false;
    }

    // Read stored checksum from offset 8
    uint32_t stored_checksum;
    std::memcpy(&stored_checksum, data.data() + 8, sizeof(uint32_t));

    // Compute checksum over data excluding the checksum field itself
    std::vector<uint8_t> temp;
    temp.insert(temp.end(), data.begin(), data.begin() + 8);
    temp.insert(temp.end(), data.begin() + 12, data.end());

    uint32_t computed = compute_crc32(temp);
    return computed == stored_checksum;
}

std::vector<uint8_t> D2SaveCodec::fix_checksum(std::vector<uint8_t> data) {
    if (data.size() < 12) {
        return data;
    }

    // Compute checksum over data excluding the checksum field
    std::vector<uint8_t> temp;
    temp.insert(temp.end(), data.begin(), data.begin() + 8);
    temp.insert(temp.end(), data.begin() + 12, data.end());

    uint32_t computed = compute_crc32(temp);

    // Write checksum back
    std::memcpy(data.data() + 8, &computed, sizeof(uint32_t));
    return data;
}

core::Result<std::string, core::Error> D2SaveCodec::extract_char_name(std::span<const uint8_t> data) {
    if (data.size() < 36) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "D2 save file too small to extract character name"
        ));
    }

    // Character name is at offset 20, 16 bytes, null-terminated
    std::array<char, 16> name_buf;
    std::memcpy(name_buf.data(), data.data() + 20, 16);

    // Find null terminator
    size_t len = 0;
    for (size_t i = 0; i < 16; ++i) {
        if (name_buf[i] == '\0') {
            len = i;
            break;
        }
        len = i + 1;
    }

    return core::Result<std::string, core::Error>(std::string(name_buf.data(), len));
}

core::Result<uint8_t, core::Error> D2SaveCodec::extract_level(std::span<const uint8_t> data) {
    if (data.size() < D2S_LEVEL_OFFSET + 1) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "D2 save file too small to extract level"
        ));
    }

    // Character level is at offset 0x2B (43): status_offset_109 (0x24) + 7.
    return core::Result<uint8_t, core::Error>(data[D2S_LEVEL_OFFSET]);
}

core::Result<uint8_t, core::Error> D2SaveCodec::extract_class(std::span<const uint8_t> data) {
    if (data.size() < D2S_CLASS_OFFSET + 1) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "D2 save file too small to extract class"
        ));
    }

    // Character class is at offset 0x28 (40) = D2CHARSAVE_CLASS_OFFSET_109.
    return core::Result<uint8_t, core::Error>(data[D2S_CLASS_OFFSET]);
}

core::Result<bool, core::Error> D2SaveCodec::is_expansion(std::span<const uint8_t> data) {
    if (data.size() < D2S_STATUS_OFFSET + 1) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "D2 save file too small to check expansion"
        ));
    }

    // Status byte is at offset 0x24 (36); EXPANSION is bit 0x20.
    uint8_t char_status = data[D2S_STATUS_OFFSET];
    return core::Result<bool, core::Error>((char_status & D2S_STATUS_FLAG_EXPANSION) != 0);
}

core::Result<bool, core::Error> D2SaveCodec::is_hardcore(std::span<const uint8_t> data) {
    if (data.size() < D2S_STATUS_OFFSET + 1) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "D2 save file too small to check hardcore"
        ));
    }

    // Status byte is at offset 0x24 (36); HARDCORE is bit 0x04.
    uint8_t char_status = data[D2S_STATUS_OFFSET];
    return core::Result<bool, core::Error>((char_status & D2S_STATUS_FLAG_HARDCORE) != 0);
}

} // namespace pvpgn::protocol::d2save
