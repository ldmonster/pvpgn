// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_mail_store.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryMailStore: InboxEmptyForUnknownAccount", "[infra][inmemory]") {
    InMemoryMailStore store;
    auto result = store.inbox("alice");
    REQUIRE(result.has_value());
    REQUIRE(result.value().empty());
}

TEST_CASE("InMemoryMailStore: SendAndReceiveMessage", "[infra][inmemory]") {
    InMemoryMailStore store;

    application::ports::MailMessage msg;
    msg.to      = "bob";
    msg.subject = "Hello";
    msg.body    = "World";

    auto send_result = store.send(std::move(msg));
    REQUIRE(send_result.has_value());

    auto inbox = store.inbox("bob");
    REQUIRE(inbox.has_value());
    REQUIRE(inbox.value().size() == 1);
    REQUIRE(inbox.value()[0].subject == "Hello");
    REQUIRE(inbox.value()[0].body    == "World");
    REQUIRE(inbox.value()[0].to      == "bob");
}

TEST_CASE("InMemoryMailStore: MultipleMessagesToSameRecipient", "[infra][inmemory]") {
    InMemoryMailStore store;

    for (int i = 0; i < 3; ++i) {
        application::ports::MailMessage msg;
        msg.to      = "carol";
        msg.subject = "msg" + std::to_string(i);
        msg.body    = "body" + std::to_string(i);
        REQUIRE(store.send(std::move(msg)).has_value());
    }

    auto inbox = store.inbox("carol");
    REQUIRE(inbox.has_value());
    REQUIRE(inbox.value().size() == 3);
}

TEST_CASE("InMemoryMailStore: InboxIsolatedPerRecipient", "[infra][inmemory]") {
    InMemoryMailStore store;

    application::ports::MailMessage m1;
    m1.to = "alice"; m1.subject = "for alice";
    REQUIRE(store.send(std::move(m1)).has_value());

    application::ports::MailMessage m2;
    m2.to = "bob"; m2.subject = "for bob";
    REQUIRE(store.send(std::move(m2)).has_value());

    auto alice_inbox = store.inbox("alice");
    REQUIRE(alice_inbox.has_value());
    REQUIRE(alice_inbox.value().size() == 1);
    REQUIRE(alice_inbox.value()[0].subject == "for alice");

    auto bob_inbox = store.inbox("bob");
    REQUIRE(bob_inbox.has_value());
    REQUIRE(bob_inbox.value().size() == 1);
    REQUIRE(bob_inbox.value()[0].subject == "for bob");
}

TEST_CASE("InMemoryMailStore: DeleteMessageByIndex", "[infra][inmemory]") {
    InMemoryMailStore store;

    application::ports::MailMessage m1;
    m1.to = "dave"; m1.subject = "first";
    REQUIRE(store.send(std::move(m1)).has_value());

    application::ports::MailMessage m2;
    m2.to = "dave"; m2.subject = "second";
    REQUIRE(store.send(std::move(m2)).has_value());

    auto del = store.delete_message("dave", 0);
    REQUIRE(del.has_value());

    auto inbox = store.inbox("dave");
    REQUIRE(inbox.has_value());
    REQUIRE(inbox.value().size() == 1);
    REQUIRE(inbox.value()[0].subject == "second");
}

TEST_CASE("InMemoryMailStore: DeleteMessageNotFoundAccount", "[infra][inmemory]") {
    InMemoryMailStore store;
    auto result = store.delete_message("nobody", 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryMailStore: DeleteMessageOutOfRangeIndex", "[infra][inmemory]") {
    InMemoryMailStore store;

    application::ports::MailMessage msg;
    msg.to = "eve"; msg.subject = "only";
    REQUIRE(store.send(std::move(msg)).has_value());

    auto result = store.delete_message("eve", 5);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

}  // namespace pvpgn::infra::inmemory
