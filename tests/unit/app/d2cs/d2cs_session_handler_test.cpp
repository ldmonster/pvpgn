// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2cs_session_handler_test.cpp
/// Unit tests for `D2CSSessionHandler`.
///
/// Test count: 14 TEST_CASEs, 40+ CHECK/REQUIRE assertions.
///
/// Strategy:
///   - Use `InMemoryCharacterRepository` and `InMemoryLadderRepository`
///     as real (non-mock) collaborators.
///   - Use `MockD2CSEgress` to capture outbound calls for assertion.
///   - Invoke handler callbacks directly via `make_callbacks()`.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/d2cs/d2cs_session_egress.hpp"
#include "app/d2cs/d2cs_session_handler.hpp"
#include "domain/d2cs/in_memory_repositories.hpp"
#include "domain/d2cs/types.hpp"
#include "protocol/d2cs/fsm.hpp"

using namespace pvpgn;
using namespace pvpgn::domain::d2cs;
using namespace pvpgn::app::d2cs;
using namespace pvpgn::protocol::d2cs;

// ---------------------------------------------------------------------------
// MockD2CSEgress — captures all outbound calls
// ---------------------------------------------------------------------------

namespace {

struct MockD2CSEgress final : public ID2CSSessionEgress {
    // send_char_list
    bool                        char_list_called{false};
    std::vector<CharacterInfo>  char_list_chars;

    // send_char_list_result
    bool char_list_result_called{false};
    bool char_list_result_success{false};

    // send_char_select_result
    bool                        char_select_called{false};
    bool                        char_select_success{false};
    std::optional<CharacterInfo> char_select_info;

    // send_char_create_result
    bool char_create_called{false};
    domain::d2cs::CharacterCreateResult char_create_result{
        domain::d2cs::CharacterCreateResult::Failed};

    // send_char_delete_result
    bool char_delete_called{false};
    bool char_delete_success{false};

    // send_ladder
    bool                      ladder_called{false};
    std::vector<LadderEntry>  ladder_entries;

    // send_realm_logon_result
    bool              logon_called{false};
    RealmLogonResult  logon_result{RealmLogonResult::Success};

    // send_motd
    bool              motd_called{false};
    std::string       motd_text;

    void send_char_list(const std::vector<CharacterInfo>& chars) override {
        char_list_called = true;
        char_list_chars  = chars;
    }

    void send_char_list_result(bool success) override {
        char_list_result_called  = true;
        char_list_result_success = success;
    }

    void send_char_select_result(bool success,
                                 const CharacterInfo* info) override {
        char_select_called  = true;
        char_select_success = success;
        if (info) {
            char_select_info = *info;
        } else {
            char_select_info = std::nullopt;
        }
    }

    void send_char_create_result(
        domain::d2cs::CharacterCreateResult result) override {
        char_create_called = true;
        char_create_result = result;
    }

    void send_char_delete_result(bool success) override {
        char_delete_called  = true;
        char_delete_success = success;
    }

    void send_ladder(const std::vector<LadderEntry>& entries) override {
        ladder_called  = true;
        ladder_entries = entries;
    }

    void send_realm_logon_result(RealmLogonResult result) override {
        logon_called = true;
        logon_result = result;
    }

    void send_motd(std::string_view message) override {
        motd_called = true;
        motd_text   = std::string(message);
    }

    void reset() {
        *this = MockD2CSEgress{};
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a minimal valid CharacterInfo.
CharacterInfo make_char(std::string name,
                        CharacterClass cls   = CharacterClass::Sorceress,
                        uint8_t        level = 10,
                        uint32_t       xp    = 1000) {
    CharacterInfo c;
    c.name        = std::move(name);
    c.class_      = cls;
    c.level       = level;
    c.experience  = xp;
    c.flags       = CharacterFlags::None;
    c.last_played = 0;
    return c;
}

/// Build a LadderEntry.
LadderEntry make_ladder_entry(std::string char_name,
                              std::string account_name,
                              uint32_t    xp,
                              CharacterClass cls = CharacterClass::Barbarian) {
    LadderEntry e;
    e.character_name = std::move(char_name);
    e.account_name   = std::move(account_name);
    e.class_         = cls;
    e.level          = 50;
    e.experience     = xp;
    e.rank           = 0;
    return e;
}

/// Fixture: repos + egress + handler + callbacks, all wired together.
struct Fixture {
    InMemoryCharacterRepository char_repo;
    InMemoryLadderRepository    ladder_repo;
    MockD2CSEgress              egress;
    D2CSSessionHandler          handler{char_repo, ladder_repo, egress};
    D2CSFsmCallbacks            cb{handler.make_callbacks()};

    /// Fire on_login to set the session account name.
    void login(std::string account = "TestAccount") {
        D2CSLoginRequest req;
        req.account_name = std::move(account);
        req.seqno        = 1;
        req.session_key  = 0;
        REQUIRE(cb.on_login(req).has_value());
        egress.reset();  // clear logon call so tests start clean
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// TC-01: on_char_list with populated repo → send_char_list with correct data
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_list populated repo sends char list",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Alice");

    REQUIRE(f.char_repo.save_character("Alice", make_char("Sorc1")));
    REQUIRE(f.char_repo.save_character("Alice", make_char("Barb2", CharacterClass::Barbarian)));

    D2CSCharListRequest req;
    req.seqno = 1;
    REQUIRE(f.cb.on_char_list(req).has_value());

    REQUIRE(f.egress.char_list_called);
    REQUIRE(f.egress.char_list_chars.size() == 2);
    CHECK(f.egress.char_list_chars[0].name == "Sorc1");
    CHECK(f.egress.char_list_chars[1].name == "Barb2");
    CHECK_FALSE(f.egress.char_list_result_called);
}

// ---------------------------------------------------------------------------
// TC-02: on_char_list with empty repo → send_char_list with empty vector
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_list empty repo sends empty list",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Bob");

    D2CSCharListRequest req;
    req.seqno = 1;
    REQUIRE(f.cb.on_char_list(req).has_value());

    REQUIRE(f.egress.char_list_called);
    CHECK(f.egress.char_list_chars.empty());
}

// ---------------------------------------------------------------------------
// TC-03: on_char_login found → send_char_select_result(true, &info)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_login found sends success",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Carol");

    auto ch = make_char("MyChar", CharacterClass::Paladin, 42, 99999);
    REQUIRE(f.char_repo.save_character("Carol", ch));

    // CHARLOGINREQ carries only the char name; the account is the session
    // account set by f.login("Carol").
    D2CSCharLoginRequest req;
    req.char_name    = "MyChar";
    REQUIRE(f.cb.on_char_login(req).has_value());

    REQUIRE(f.egress.char_select_called);
    CHECK(f.egress.char_select_success);
    REQUIRE(f.egress.char_select_info.has_value());
    CHECK(f.egress.char_select_info->name == "MyChar");
    CHECK(f.egress.char_select_info->level == 42);
}

// ---------------------------------------------------------------------------
// TC-04: on_char_login not found → send_char_select_result(false, nullptr)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_login not found sends failure",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Dave");

    D2CSCharLoginRequest req;
    req.char_name    = "Ghost";
    REQUIRE(f.cb.on_char_login(req).has_value());

    REQUIRE(f.egress.char_select_called);
    CHECK_FALSE(f.egress.char_select_success);
    CHECK_FALSE(f.egress.char_select_info.has_value());
}

// ---------------------------------------------------------------------------
// TC-05: on_create_char success → send_char_create_result(true)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_create_char success sends true",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Eve");

    D2CSCreateCharRequest req;
    req.char_class  = static_cast<uint16_t>(CharacterClass::Amazon);
    req.char_status = static_cast<uint16_t>(CharacterFlags::None);
    req.char_name   = "NewAmazon";
    REQUIRE(f.cb.on_create_char(req).has_value());

    REQUIRE(f.egress.char_create_called);
    CHECK(f.egress.char_create_result ==
          domain::d2cs::CharacterCreateResult::Succeed);

    // Verify the character was actually persisted.
    auto found = f.char_repo.find_character("Eve", "NewAmazon");
    REQUIRE(found.has_value());
    CHECK(found->class_ == CharacterClass::Amazon);
}

// ---------------------------------------------------------------------------
// TC-06: on_create_char duplicate → send_char_create_result(Rejected/0x14)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_create_char duplicate sends Rejected",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Frank");

    // Pre-seed the character.
    REQUIRE(f.char_repo.save_character("Frank", make_char("Dupe")));

    D2CSCreateCharRequest req;
    req.char_class  = static_cast<uint16_t>(CharacterClass::Necromancer);
    req.char_status = static_cast<uint16_t>(CharacterFlags::None);
    req.char_name   = "Dupe";
    REQUIRE(f.cb.on_create_char(req).has_value());

    REQUIRE(f.egress.char_create_called);
    CHECK(f.egress.char_create_result ==
          domain::d2cs::CharacterCreateResult::Rejected);
}

// ---------------------------------------------------------------------------
// on_motd → send_motd (MOTDREPLY) — the original always replies; v3 sends the
// conventional default text.
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionHandler: on_motd sends a MOTD reply",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Moe");

    protocol::d2cs::D2CSMotdRequest req;
    REQUIRE(f.cb.on_motd(req).has_value());

    REQUIRE(f.egress.motd_called);
    CHECK(f.egress.motd_text == "No Message Of The Day Set");
}

// ---------------------------------------------------------------------------
// TC-07: on_delete_char success → send_char_delete_result(true)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_delete_char success sends true",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Grace");

    REQUIRE(f.char_repo.save_character("Grace", make_char("ToDelete")));

    D2CSDeleteCharRequest req;
    req.char_name = "ToDelete";
    REQUIRE(f.cb.on_delete_char(req).has_value());

    REQUIRE(f.egress.char_delete_called);
    CHECK(f.egress.char_delete_success);

    // Verify the character is gone.
    auto found = f.char_repo.find_character("Grace", "ToDelete");
    CHECK_FALSE(found.has_value());
}

// ---------------------------------------------------------------------------
// TC-08: on_delete_char not found → send_char_delete_result(false)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_delete_char not found sends false",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Hank");

    D2CSDeleteCharRequest req;
    req.char_name = "NoSuchChar";
    REQUIRE(f.cb.on_delete_char(req).has_value());

    REQUIRE(f.egress.char_delete_called);
    CHECK_FALSE(f.egress.char_delete_success);
}

// ---------------------------------------------------------------------------
// TC-09: on_ladder populated → send_ladder with entries
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_ladder populated sends entries",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Iris");

    f.ladder_repo.add_entry(LadderType::Standard,
        make_ladder_entry("TopChar", "Iris", 500000));
    f.ladder_repo.add_entry(LadderType::Standard,
        make_ladder_entry("SecondChar", "Iris", 300000));

    D2CSLadderRequest req;
    req.ladder_type = static_cast<uint8_t>(LadderType::Standard);
    req.start_pos   = 0;
    REQUIRE(f.cb.on_ladder(req).has_value());

    REQUIRE(f.egress.ladder_called);
    REQUIRE(f.egress.ladder_entries.size() == 2);
    // Entries are sorted by rank (highest XP = rank 1).
    CHECK(f.egress.ladder_entries[0].character_name == "TopChar");
    CHECK(f.egress.ladder_entries[1].character_name == "SecondChar");
}

// ---------------------------------------------------------------------------
// TC-10: on_ladder empty → send_ladder with empty vector
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_ladder empty sends empty vector",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Jack");

    D2CSLadderRequest req;
    req.ladder_type = static_cast<uint8_t>(LadderType::Hardcore);
    req.start_pos   = 0;
    REQUIRE(f.cb.on_ladder(req).has_value());

    REQUIRE(f.egress.ladder_called);
    CHECK(f.egress.ladder_entries.empty());
}

// ---------------------------------------------------------------------------
// TC-11: on_realm_logon → send_realm_logon_result(Success)
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_login sends realm logon success",
          "[app][d2cs][handler]")
{
    Fixture f;
    // Do NOT call f.login() — we want to observe the raw logon call.

    D2CSLoginRequest req;
    req.account_name = "Kate";
    req.seqno        = 1;
    req.session_key  = 0xDEADBEEF;
    REQUIRE(f.cb.on_login(req).has_value());

    REQUIRE(f.egress.logon_called);
    CHECK(f.egress.logon_result == RealmLogonResult::Success);
}

// ---------------------------------------------------------------------------
// TC-12: on_char_list_110 → same result as on_char_list
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_list_110 sends char list (1.10+ variant)",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Leo");

    REQUIRE(f.char_repo.save_character("Leo", make_char("LeoChar")));

    D2CSCharListRequest req;
    req.seqno = 1;
    REQUIRE(f.cb.on_char_list_110(req).has_value());

    REQUIRE(f.egress.char_list_called);
    REQUIRE(f.egress.char_list_chars.size() == 1);
    CHECK(f.egress.char_list_chars[0].name == "LeoChar");
}

// ---------------------------------------------------------------------------
// TC-13: on_char_ladder found → send_ladder with single entry
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_ladder found sends single entry",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Mia");

    f.ladder_repo.add_entry(LadderType::Standard,
        make_ladder_entry("MiaChar", "Mia", 750000));

    D2CSCharLadderRequest req;
    req.char_name = "MiaChar";
    req.hardcore  = 0;
    req.expansion = 0;
    REQUIRE(f.cb.on_char_ladder(req).has_value());

    REQUIRE(f.egress.ladder_called);
    REQUIRE(f.egress.ladder_entries.size() == 1);
    CHECK(f.egress.ladder_entries[0].character_name == "MiaChar");
}

// ---------------------------------------------------------------------------
// TC-14: on_char_ladder not found → send_ladder with empty vector
// ---------------------------------------------------------------------------

TEST_CASE("D2CSSessionHandler: on_char_ladder not found sends empty vector",
          "[app][d2cs][handler]")
{
    Fixture f;
    f.login("Ned");

    D2CSCharLadderRequest req;
    req.char_name = "NoSuchChar";
    req.hardcore  = 0;
    req.expansion = 0;
    REQUIRE(f.cb.on_char_ladder(req).has_value());

    REQUIRE(f.egress.ladder_called);
    CHECK(f.egress.ladder_entries.empty());
}
