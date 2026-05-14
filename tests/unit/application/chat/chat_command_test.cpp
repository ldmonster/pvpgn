// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/chat/chat_command.hpp"

using namespace pvpgn::application::chat;

namespace {

template <class T>
const T* as(const ChatAction& a) { return std::get_if<T>(&a); }

}  // namespace

TEST_CASE("chat: empty / whitespace -> Empty", "[application][chat]") {
    REQUIRE(as<EmptyAction>(classify_chat_command("")));
    REQUIRE(as<EmptyAction>(classify_chat_command("   ")));
    REQUIRE(as<EmptyAction>(classify_chat_command(" \t \r\n")));
}

TEST_CASE("chat: plain text -> ChannelMessage (trimmed)",
          "[application][chat]") {
    auto a = classify_chat_command("  hello world  ");
    auto* m = as<ChannelMessageAction>(a);
    REQUIRE(m != nullptr);
    REQUIRE(m->text == "hello world");
}

TEST_CASE("chat: '/help' -> Command{name='help', args=''}",
          "[application][chat]") {
    auto a = classify_chat_command("/help");
    auto* c = as<CommandAction>(a);
    REQUIRE(c != nullptr);
    REQUIRE(c->name == "help");
    REQUIRE(c->args.empty());
}

TEST_CASE("chat: '/HELP foo bar' lowercases the verb, keeps args",
          "[application][chat]") {
    auto a = classify_chat_command("/HELP foo bar");
    auto* c = as<CommandAction>(a);
    REQUIRE(c != nullptr);
    REQUIRE(c->name == "help");
    REQUIRE(c->args == "foo bar");
}

TEST_CASE("chat: '/w nick body words' -> Whisper{target='nick', body='body words'}",
          "[application][chat]") {
    auto a = classify_chat_command("/w nick body words");
    auto* w = as<WhisperAction>(a);
    REQUIRE(w != nullptr);
    REQUIRE(w->target == "nick");
    REQUIRE(w->body == "body words");
}

TEST_CASE("chat: '/whisper' '/msg' '/m' all parse as whisper",
          "[application][chat]") {
    for (auto* prefix : {"/whisper", "/msg", "/m"}) {
        auto a = classify_chat_command(std::string{prefix} + " bob hi");
        auto* w = as<WhisperAction>(a);
        REQUIRE(w != nullptr);
        REQUIRE(w->target == "bob");
        REQUIRE(w->body == "hi");
    }
}

TEST_CASE("chat: '/w' with no target degrades to Command{name='w'}",
          "[application][chat]") {
    auto a = classify_chat_command("/w");
    auto* c = as<CommandAction>(a);
    REQUIRE(c != nullptr);
    REQUIRE(c->name == "w");
    REQUIRE(c->args.empty());
}
