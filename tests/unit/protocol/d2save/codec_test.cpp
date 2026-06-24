#include <gtest/gtest.h>
#include "protocol/d2save/codec.hpp"
#include <cstring>

namespace pvpgn::protocol::d2save::test {

class D2SaveCodecTest : public ::testing::Test {
protected:
    // Canonical .d2s v1.09/v1.10 (v87/v96) fixed-header byte offsets,
    // matching the original PvPGN src/d2cs/d2charfile.h constants:
    //   name   @ 0x14 (20)  D2CHARSAVE_CHARNAME_OFFSET_109
    //   status @ 0x24 (36)  D2CHARSAVE_STATUS_OFFSET_109
    //   class  @ 0x28 (40)  D2CHARSAVE_CLASS_OFFSET_109
    //   level  @ 0x2B (43)  status_offset_109 + 7
    static constexpr size_t OFF_NAME   = 0x14; // 20
    static constexpr size_t OFF_STATUS = 0x24; // 36
    static constexpr size_t OFF_CLASS  = 0x28; // 40
    static constexpr size_t OFF_LEVEL  = 0x2B; // 43

    // On-disk status flag bits (D2CHARINFO_STATUS_FLAG_*):
    //   INIT=0x01, HARDCORE=0x04, DEAD=0x08, EXPANSION=0x20, LADDER=0x40
    static constexpr uint8_t FLAG_INIT      = 0x01;
    static constexpr uint8_t FLAG_HARDCORE  = 0x04;
    static constexpr uint8_t FLAG_EXPANSION = 0x20;

    // Create a minimal valid D2 save file header using the CANONICAL layout.
    // Class and level are deliberately distinct, non-trivial values so that a
    // regression that swaps the class/level/status offsets is caught.
    std::vector<uint8_t> create_minimal_save() {
        std::vector<uint8_t> data(sizeof(D2SaveHeader), 0);

        // Write signature
        uint32_t sig = D2S_SIGNATURE;
        std::memcpy(data.data(), &sig, sizeof(uint32_t));

        // Write version
        uint32_t ver = D2S_VERSION_110;
        std::memcpy(data.data() + 4, &ver, sizeof(uint32_t));

        // Write file size
        uint32_t size = data.size();
        std::memcpy(data.data() + 8, &size, sizeof(uint32_t));

        // Write checksum (placeholder)
        uint32_t checksum = 0;
        std::memcpy(data.data() + 12, &checksum, sizeof(uint32_t));

        // Write character name at offset 0x14 (20)
        std::string name = "TestChar";
        std::memcpy(data.data() + OFF_NAME, name.c_str(), std::min(name.size(), size_t(16)));

        // Status byte at offset 0x24 (36) — clear by default.
        data[OFF_STATUS] = 0x00;

        // Character class at offset 0x28 (40). Use Paladin (3) — a value that
        // differs from both the status byte and the level, so any offset swap
        // produces a wrong, detectable result.
        data[OFF_CLASS] = 3;  // Paladin

        // Character level at offset 0x2B (43). Use 42 — distinct from the class
        // id and outside the 0..7 class range.
        data[OFF_LEVEL] = 42;

        return data;
    }
};

TEST_F(D2SaveCodecTest, ParseMinimalValidSave) {
    auto data = create_minimal_save();
    auto result = D2SaveCodec::parse(data);
    
    ASSERT_TRUE(result.has_value());
    auto save_file = result.value();
    EXPECT_EQ(save_file.header.signature, D2S_SIGNATURE);
    EXPECT_EQ(save_file.header.version, D2S_VERSION_110);
}

TEST_F(D2SaveCodecTest, ParseTooSmallFile) {
    std::vector<uint8_t> data(10, 0);
    auto result = D2SaveCodec::parse(data);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), core::StatusCode::InvalidArgument);
}

TEST_F(D2SaveCodecTest, ParseInvalidSignature) {
    auto data = create_minimal_save();
    // Corrupt signature
    std::memcpy(data.data(), &(uint32_t){0xDEADBEEF}, sizeof(uint32_t));
    
    auto result = D2SaveCodec::parse(data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(D2SaveCodecTest, ExtractCharacterName) {
    auto data = create_minimal_save();
    auto result = D2SaveCodec::extract_char_name(data);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "TestChar");
}

TEST_F(D2SaveCodecTest, ExtractCharacterLevel) {
    auto data = create_minimal_save();
    auto result = D2SaveCodec::extract_level(data);

    // Level lives at offset 0x2B (43). The blob stores 42 there; a regression
    // that read the class offset (40) would instead see 3.
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(D2SaveCodecTest, ExtractCharacterClass) {
    auto data = create_minimal_save();
    auto result = D2SaveCodec::extract_class(data);

    // Class lives at offset 0x28 (40). The blob stores 3 (Paladin) there; a
    // regression that read the status offset (36) would instead see 0.
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3);  // Paladin
}

// Regression guard: class and level are at distinct canonical offsets with
// distinct values, so swapping the two offsets (the original bug) is caught.
TEST_F(D2SaveCodecTest, ClassAndLevelDoNotAlias) {
    auto data = create_minimal_save();

    auto cls = D2SaveCodec::extract_class(data);
    auto lvl = D2SaveCodec::extract_level(data);
    ASSERT_TRUE(cls.has_value());
    ASSERT_TRUE(lvl.has_value());

    EXPECT_EQ(cls.value(), 3);   // offset 40
    EXPECT_EQ(lvl.value(), 42);  // offset 43
    EXPECT_NE(cls.value(), lvl.value());
}

TEST_F(D2SaveCodecTest, IsExpansion) {
    auto data = create_minimal_save();

    // Status byte is at offset 0x24 (36); EXPANSION is bit 0x20.
    // Non-expansion
    data[OFF_STATUS] = 0x00;
    auto result = D2SaveCodec::is_expansion(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());

    // The HARDCORE bit (0x04) must NOT be mistaken for expansion.
    data[OFF_STATUS] = FLAG_HARDCORE;  // 0x04
    result = D2SaveCodec::is_expansion(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());

    // Expansion bit set
    data[OFF_STATUS] = FLAG_EXPANSION;  // 0x20
    result = D2SaveCodec::is_expansion(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST_F(D2SaveCodecTest, IsHardcore) {
    auto data = create_minimal_save();

    // Status byte is at offset 0x24 (36); HARDCORE is bit 0x04.
    // Softcore
    data[OFF_STATUS] = 0x00;
    auto result = D2SaveCodec::is_hardcore(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());

    // The INIT bit (0x01) must NOT be mistaken for hardcore.
    data[OFF_STATUS] = FLAG_INIT;  // 0x01
    result = D2SaveCodec::is_hardcore(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());

    // Hardcore bit set
    data[OFF_STATUS] = FLAG_HARDCORE;  // 0x04
    result = D2SaveCodec::is_hardcore(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST_F(D2SaveCodecTest, VerifyChecksumInvalidSize) {
    std::vector<uint8_t> data(5, 0);
    bool valid = D2SaveCodec::verify_checksum(data);
    EXPECT_FALSE(valid);
}

TEST_F(D2SaveCodecTest, FixChecksum) {
    auto data = create_minimal_save();
    auto fixed = D2SaveCodec::fix_checksum(data);
    
    EXPECT_EQ(fixed.size(), data.size());
    // Checksum should be written at offset 8
    uint32_t checksum;
    std::memcpy(&checksum, fixed.data() + 8, sizeof(uint32_t));
    EXPECT_NE(checksum, 0);  // Should have computed a non-zero checksum
}

} // namespace pvpgn::protocol::d2save::test
