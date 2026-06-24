// SPDX-License-Identifier: GPL-2.0-or-later
//
// Coverage for `application::auth::CreateAccount`. The base
// create_account_test.cpp covers the happy path, username-taken, IP-banned,
// and event publication. This file drives the two remaining error arms:
//   * is_banned() port failure -> CreateAccountError::Internal
//   * accounts_.save() failure -> CreateAccountError::PersistenceFailed

#include <functional>
#include <optional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/create_account.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"

namespace {

using namespace pvpgn;
using application::auth::CreateAccount;
using application::auth::CreateAccountError;
using application::auth::CreateAccountRequest;

domain::BNHash make_hash(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

// IP-ban repository whose is_banned() lookup deliberately fails.  Only
// is_banned() is exercised by CreateAccount; the remaining members are
// inert stubs that are never reached on this path.
class FailingIsBannedRepo final : public domain::moderation::IIpBanRepository {
public:
    core::Result<bool> is_banned(const domain::IpAddress&) const override {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "is_banned deliberately fails"});
    }
    core::Status<> add_ban(domain::moderation::IpBanEntry) override {
        return core::ok();
    }
    core::Status<> add_range_ban(domain::IpAddress, std::uint8_t, std::string,
                                 domain::AccountId, core::SystemTime,
                                 std::optional<core::SystemTime>) override {
        return core::ok();
    }
    core::Status<> remove_ban(const domain::IpAddress&) override {
        return core::ok();
    }
    core::Status<> remove_range_ban(domain::IpAddress, std::uint8_t) override {
        return core::ok();
    }
    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)>)
        const override {}
    core::Result<domain::moderation::IpBanList> load_banlist() const override {
        return core::Result<domain::moderation::IpBanList>(
            domain::moderation::IpBanList{});
    }
    core::Status<> save_banlist(
        const domain::moderation::IpBanList&) override {
        return core::ok();
    }
};

// Account repository whose save() always fails (reads delegate to a real
// in-memory store, though no account is ever seeded on this path).
class FailingSaveAccountRepo final
    : public domain::identity::IAccountRepository {
public:
    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        return inner_.find_by_id(id);
    }
    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        return inner_.find_by_name(name);
    }
    void forEach(
        std::function<bool(const domain::identity::Account&)> p) const override {
        inner_.forEach(std::move(p));
    }
    std::size_t size() const noexcept override { return inner_.size(); }
    core::Status<> save(const domain::identity::Account&) override {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "save deliberately fails"});
    }
    core::Status<> remove(domain::AccountId id) override {
        return inner_.remove(id);
    }

private:
    infra::inmemory::InMemoryAccountRepository inner_;
};

CreateAccountRequest make_request() {
    return CreateAccountRequest{
        make_name("Alice"),
        make_hash(0xAA),
        "",
        domain::Locale{},
        domain::ClientTag::parse("STAR").value(),
        domain::IpAddress{}};
}

}  // namespace

TEST_CASE("CreateAccount: is_banned port failure surfaces Internal",
          "[application][auth][create][errors]") {
    FailingIsBannedRepo                        ip_bans;
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemoryEventBus          bus;
    core::ManualClock                          clock{core::SystemTime{}};

    CreateAccount uc{accounts, ip_bans, bus, clock};
    auto r = uc.execute(make_request());

    REQUIRE_FALSE(r);
    CHECK(r.error() == CreateAccountError::Internal);
}

TEST_CASE("CreateAccount: account save failure surfaces PersistenceFailed",
          "[application][auth][create][errors]") {
    infra::inmemory::InMemoryIpBanRepository ip_bans;  // is_banned -> false
    FailingSaveAccountRepo                   accounts;
    infra::inmemory::InMemoryEventBus        bus;
    core::ManualClock                        clock{core::SystemTime{}};

    CreateAccount uc{accounts, ip_bans, bus, clock};
    auto r = uc.execute(make_request());

    REQUIRE_FALSE(r);
    CHECK(r.error() == CreateAccountError::PersistenceFailed);
}
