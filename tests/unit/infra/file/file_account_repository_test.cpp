// SPDX-License-Identifier: GPL-2.0-or-later

/// @file file_account_repository_test.cpp
/// Catch2 unit tests for FileAccountRepository using a temporary directory.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/file/account_repository.hpp"

namespace pvpgn::infra::file {

namespace {

/// RAII helper: creates a unique temp directory and removes it on destruction.
class TempDir {
public:
    TempDir() {
        base_ = std::filesystem::temp_directory_path() /
                ("pvpgn_file_repo_test_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(base_);
    }

    ~TempDir() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
        // Ignore errors on cleanup
    }

    const std::filesystem::path& path() const noexcept { return base_; }
    std::string str() const { return base_.string(); }

private:
    std::filesystem::path base_;
};

/// Build a test account with the given id and username.
domain::identity::Account make_test_account(uint32_t id, std::string_view username) {
    auto name_result = domain::UserName::parse(username);
    REQUIRE(name_result.has_value());

    return domain::identity::Account::rehydrate(
        domain::AccountId{id},
        std::move(name_result.value()),
        domain::BNHash{},
        domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{},
        std::nullopt,
        false,
        false);
}

}  // namespace

TEST_CASE("FileAccountRepository: save_creates_plain_file", "[infra][file]") {
    TempDir tmp;
    FileAccountRepository repo(tmp.str());

    auto account = make_test_account(1, "PlainUser");
    auto save_result = repo.save(account);
    REQUIRE(save_result.has_value());

    // Verify the .plain file was created on disk
    const auto expected_path = tmp.path() / "PlainUser.plain";
    REQUIRE(std::filesystem::exists(expected_path));
}

TEST_CASE("FileAccountRepository: load_round_trip", "[infra][file]") {
    TempDir tmp;

    // Save via one repository instance
    {
        FileAccountRepository repo(tmp.str());
        auto account = make_test_account(10, "RoundTrip");
        REQUIRE(repo.save(account).has_value());
    }

    // Load via a fresh repository instance (reads from disk)
    FileAccountRepository repo2(tmp.str());
    auto find_result = repo2.find_by_name(domain::UserName::parse("RoundTrip").value());
    REQUIRE(find_result.has_value());
    REQUIRE(find_result.value().name().display() == "RoundTrip");
    REQUIRE(find_result.value().id().value() == 10u);
}

TEST_CASE("FileAccountRepository: find_by_name_returns_account", "[infra][file]") {
    TempDir tmp;
    FileAccountRepository repo(tmp.str());

    auto account = make_test_account(5, "FindMe");
    REQUIRE(repo.save(account).has_value());

    auto find_result = repo.find_by_name(domain::UserName::parse("FindMe").value());
    REQUIRE(find_result.has_value());
    REQUIRE(find_result.value().name().display() == "FindMe");
}

TEST_CASE("FileAccountRepository: find_nonexistent_returns_error", "[infra][file]") {
    TempDir tmp;
    FileAccountRepository repo(tmp.str());

    auto find_result = repo.find_by_name(domain::UserName::parse("NoSuchUser").value());
    REQUIRE_FALSE(find_result.has_value());
}

TEST_CASE("FileAccountRepository: remove_deletes_file", "[infra][file]") {
    TempDir tmp;
    FileAccountRepository repo(tmp.str());

    auto account = make_test_account(99, "DeleteMe");
    REQUIRE(repo.save(account).has_value());

    // Verify file exists
    const auto expected_path = tmp.path() / "DeleteMe.plain";
    REQUIRE(std::filesystem::exists(expected_path));

    // Remove by ID
    auto remove_result = repo.remove(domain::AccountId{99});
    REQUIRE(remove_result.has_value());

    // Verify file is gone
    REQUIRE_FALSE(std::filesystem::exists(expected_path));

    // Verify find returns error
    auto find_result = repo.find_by_name(domain::UserName::parse("DeleteMe").value());
    REQUIRE_FALSE(find_result.has_value());
}

TEST_CASE("FileAccountRepository: size_reflects_saved_accounts", "[infra][file]") {
    TempDir tmp;
    FileAccountRepository repo(tmp.str());

    REQUIRE(repo.size() == 0u);

    REQUIRE(repo.save(make_test_account(1, "Alpha")).has_value());
    REQUIRE(repo.size() == 1u);

    REQUIRE(repo.save(make_test_account(2, "Beta")).has_value());
    REQUIRE(repo.size() == 2u);
}

TEST_CASE("FileAccountRepository: find_by_id_returns_account", "[infra][file]") {
    TempDir tmp;
    FileAccountRepository repo(tmp.str());

    auto account = make_test_account(77, "ById");
    REQUIRE(repo.save(account).has_value());

    auto find_result = repo.find_by_id(domain::AccountId{77});
    REQUIRE(find_result.has_value());
    REQUIRE(find_result.value().id().value() == 77u);
    REQUIRE(find_result.value().name().display() == "ById");
}

}  // namespace pvpgn::infra::file
