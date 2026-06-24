// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge-case unit tests for DupeChecker (dupe_checker.cpp).
// Complements dupe_checker_test.cpp by exercising the GUID-based confirmed-dupe
// path, the identical-save suspected path, and the various early-skip branches.

#include <catch2/catch_test_macros.hpp>
#include "domain/realm/dupe_checker.hpp"
#include <cstring>

namespace pvpgn::domain::realm::test {

// D2 save magic recognised by dupe_checker.cpp (0x55AAAA55, little-endian bytes
// 0x55 0xAA 0xAA 0x55). Min size is 1000.
inline std::vector<uint8_t> make_d2_save(size_t size = 1100) {
    std::vector<uint8_t> data(size, 0);
    data[0] = 0x55; data[1] = 0xAA; data[2] = 0xAA; data[3] = 0x55;  // magic
    return data;
}

// Embed an item marker ('J' = 0x4A) plus a 4-byte GUID at offset+8 within the
// item section (which starts at byte 767). Position is relative to the section.
inline void embed_guid(std::vector<uint8_t>& data, size_t section_pos, uint32_t guid) {
    constexpr size_t kItemSectionOffset = 767;
    size_t marker = kItemSectionOffset + section_pos;
    data[marker] = 0x4A;
    std::memcpy(data.data() + marker + 8, &guid, sizeof(uint32_t));
}

TEST_CASE("DupeChecker: confirmed dupe on shared item GUID", "[domain][realm]") {
    auto current = make_d2_save();
    auto other   = make_d2_save();

    embed_guid(current, 10, 0xDEADBEEF);
    embed_guid(other, 50, 0xDEADBEEF);  // same GUID -> dupe

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"OtherChar", other});

    auto report = DupeChecker::check("acct", "Mine", current, others);

    REQUIRE(report.result == DupeCheckResult::confirmed_dupe);
    CHECK(report.reason.find("OtherChar") != std::string::npos);
    REQUIRE(report.suspected_items.size() == 1);
    CHECK(report.suspected_items[0] == "OtherChar");
}

TEST_CASE("DupeChecker: distinct GUIDs stay clean", "[domain][realm]") {
    auto current = make_d2_save();
    auto other   = make_d2_save();

    embed_guid(current, 10, 0x11111111);
    embed_guid(other, 10, 0x22222222);  // different GUID

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"OtherChar", other});

    auto report = DupeChecker::check("acct", "Mine", current, others);
    CHECK(report.result == DupeCheckResult::clean);
}

TEST_CASE("DupeChecker: zero GUID is ignored", "[domain][realm]") {
    auto current = make_d2_save();
    auto other   = make_d2_save();

    // Marker present but GUID == 0 in both: extract_item_guids drops zero GUIDs,
    // so no confirmed dupe is reported from the GUID pass.
    embed_guid(current, 10, 0);
    embed_guid(other, 10, 0);

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"OtherChar", other});

    auto report = DupeChecker::check("acct", "Mine", current, others);
    // Both saves are byte-identical, so the hash pass flags them as suspected.
    CHECK(report.result == DupeCheckResult::suspected);
}

TEST_CASE("DupeChecker: identical saves are suspected via hash", "[domain][realm]") {
    // Below the D2 min size so the GUID path is skipped, but large enough to
    // have an item section (> 767). Byte-identical content triggers suspected.
    std::vector<uint8_t> save(900, 0);
    for (size_t i = 0; i < save.size(); ++i) {
        save[i] = static_cast<uint8_t>((i * 7 + 3) % 256);
    }
    auto copy = save;

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"Twin", copy});

    auto report = DupeChecker::check("acct", "Mine", save, others);
    REQUIRE(report.result == DupeCheckResult::suspected);
    CHECK(report.reason.find("Twin") != std::string::npos);
}

TEST_CASE("DupeChecker: different-size saves are not hash-suspected",
          "[domain][realm]") {
    std::vector<uint8_t> save(900, 0xAB);
    std::vector<uint8_t> other(800, 0xAB);  // different length

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"Other", other});

    auto report = DupeChecker::check("acct", "Mine", save, others);
    CHECK(report.result == DupeCheckResult::clean);
}

TEST_CASE("DupeChecker: empty other-save entries are skipped", "[domain][realm]") {
    auto current = make_d2_save();
    embed_guid(current, 10, 0xCAFEBABE);

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"EmptyOne", {}});  // empty other -> skipped

    auto report = DupeChecker::check("acct", "Mine", current, others);
    CHECK(report.result == DupeCheckResult::clean);
}

TEST_CASE("DupeChecker: invalid (non-D2) other save skipped in GUID pass",
          "[domain][realm]") {
    auto current = make_d2_save();
    embed_guid(current, 10, 0x0BADF00D);

    // Other has the same GUID bytes but is NOT a valid D2 save (no magic),
    // so the GUID comparison is skipped for it.
    auto other = make_d2_save();
    other[0] = 0x00;  // break the magic
    embed_guid(other, 10, 0x0BADF00D);

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;
    others.push_back({"NotD2", other});

    auto report = DupeChecker::check("acct", "Mine", current, others);
    CHECK(report.result == DupeCheckResult::clean);
}

TEST_CASE("DupeChecker: no other saves yields clean", "[domain][realm]") {
    auto current = make_d2_save();
    embed_guid(current, 10, 0x12345678);

    std::vector<std::pair<std::string, std::vector<uint8_t>>> others;  // empty
    auto report = DupeChecker::check("acct", "Mine", current, others);
    CHECK(report.result == DupeCheckResult::clean);
}

TEST_CASE("DupeChecker: compute_item_hash returns 0 for tiny saves",
          "[domain][realm]") {
    std::vector<uint8_t> tiny(100, 0xFF);  // <= item-section offset (767)
    CHECK(DupeChecker::compute_item_hash(tiny) == 0);
}

TEST_CASE("DupeChecker: extract_item_guids empty for tiny saves",
          "[domain][realm]") {
    std::vector<uint8_t> tiny(50, 0x4A);
    CHECK(DupeChecker::extract_item_guids(tiny).empty());
}

TEST_CASE("DupeChecker: extract_item_guids finds embedded GUID",
          "[domain][realm]") {
    auto save = make_d2_save();
    embed_guid(save, 20, 0xABCDEF01);

    auto guids = DupeChecker::extract_item_guids(save);
    bool found = false;
    for (uint32_t g : guids) {
        if (g == 0xABCDEF01) found = true;
    }
    CHECK(found);
}

} // namespace pvpgn::domain::realm::test
