// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for FileHelpResponder using fake ports.

#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/admin_commands/file_help_responder.hpp"
#include "application/admin_commands/help_command_permissions.hpp"
#include "application/admin_commands/help_corpus.hpp"
#include "application/admin_commands/help_corpus_parser.hpp"
#include "application/admin_commands/help_corpus_provider.hpp"
#include "application/admin_commands/message_sink.hpp"

namespace ac = pvpgn::application::admin_commands;

namespace {

struct CapturedMessage {
    ac::IMessageSink::Severity severity;
    std::string                text;
};

class CapturingSink final : public ac::IMessageSink {
public:
    mutable std::vector<CapturedMessage> messages;
    void send(void*, Severity sev, std::string_view text) const override {
        messages.push_back({sev, std::string{text}});
    }
};

class AllowAll final : public ac::IHelpCommandPermissions {
public:
    bool is_visible(void*, std::string_view) const override { return true; }
};

class DenyAll final : public ac::IHelpCommandPermissions {
public:
    bool is_visible(void*, std::string_view) const override { return false; }
};

class DenyList final : public ac::IHelpCommandPermissions {
public:
    std::vector<std::string> denied;
    bool is_visible(void*, std::string_view name) const override {
        for (const auto& d : denied) {
            if (d == name) return false;
        }
        return true;
    }
};

class FixedCorpusProvider final : public ac::IHelpCorpusProvider {
public:
    explicit FixedCorpusProvider(const ac::HelpCorpus* c) : corpus_(c) {}
    const ac::HelpCorpus* for_connection(void*) const override { return corpus_; }
private:
    const ac::HelpCorpus* corpus_;
};

ac::HelpCorpus make_corpus(std::string_view text) {
    std::stringstream ss{std::string{text}};
    auto r = ac::parse_help_corpus(ss);
    REQUIRE(r);
    return std::move(r).value();
}

// Sentinel "connection" pointer. The fakes never dereference it.
void* const kFakeConn = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xABCD));

}  // namespace

TEST_CASE("FileHelpResponder: list mode emits header and one line per allowed entry",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus(
        "%help ?\nshow help\n"
        "%who w\nlist users\n");
    FixedCorpusProvider corpora{&corpus};
    AllowAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE(responder.respond(kFakeConn, "/help"));

    REQUIRE(sink.messages.size() == 3);
    REQUIRE(sink.messages[0].severity == ac::IMessageSink::Severity::Info);
    REQUIRE(sink.messages[0].text == "Chat commands : ");
    REQUIRE(sink.messages[1].text == " /help /?");
    REQUIRE(sink.messages[2].text == " /who /w");
}

TEST_CASE("FileHelpResponder: list mode filters entries by permission",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus(
        "%help ?\nbody\n"
        "%kick\nban hammer\n");
    FixedCorpusProvider corpora{&corpus};
    DenyList perms; perms.denied.push_back("/kick");
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE(responder.respond(kFakeConn, "/help"));

    REQUIRE(sink.messages.size() == 2);
    REQUIRE(sink.messages[0].text == "Chat commands : ");
    REQUIRE(sink.messages[1].text == " /help /?");
}

TEST_CASE("FileHelpResponder: describe mode emits each body line",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus(
        "%help\n"
        "first body line\n"
        "second body line\n");
    FixedCorpusProvider corpora{&corpus};
    AllowAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE(responder.respond(kFakeConn, "/? help"));

    REQUIRE(sink.messages.size() == 2);
    REQUIRE(sink.messages[0].text == "first body line");
    REQUIRE(sink.messages[1].text == "second body line");
}

TEST_CASE("FileHelpResponder: describe mode accepts arg with or without slash",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus("%help\nbody\n");
    FixedCorpusProvider corpora{&corpus};
    AllowAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE(responder.respond(kFakeConn, "/help /help"));
    REQUIRE(sink.messages.size() == 1);
    sink.messages.clear();

    REQUIRE(responder.respond(kFakeConn, "/help help"));
    REQUIRE(sink.messages.size() == 1);
}

TEST_CASE("FileHelpResponder: describe mode body line starting with / is Error severity",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus(
        "%help\n"
        "this is an info line\n"
        "/help see also\n");
    FixedCorpusProvider corpora{&corpus};
    AllowAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE(responder.respond(kFakeConn, "/help help"));

    REQUIRE(sink.messages.size() == 2);
    REQUIRE(sink.messages[0].severity == ac::IMessageSink::Severity::Info);
    REQUIRE(sink.messages[1].severity == ac::IMessageSink::Severity::Error);
    REQUIRE(sink.messages[1].text == "/help see also");
}

TEST_CASE("FileHelpResponder: describe mode miss sends error and returns false",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus("%help\nbody\n");
    FixedCorpusProvider corpora{&corpus};
    AllowAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE_FALSE(responder.respond(kFakeConn, "/help nonexistent"));
    REQUIRE(sink.messages.size() == 1);
    REQUIRE(sink.messages[0].severity == ac::IMessageSink::Severity::Error);
    REQUIRE(sink.messages[0].text == "No help available for that command");
}

TEST_CASE("FileHelpResponder: null corpus reports administrator error and returns false",
          "[file_help_responder]")
{
    FixedCorpusProvider corpora{nullptr};
    AllowAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE_FALSE(responder.respond(kFakeConn, "/help"));
    REQUIRE(sink.messages.size() == 1);
    REQUIRE(sink.messages[0].severity == ac::IMessageSink::Severity::Error);
    REQUIRE(sink.messages[0].text.find("problem with the help file") != std::string::npos);
}

TEST_CASE("FileHelpResponder: deny-all in list mode yields header only",
          "[file_help_responder]")
{
    const ac::HelpCorpus corpus = make_corpus("%help\nbody\n");
    FixedCorpusProvider corpora{&corpus};
    DenyAll perms;
    CapturingSink sink;
    const ac::FileHelpResponder responder{corpora, perms, sink};

    REQUIRE(responder.respond(kFakeConn, "/help"));
    REQUIRE(sink.messages.size() == 1);
    REQUIRE(sink.messages[0].text == "Chat commands : ");
}
