// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::SendWhisper`. Exercises the use-case
// against inline fake port adapters.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/chat/send_whisper.hpp"
#include "application/ports/account_repository.hpp"
#include "application/ports/message_router.hpp"
#include "application/ports/session_registry.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace {

using namespace pvpgn;
using application::chat::SendWhisper;
using application::chat::SendWhisperCommand;
using application::chat::SendWhisperError;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

domain::identity::Account make_account(domain::AccountId id,
                                       std::string_view name) {
    auto uname = domain::UserName::parse(name);
    REQUIRE(uname);
    auto locale = domain::Locale::parse_or_default("enUS");
    return domain::identity::Account::rehydrate(
        id, uname.value(), domain::BNHash{}, locale,
        domain::identity::CommandGroupMask{}, std::nullopt, false);
}

// ---------------------------------------------------------------------------
// Fake IAccountRepository
// ---------------------------------------------------------------------------

class FakeAccountRepository final
    : public application::ports::IAccountRepository {
public:
    void add(domain::identity::Account acct) {
        accounts_.emplace(std::string{acct.name().display()}, std::move(acct));
    }

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override {
        auto it = accounts_.find(std::string{name});
        if (it == accounts_.end()) {
            return core::fail(
                core::Error{core::StatusCode::NotFound, "account not found"});
        }
        return it->second;
    }

    core::Result<domain::identity::Account, core::Error>
    find_by_id(uint32_t id) override {
        for (auto& [_, a] : accounts_) {
            if (a.id().value() == id) return a;
        }
        return core::fail(
            core::Error{core::StatusCode::NotFound, "account not found"});
    }

    core::Result<void, core::Error>
    save(const domain::identity::Account&) override {
        return {};
    }

    core::Result<void, core::Error>
    remove(std::string_view name) override {
        accounts_.erase(std::string{name});
        return {};
    }

    core::Result<bool, core::Error>
    exists(std::string_view name) override {
        return accounts_.count(std::string{name}) > 0;
    }

    core::Result<std::vector<domain::identity::Account>, core::Error>
    list_online() override {
        std::vector<domain::identity::Account> out;
        for (auto& [_, a] : accounts_) out.push_back(a);
        return out;
    }

    core::Result<uint32_t, core::Error> count() override {
        return static_cast<uint32_t>(accounts_.size());
    }

private:
    std::unordered_map<std::string, domain::identity::Account> accounts_;
};

// ---------------------------------------------------------------------------
// Fake ISessionRegistry
// ---------------------------------------------------------------------------

class FakeSessionRegistry final
    : public application::ports::ISessionRegistry {
public:
    void set_online(domain::AccountId account, domain::SessionId session) {
        account_to_session_[account.value()] = session;
        session_to_account_[session.value()] = account;
    }

    core::Status<>
    attach(domain::SessionId session, domain::AccountId account) override {
        account_to_session_[account.value()] = session;
        session_to_account_[session.value()] = account;
        return core::ok();
    }

    void detach(domain::SessionId session) override {
        auto it = session_to_account_.find(session.value());
        if (it != session_to_account_.end()) {
            account_to_session_.erase(it->second.value());
            session_to_account_.erase(it);
        }
    }

    std::optional<domain::SessionId>
    session_for(domain::AccountId account) const override {
        auto it = account_to_session_.find(account.value());
        if (it == account_to_session_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<domain::AccountId>
    account_for(domain::SessionId session) const override {
        auto it = session_to_account_.find(session.value());
        if (it == session_to_account_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<domain::SessionId> list() const override {
        std::vector<domain::SessionId> out;
        for (auto& [_, s] : account_to_session_) out.push_back(s);
        return out;
    }

private:
    std::unordered_map<uint32_t, domain::SessionId>  account_to_session_;
    std::unordered_map<uint64_t, domain::AccountId>  session_to_account_;
};

// ---------------------------------------------------------------------------
// Fake IMessageRouter
// ---------------------------------------------------------------------------

class FakeMessageRouter final : public application::ports::IMessageRouter {
public:
    int route_count = 0;

    core::Result<void, core::Error>
    send(domain::SessionId, std::span<const std::byte>) override {
        ++route_count;
        return {};
    }

    core::Result<void, core::Error>
    broadcast(std::span<const domain::SessionId>,
              std::span<const std::byte>) override {
        ++route_count;
        return {};
    }

    core::Result<void, core::Error>
    send_to_account(domain::AccountId,
                    std::span<const std::byte>) override {
        ++route_count;
        return {};
    }
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

struct Fixture {
    std::shared_ptr<FakeAccountRepository> accounts =
        std::make_shared<FakeAccountRepository>();
    std::shared_ptr<FakeSessionRegistry> sessions =
        std::make_shared<FakeSessionRegistry>();
    std::shared_ptr<FakeMessageRouter> router =
        std::make_shared<FakeMessageRouter>();

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::SessionId bob_session{100};

    void setup() {
        accounts->add(make_account(alice_id, "Alice"));
        accounts->add(make_account(bob_id, "Bob"));
        sessions->set_online(bob_id, bob_session);
    }

    SendWhisper make_use_case() {
        return SendWhisper{accounts, sessions, router};
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("SendWhisper: happy path — target online, message routed",
          "[application][chat][send_whisper]") {
    Fixture f;
    f.setup();

    auto uc = f.make_use_case();
    SendWhisperCommand cmd{
        .sender_id   = f.alice_id,
        .sender_name = "Alice",
        .target_name = "Bob",
        .message     = "Hello Bob!",
    };
    auto r = uc.execute(cmd);

    REQUIRE(r);
}

TEST_CASE("SendWhisper: target account not found returns NotFound",
          "[application][chat][send_whisper]") {
    Fixture f;
    f.setup();

    auto uc = f.make_use_case();
    SendWhisperCommand cmd{
        .sender_id   = f.alice_id,
        .sender_name = "Alice",
        .target_name = "NoSuchUser",
        .message     = "Hello?",
    };
    auto r = uc.execute(cmd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SendWhisperError::NotFound);
}

TEST_CASE("SendWhisper: target offline returns NotFound",
          "[application][chat][send_whisper]") {
    Fixture f;
    f.setup();
    // Alice exists but has no session (offline)
    auto uc = f.make_use_case();
    SendWhisperCommand cmd{
        .sender_id   = f.bob_id,
        .sender_name = "Bob",
        .target_name = "Alice",  // Alice is not online
        .message     = "Hey Alice",
    };
    auto r = uc.execute(cmd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SendWhisperError::NotFound);
}

TEST_CASE("SendWhisper: empty message returns InvalidArgument",
          "[application][chat][send_whisper]") {
    Fixture f;
    f.setup();

    auto uc = f.make_use_case();
    SendWhisperCommand cmd{
        .sender_id   = f.alice_id,
        .sender_name = "Alice",
        .target_name = "Bob",
        .message     = "",
    };
    auto r = uc.execute(cmd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SendWhisperError::InvalidArgument);
}
