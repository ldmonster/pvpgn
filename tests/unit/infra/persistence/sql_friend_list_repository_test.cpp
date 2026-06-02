// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_friend_list_repository_test.cpp -- Plan 07.
//
// Verifies the consolidated SqlFriendListRepository over the recording fake
// IDbDriver (no sqlite). Pins the ordered SELECT, the transactional full-replace
// save (DELETE + ordered INSERTs inside begin/commit), and the empty-list case.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/shared/ids.hpp"
#include "domain/social/friend_list.hpp"
#include "infra/persistence/friend_list_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::AccountId;
using pvpgn::domain::social::FriendList;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

FakeRow friend_row(std::int64_t friend_id) {
    return FakeRow{std::vector<Cell>{friend_id}};
}

}  // namespace

TEST_CASE("SqlFriendListRepository::find_by_owner reads the ordered list",
          "[infra][persistence][friend_list]") {
    auto driver = make_driver();
    SqlFriendListRepository repo{driver};
    driver->next_rows.push_back(friend_row(2));
    driver->next_rows.push_back(friend_row(3));
    driver->next_rows.push_back(friend_row(4));

    auto r = repo.find_by_owner(AccountId{1});
    REQUIRE(r.has_value());
    const FriendList& list = r.value();
    CHECK(list.owner().value() == 1u);
    REQUIRE(list.entries().size() == 3);
    CHECK(list.entries()[0].value() == 2u);
    CHECK(list.entries()[1].value() == 3u);
    CHECK(list.entries()[2].value() == 4u);

    const auto& call = driver->last();
    CHECK(call.sql.find("WHERE owner_id = ?") != std::string::npos);
    CHECK(call.sql.find("ORDER BY position") != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_int(call.params[0]) == 1);
}

TEST_CASE("SqlFriendListRepository::find_by_owner returns an empty list, not NotFound",
          "[infra][persistence][friend_list]") {
    auto driver = make_driver();  // no rows
    SqlFriendListRepository repo{driver};

    auto r = repo.find_by_owner(AccountId{99});
    REQUIRE(r.has_value());  // empty list is valid
    CHECK(r.value().owner().value() == 99u);
    CHECK(r.value().entries().empty());
}

TEST_CASE("SqlFriendListRepository::save replaces the list transactionally",
          "[infra][persistence][friend_list]") {
    auto driver = make_driver();
    SqlFriendListRepository repo{driver};

    FriendList list =
        FriendList::rehydrate(AccountId{1}, {AccountId{2}, AccountId{3}});

    REQUIRE(repo.save(list).has_value());

    // One transaction wrapping the writes.
    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    CHECK(driver->rollback_count == 0);

    // DELETE then two ordered INSERTs.
    REQUIRE(driver->calls.size() == 3);
    CHECK(driver->calls[0].sql.find("DELETE FROM friends WHERE owner_id = ?")
          != std::string::npos);
    CHECK(as_int(driver->calls[0].params.at(0)) == 1);

    CHECK(driver->calls[1].sql.find("INSERT INTO friends") != std::string::npos);
    REQUIRE(driver->calls[1].params.size() == 3);
    CHECK(as_int(driver->calls[1].params[0]) == 1);  // owner
    CHECK(as_int(driver->calls[1].params[1]) == 2);  // friend_id
    CHECK(as_int(driver->calls[1].params[2]) == 0);  // position

    REQUIRE(driver->calls[2].params.size() == 3);
    CHECK(as_int(driver->calls[2].params[1]) == 3);  // friend_id
    CHECK(as_int(driver->calls[2].params[2]) == 1);  // position
}

TEST_CASE("SqlFriendListRepository::save of an empty list clears the rows",
          "[infra][persistence][friend_list]") {
    auto driver = make_driver();
    SqlFriendListRepository repo{driver};

    FriendList empty{AccountId{5}};
    REQUIRE(repo.save(empty).has_value());

    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    // Only the DELETE, no INSERTs.
    REQUIRE(driver->calls.size() == 1);
    CHECK(driver->calls[0].sql.find("DELETE FROM friends") != std::string::npos);
}
