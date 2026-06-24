// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_ip_ban_repository_test.cpp
//
// Verifies the SqlIpBanRepository over the recording fake IDbDriver
// (no sqlite). Covers the two-table model (exact ip_bans + CIDR ip_ban_ranges):
// bound writes, is_banned via an exact hit and via in-process CIDR matching,
// for_each_entry mapping, load_banlist, and the transactional save_banlist.

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "infra/persistence/ip_ban_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::AccountId;
using pvpgn::domain::IpAddress;
using pvpgn::domain::moderation::IpBanEntry;
using pvpgn::domain::moderation::IpBanList;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

IpAddress ip(std::string_view s) { return IpAddress::parse(s).value(); }

pvpgn::core::SystemTime epoch_plus(std::int64_t secs) {
    return pvpgn::core::SystemTime{} + std::chrono::seconds{secs};
}

// ip_bans row: ip, reason, issuer, issued_at, expires_at
FakeRow entry_row(std::string ip_s, std::string reason, std::int64_t issuer,
                  std::int64_t issued, Cell expires) {
    return FakeRow{std::vector<Cell>{std::move(ip_s), std::move(reason), issuer,
                                     issued, std::move(expires)}};
}

}  // namespace

TEST_CASE("SqlIpBanRepository::add_ban issues the bound upsert",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};

    IpBanEntry e;
    e.ip        = ip("203.0.113.7");
    e.reason    = "abuse";
    e.issuer    = AccountId{7};
    e.issued_at = epoch_plus(100);
    e.expires_at = epoch_plus(900);

    REQUIRE(repo.add_ban(e).has_value());
    const auto& call = driver->last();
    CHECK(call.sql.find("INSERT OR REPLACE INTO ip_bans") != std::string::npos);
    REQUIRE(call.params.size() == 5);
    CHECK(as_str(call.params[0]) == "203.0.113.7");
    CHECK(as_str(call.params[1]) == "abuse");
    CHECK(as_int(call.params[2]) == 7);
    CHECK(as_int(call.params[3]) == 100);
    CHECK(as_int(call.params[4]) == 900);
}

TEST_CASE("SqlIpBanRepository::add_ban binds NULL for a permanent ban",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};

    IpBanEntry e;
    e.ip        = ip("203.0.113.8");
    e.issuer    = AccountId{1};
    e.issued_at = epoch_plus(1);
    e.expires_at = std::nullopt;

    REQUIRE(repo.add_ban(e).has_value());
    CHECK(is_null_param(driver->last().params.at(4)));
}

TEST_CASE("SqlIpBanRepository::add_range_ban writes the range table",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};

    REQUIRE(repo.add_range_ban(ip("10.0.0.0"), 8, "lan block", AccountId{2},
                               epoch_plus(5), std::nullopt)
                .has_value());
    const auto& call = driver->last();
    CHECK(call.sql.find("INSERT OR REPLACE INTO ip_ban_ranges")
          != std::string::npos);
    REQUIRE(call.params.size() == 6);
    CHECK(as_str(call.params[0]) == "10.0.0.0");
    CHECK(as_int(call.params[1]) == 8);
    CHECK(as_str(call.params[2]) == "lan block");
    CHECK(is_null_param(call.params[5]));
}

TEST_CASE("SqlIpBanRepository::is_banned true on an exact-host hit",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};
    // First query (exact) returns a row → banned.
    driver->push_result_set({FakeRow{std::vector<Cell>{std::int64_t{1}}}});

    auto r = repo.is_banned(ip("203.0.113.7"));
    REQUIRE(r.has_value());
    CHECK(r.value());
    CHECK(driver->calls[0].sql.find("FROM ip_bans WHERE ip = ?")
          != std::string::npos);
}

TEST_CASE("SqlIpBanRepository::is_banned matches a CIDR range in process",
          "[infra][persistence][ip_ban]") {
    SECTION("address inside the range is banned") {
        auto driver = make_driver();
        SqlIpBanRepository repo{driver};
        driver->push_result_set({});  // no exact hit
        driver->push_result_set({
            FakeRow{std::vector<Cell>{std::string{"10.0.0.0"}, std::int64_t{8}}}});

        auto r = repo.is_banned(ip("10.5.5.5"));
        REQUIRE(r.has_value());
        CHECK(r.value());
        CHECK(driver->calls[1].sql.find("FROM ip_ban_ranges")
              != std::string::npos);
    }
    SECTION("address outside the range is not banned") {
        auto driver = make_driver();
        SqlIpBanRepository repo{driver};
        driver->push_result_set({});  // no exact hit
        driver->push_result_set({
            FakeRow{std::vector<Cell>{std::string{"10.0.0.0"}, std::int64_t{8}}}});

        auto r = repo.is_banned(ip("11.5.5.5"));
        REQUIRE(r.has_value());
        CHECK_FALSE(r.value());
    }
}

TEST_CASE("SqlIpBanRepository::remove_ban / remove_range_ban bind their keys",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};

    REQUIRE(repo.remove_ban(ip("203.0.113.7")).has_value());
    CHECK(driver->last().sql.find("DELETE FROM ip_bans WHERE ip = ?")
          != std::string::npos);
    CHECK(as_str(driver->last().params.at(0)) == "203.0.113.7");

    REQUIRE(repo.remove_range_ban(ip("10.0.0.0"), 8).has_value());
    CHECK(driver->last().sql.find(
              "DELETE FROM ip_ban_ranges WHERE network = ? AND prefix_bits = ?")
          != std::string::npos);
    CHECK(as_str(driver->last().params.at(0)) == "10.0.0.0");
    CHECK(as_int(driver->last().params.at(1)) == 8);
}

TEST_CASE("SqlIpBanRepository::for_each_entry maps rows",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};
    driver->push_result_set({
        entry_row("1.2.3.4", "r1", 10, 100, nullptr),
        entry_row("5.6.7.8", "r2", 11, 200, std::int64_t{999})});

    std::vector<std::string> ips;
    repo.for_each_entry([&](const IpBanEntry& e) {
        ips.push_back(e.ip.to_string());
        return true;
    });
    REQUIRE(ips.size() == 2);
    CHECK(ips[0] == "1.2.3.4");
    CHECK(ips[1] == "5.6.7.8");
}

TEST_CASE("SqlIpBanRepository::load_banlist rehydrates the exact entries",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};
    driver->push_result_set({
        entry_row("1.2.3.4", "r1", 10, 100, nullptr)});

    auto r = repo.load_banlist();
    REQUIRE(r.has_value());
    REQUIRE(r.value().entries().size() == 1);
    CHECK(r.value().entries()[0].ip.to_string() == "1.2.3.4");
    CHECK(r.value().entries()[0].reason == "r1");
}

TEST_CASE("SqlIpBanRepository::save_banlist replaces the exact table transactionally",
          "[infra][persistence][ip_ban]") {
    auto driver = make_driver();
    SqlIpBanRepository repo{driver};

    IpBanEntry a;
    a.ip = ip("1.1.1.1"); a.issuer = AccountId{1}; a.issued_at = epoch_plus(1);
    IpBanEntry b;
    b.ip = ip("2.2.2.2"); b.issuer = AccountId{2}; b.issued_at = epoch_plus(2);
    IpBanList list = IpBanList::rehydrate({a, b});

    REQUIRE(repo.save_banlist(list).has_value());

    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    REQUIRE(driver->calls.size() == 3);  // DELETE + two INSERTs
    CHECK(driver->calls[0].sql.find("DELETE FROM ip_bans") != std::string::npos);
    CHECK(driver->calls[1].sql.find("INSERT INTO ip_bans") != std::string::npos);
    CHECK(as_str(driver->calls[1].params.at(0)) == "1.1.1.1");
    CHECK(as_str(driver->calls[2].params.at(0)) == "2.2.2.2");
}
