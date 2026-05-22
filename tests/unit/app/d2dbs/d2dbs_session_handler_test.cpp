// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2dbs_session_handler_test.cpp
/// Unit tests for `D2DBSSessionHandler`.
///
/// Test count: 10 TEST_CASEs, 40+ CHECK/REQUIRE assertions.
///
/// Strategy:
///   - Use `InMemoryCharacterSaveRepository` and `InMemoryD2DBSLadderRepository`
///     as real (non-mock) collaborators.
///   - Use `MockD2DBSEgress` to capture outbound calls for assertion.
///   - Invoke handler callbacks directly via `make_callbacks()`.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/d2dbs/d2dbs_session_egress.hpp"
#include "app/d2dbs/d2dbs_session_handler.hpp"
#include "domain/d2dbs/in_memory_repositories.hpp"
#include "domain/d2dbs/types.hpp"
#include "protocol/d2dbs/fsm.hpp"

using namespace pvpgn;
using namespace pvpgn::domain::d2dbs;
using namespace pvpgn::app::d2dbs;
using namespace pvpgn::protocol::d2dbs;

// ---------------------------------------------------------------------------
// MockD2DBSEgress — captures all outbound calls
// ---------------------------------------------------------------------------

namespace {

struct MockD2DBSEgress final : public ID2DBSSessionEgress {
    // send_char_login_result
    bool char_login_called{false};
    bool char_login_success{false};

    // send_char_logout_result
    bool char_logout_called{false};
    bool char_logout_success{false};

    // send_char_save_result
    bool char_save_called{false};
    bool char_save_success{false};

    // send_char_load_result
    bool                              char_load_called{false};
    bool                              char_load_success{false};
    std::optional<CharacterSaveData>  char_load_data;

    // send_ladder_update_result
    bool ladder_update_called{false};
    bool ladder_update_success{false};

    void send_char_login_result(bool success) override {
        char_login_called  = true;
        char_login_success = success;
    }

    void send_char_logout_result(bool success) override {
        char_logout_called  = true;
        char_logout_success = success;
    }

    void send_char_save_result(bool success) override {
        char_save_called  = true;
        char_save_success = success;
    }

    void send_char_load_result(bool success,
                                const CharacterSaveData* data) override {
        char_load_called  = true;
        char_load_success = success;
        if (data) {
            char_load_data = *data;
        } else {
            char_load_data = std::nullopt;
        }
    }

    void send_ladder_update_result(bool success) override {
        ladder_update_called  = true;
        ladder_update_success = success;
    }

    void reset() {
        *this = MockD2DBSEgress{};
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a minimal CharacterSaveData and seed it into the repo.
void seed_save(InMemoryCharacterSaveRepository& repo,
               std::string account,
               std::string char_name,
               std::vector<uint8_t> data = {0xAA, 0xBB}) {
    CharacterSaveData d;
    d.account_name = std::move(account);
    d.char_name    = std::move(char_name);
    d.realm_name   = "USEast";
    d.data         = std::move(data);
    d.timestamp    = 1000;
    REQUIRE(repo.save(d));
}

/// Build a D2DBSCharSaveData request.
D2DBSCharSaveData make_save_req(std::string account,
                                 std::string char_name,
                                 std::vector<uint8_t> data = {0x01, 0x02}) {
    D2DBSCharSaveData req;
    req.seqno        = 1;
    req.datatype     = D2DBSDataType::CHAR_SAVE;
    req.account_name = std::move(account);
    req.char_name    = std::move(char_name);
    req.realm_name   = "USEast";
    req.data         = std::move(data);
    return req;
}

/// Build a D2DBSCharLoadData request.
D2DBSCharLoadData make_load_req(std::string account, std::string char_name) {
    D2DBSCharLoadData req;
    req.seqno        = 2;
    req.datatype     = D2DBSDataType::CHAR_SAVE;
    req.account_name = std::move(account);
    req.char_name    = std::move(char_name);
    req.realm_name   = "USEast";
    return req;
}

/// Build a D2DBSCharLockReq (lock = lockstatus != 0).
D2DBSCharLockReq make_lock_req(std::string account,
                                std::string char_name,
                                uint32_t    lockstatus) {
    D2DBSCharLockReq req;
    req.seqno        = 3;
    req.lockstatus   = lockstatus;
    req.account_name = std::move(account);
    req.char_name    = std::move(char_name);
    req.realm_name   = "USEast";
    return req;
}

/// Build a D2DBSCharLadderData request.
D2DBSCharLadderData make_ladder_req(std::string char_name,
                                     uint32_t    level    = 50,
                                     uint32_t    explow   = 500000,
                                     uint32_t    exphigh  = 0,
                                     uint16_t    cls      = 3,
                                     uint16_t    status   = 0) {
    D2DBSCharLadderData req;
    req.seqno       = 4;
    req.charlevel   = level;
    req.charexplow  = explow;
    req.charexphigh = exphigh;
    req.charclass   = cls;
    req.charstatus  = status;
    req.char_name   = std::move(char_name);
    req.realm_name  = "USEast";
    return req;
}

/// Fixture: repos + egress + handler + callbacks, all wired together.
struct Fixture {
    InMemoryCharacterSaveRepository save_repo;
    InMemoryD2DBSLadderRepository   ladder_repo;
    MockD2DBSEgress                 egress;
    D2DBSSessionHandler             handler{save_repo, ladder_repo, egress};
    D2DBSFsmCallbacks               cb{handler.make_callbacks()};
};

} // anonymous namespace

// ===========================================================================
// TC-01: on_char_save — new character → save_result(true)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_save new character succeeds",
          "[app][d2dbs][handler]")
{
    Fixture f;

    auto req = make_save_req("alice", "Sorc1", {0xDE, 0xAD, 0xBE, 0xEF});
    REQUIRE(f.cb.on_char_save(req).has_value());

    REQUIRE(f.egress.char_save_called);
    CHECK(f.egress.char_save_success);
    CHECK(f.save_repo.save_count() == 1);
}

// ===========================================================================
// TC-02: on_char_save — overwrite existing → save_result(true)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_save overwrites existing character",
          "[app][d2dbs][handler]")
{
    Fixture f;
    seed_save(f.save_repo, "alice", "Sorc1", {0x01});

    auto req = make_save_req("alice", "Sorc1", {0x02, 0x03});
    REQUIRE(f.cb.on_char_save(req).has_value());

    REQUIRE(f.egress.char_save_called);
    CHECK(f.egress.char_save_success);
    // Still only one entry (overwrite)
    CHECK(f.save_repo.save_count() == 1);
}

// ===========================================================================
// TC-03: on_char_load — existing character → load_result(true, &data)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_load existing character succeeds",
          "[app][d2dbs][handler]")
{
    Fixture f;
    seed_save(f.save_repo, "alice", "Sorc1", {0xCA, 0xFE});

    auto req = make_load_req("alice", "Sorc1");
    REQUIRE(f.cb.on_char_load(req).has_value());

    REQUIRE(f.egress.char_load_called);
    CHECK(f.egress.char_load_success);
    REQUIRE(f.egress.char_load_data.has_value());
    CHECK(f.egress.char_load_data->char_name    == "Sorc1");
    CHECK(f.egress.char_load_data->account_name == "alice");
    CHECK(f.egress.char_load_data->data         == std::vector<uint8_t>{0xCA, 0xFE});
}

// ===========================================================================
// TC-04: on_char_load — non-existent character → load_result(false, nullptr)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_load non-existent character fails",
          "[app][d2dbs][handler]")
{
    Fixture f;

    auto req = make_load_req("alice", "Ghost");
    REQUIRE(f.cb.on_char_load(req).has_value());

    REQUIRE(f.egress.char_load_called);
    CHECK_FALSE(f.egress.char_load_success);
    CHECK_FALSE(f.egress.char_load_data.has_value());
}

// ===========================================================================
// TC-05: on_char_lock (lock) — existing unlocked → login_result(true)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_lock lock existing character succeeds",
          "[app][d2dbs][handler]")
{
    Fixture f;
    seed_save(f.save_repo, "alice", "Sorc1");

    auto req = make_lock_req("alice", "Sorc1", 1 /* lock */);
    REQUIRE(f.cb.on_char_lock(req).has_value());

    REQUIRE(f.egress.char_login_called);
    CHECK(f.egress.char_login_success);
    CHECK_FALSE(f.egress.char_logout_called);
    CHECK(f.save_repo.lock_count() == 1);
}

// ===========================================================================
// TC-06: on_char_lock (lock) — non-existent → login_result(false)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_lock lock non-existent character fails",
          "[app][d2dbs][handler]")
{
    Fixture f;

    auto req = make_lock_req("alice", "Ghost", 1 /* lock */);
    REQUIRE(f.cb.on_char_lock(req).has_value());

    REQUIRE(f.egress.char_login_called);
    CHECK_FALSE(f.egress.char_login_success);
    CHECK(f.save_repo.lock_count() == 0);
}

// ===========================================================================
// TC-07: on_char_lock (unlock) — locked character → logout_result(true)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_lock unlock locked character succeeds",
          "[app][d2dbs][handler]")
{
    Fixture f;
    seed_save(f.save_repo, "alice", "Sorc1");
    REQUIRE(f.save_repo.lock("alice", "Sorc1"));

    auto req = make_lock_req("alice", "Sorc1", 0 /* unlock */);
    REQUIRE(f.cb.on_char_lock(req).has_value());

    REQUIRE(f.egress.char_logout_called);
    CHECK(f.egress.char_logout_success);
    CHECK_FALSE(f.egress.char_login_called);
    CHECK(f.save_repo.lock_count() == 0);
}

// ===========================================================================
// TC-08: on_char_lock (unlock) — not locked → logout_result(false)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_lock unlock non-locked character fails",
          "[app][d2dbs][handler]")
{
    Fixture f;
    seed_save(f.save_repo, "alice", "Sorc1");

    auto req = make_lock_req("alice", "Sorc1", 0 /* unlock */);
    REQUIRE(f.cb.on_char_lock(req).has_value());

    REQUIRE(f.egress.char_logout_called);
    CHECK_FALSE(f.egress.char_logout_success);
}

// ===========================================================================
// TC-09: on_char_ladder — new entry → ladder_update_result(true)
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_char_ladder inserts new entry",
          "[app][d2dbs][handler]")
{
    Fixture f;

    auto req = make_ladder_req("Sorc1", 75, 999999, 0, 3, 0);
    REQUIRE(f.cb.on_char_ladder(req).has_value());

    REQUIRE(f.egress.ladder_update_called);
    CHECK(f.egress.ladder_update_success);
    CHECK(f.ladder_repo.entry_count() == 1);

    auto entry = f.ladder_repo.find_entry("Sorc1");
    REQUIRE(entry.has_value());
    CHECK(entry->level      == 75);
    CHECK(entry->char_class == 3);
    // experience = (exphigh << 32) | explow = 0 | 999999
    CHECK(entry->experience == 999999);
}

// ===========================================================================
// TC-10: on_echo_reply — no-op, returns success
// ===========================================================================

TEST_CASE("D2DBSSessionHandler: on_echo_reply is a no-op",
          "[app][d2dbs][handler]")
{
    Fixture f;

    D2DBSEchoReply req;
    req.seqno = 99;
    REQUIRE(f.cb.on_echo_reply(req).has_value());

    // No egress calls should have been made
    CHECK_FALSE(f.egress.char_save_called);
    CHECK_FALSE(f.egress.char_load_called);
    CHECK_FALSE(f.egress.char_login_called);
    CHECK_FALSE(f.egress.char_logout_called);
    CHECK_FALSE(f.egress.ladder_update_called);
}
