#include <gtest/gtest.h>
#include "protocol/d2save/codec.hpp"
#include <cstring>

namespace pvpgn::protocol::d2save::test {

class D2SaveCodecTest : public ::testing::Test {
protected:
    // Create a minimal valid D2 save file header
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
        
        // Write character name at offset 20
        std::string name = "TestChar";
        std::memcpy(data.data() + 20, name.c_str(), std::min(name.size(), size_t(16)));
        
        // Write character class at offset 36
        data[36] = 0;  // Amazon
        
        // Write character level at offset 40
        data[40] = 1;
        
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
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

TEST_F(D2SaveCodecTest, ExtractCharacterClass) {
    auto data = create_minimal_save();
    auto result = D2SaveCodec::extract_class(data);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0);  // Amazon
}

TEST_F(D2SaveCodecTest, IsExpansion) {
    auto data = create_minimal_save();
    
    // Test non-expansion
    data[36] = 0x00;  // No expansion bit
    auto result = D2SaveCodec::is_expansion(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
    
    // Test expansion
    data[36] = 0x04;  // Expansion bit set
    result = D2SaveCodec::is_expansion(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST_F(D2SaveCodecTest, IsHardcore) {
    auto data = create_minimal_save();
    
    // Test softcore
    data[36] = 0x00;  // No hardcore bit
    auto result = D2SaveCodec::is_hardcore(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
    
    // Test hardcore
    data[36] = 0x01;  // Hardcore bit set
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
