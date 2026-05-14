// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler-parity test for the v3 chat-reply sink (Batch 25b).
//
// We verify that the *strings* the v3 `LegacyChatReplySink` would
// emit, after `seed_default_strings()` + `format()`, exactly match
// the user-visible text the legacy whisper-rejection path produces.
//
// We DO NOT link `integration_legacy_bnetd_linked` (that would drag
// in all of `bnetd_legacy`). Instead we use the `inline`
// `seed_default_strings()` and the pure `MapStringTable::format()`
// directly. The `RecordingSink` exercises the same
// `IChatReplySink` contract the production sink implements, so any
// future refactor that breaks the contract surface area trips this
// test.

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/chat/chat_reply_sink.hpp"
#include "application/i18n/string_table.hpp"
#include "integration/legacy_bnetd/legacy_chat_reply_sink.hpp"

namespace pac = pvpgn::application::chat;
namespace plb = pvpgn::integration::legacy_bnetd;
namespace pai = pvpgn::application::i18n;

namespace {

// Test-only sink that captures everything the bridge would dispatch.
struct RecordingSink final : pac::IChatReplySink {
    struct Rec {
        pac::WhisperReplyReason reason;
        std::string sender;
        std::string target;
        std::string locale;
    };
    std::vector<Rec> records;

    bool emit_whisper_reply(
        pac::WhisperReplyReason reason,
        const pac::WhisperReplyContext& ctx) noexcept override {
        records.push_back(
            Rec{reason, std::string{ctx.sender_name},
                std::string{ctx.target_name},
                std::string{ctx.sender_locale}});
        return true;
    }
};

// Resolve key + format using the same default-locale table the
// composition root seeds. Mirrors `LegacyChatReplySink::emit_whisper_reply`
// without depending on legacy globals.
std::string render(const pai::MapStringTable& tbl,
                   const char* key,
                   std::string_view target,
                   std::string_view sender,
                   std::string_view locale = {}) {
    std::array<std::string_view, 2> args{target, sender};
    return tbl.format(std::string_view{key}, locale,
                      std::span<const std::string_view>{args});
}

}  // namespace

TEST_CASE("legacy_chat_reply_sink: seed_default_strings populates "
          "all whisper-rejection keys",
          "[integration][chat][parity]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);

    REQUIRE(render(tbl, "whisper.reply.no_target", "x", "y")
            == "That user is not logged on.");
    REQUIRE(render(tbl, "whisper.reply.empty_body", "x", "y")
            == "You must enter the message you wish to whisper.");
    REQUIRE(render(tbl, "whisper.reply.self_whisper", "x", "y")
            == "What, are you talking to yourself?");
    REQUIRE(render(tbl, "whisper.reply.target_offline", "x", "y")
            == "That user is not logged on.");
}

TEST_CASE("legacy_chat_reply_sink: target_dnd interpolates target",
          "[integration][chat][parity]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    REQUIRE(render(tbl, "whisper.reply.target_dnd", "alice", "bob")
            == "alice is unavailable (Do Not Disturb).");
}

TEST_CASE("legacy_chat_reply_sink: ignored_by_sender names the target",
          "[integration][chat][parity]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    REQUIRE(render(tbl, "whisper.reply.ignored_by_sender", "carol", "dave")
            == "You are ignoring carol.");
}

TEST_CASE("legacy_chat_reply_sink: ignored_by_target stays "
          "ambiguous (does not leak the target name)",
          "[integration][chat][parity]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    REQUIRE(render(tbl, "whisper.reply.ignored_by_target", "eve", "frank")
            == "That user is not accepting your messages.");
}

TEST_CASE("legacy_chat_reply_sink: operator override beats the default",
          "[integration][chat][parity]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    // Operator pins a locale-specific translation.
    tbl.set("ruRU", "whisper.reply.no_target",
            "Polzovatel ne v seti.");
    REQUIRE(render(tbl, "whisper.reply.no_target", "x", "y", "ruRU")
            == "Polzovatel ne v seti.");
    // Falls back to default when locale is unknown.
    REQUIRE(render(tbl, "whisper.reply.no_target", "x", "y", "deDE")
            == "That user is not logged on.");
}

TEST_CASE("chat_reply_sink contract: RecordingSink captures all "
          "WhisperReplyReason values",
          "[integration][chat][parity]") {
    RecordingSink sink;
    pac::WhisperReplyContext ctx{"sender", "WAR3", "enUS", "target"};
    for (auto r : {pac::WhisperReplyReason::NoTarget,
                   pac::WhisperReplyReason::EmptyBody,
                   pac::WhisperReplyReason::SelfWhisper,
                   pac::WhisperReplyReason::TargetOffline,
                   pac::WhisperReplyReason::TargetDnd,
                   pac::WhisperReplyReason::IgnoredBySender,
                   pac::WhisperReplyReason::IgnoredByTarget}) {
        sink.emit_whisper_reply(r, ctx);
    }
    REQUIRE(sink.records.size() == 7);
    REQUIRE(sink.records.front().reason
            == pac::WhisperReplyReason::NoTarget);
    REQUIRE(sink.records.back().reason
            == pac::WhisperReplyReason::IgnoredByTarget);
    REQUIRE(sink.records.front().sender == "sender");
    REQUIRE(sink.records.front().target == "target");
}

TEST_CASE("legacy_chat_reply_sink: seed_de_strings provides German fallback "
          "via language-only locale chain",
          "[integration][chat][parity][i18n]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink::seed_de_strings(tbl);
    // `deDE` falls back to the `de` language-only entry.
    REQUIRE(render(tbl, "whisper.reply.no_target", "x", "y", "deDE")
            == "Dieser Benutzer ist nicht angemeldet.");
    REQUIRE(render(tbl, "whisper.reply.target_dnd", "alice", "bob", "deDE")
            == "alice ist nicht verfuegbar (Nicht stoeren).");
    // English default still wins for `enUS`.
    REQUIRE(render(tbl, "whisper.reply.no_target", "x", "y", "enUS")
            == "That user is not logged on.");
}

TEST_CASE("legacy_chat_reply_sink: seed_ru_strings provides Russian fallback",
          "[integration][chat][parity][i18n]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink::seed_ru_strings(tbl);
    REQUIRE(render(tbl, "whisper.reply.ignored_by_sender",
                   "carol", "dave", "ruRU")
            == "Vy ignoriruete carol.");
    REQUIRE(render(tbl, "whisper.reply.self_whisper", "x", "y", "ru")
            == "Vy razgovarivaete sami s soboy?");
}
