// SPDX-License-Identifier: GPL-2.0-or-later
//
// Integration-level fixture test for `LegacyChatReplySink` (Batch 26e).
//
// We exercise the production code path through the
// `LegacyChatReplySink::IChatReplySink` interface, with a custom
// `DispatchFn` installed via `set_dispatch(...)` to capture the
// `(sender_name, text)` pair the sink would otherwise hand to the
// legacy `message_send_text(...)` transport. This validates:
//
//   * the key-derivation switch for every `WhisperReplyReason`
//     variant,
//   * the (locale, args) -> string format pipeline end-to-end,
//   * the dispatch fallback when no transport is installed,
//   * the dispatch-failure -> bridge-fall-through path.
//
// The sink TU itself no longer references any `bnetd_legacy` symbol
// (the legacy dispatch helper lives in
// `legacy_chat_reply_sink_default_dispatch.cpp`), so this test
// links the unlinked `integration_legacy_bnetd` variant only.

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/chat/chat_reply_sink.hpp"
#include "application/i18n/string_table.hpp"
#include "integration/legacy_bnetd/legacy_chat_reply_sink.hpp"

namespace pac = pvpgn::application::chat;
namespace pai = pvpgn::application::i18n;
namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

struct CapturedCall {
    std::string sender;
    std::string text;
};

// Test seam: shared across the cases. Reset in each TEST_CASE.
std::vector<CapturedCall>* g_capture = nullptr;
bool g_return = true;

bool capture_dispatch(std::string_view sender,
                      std::string_view text) noexcept {
    if (g_capture != nullptr) {
        try {
            g_capture->push_back(CapturedCall{
                std::string{sender}, std::string{text}});
        } catch (...) {
        }
    }
    return g_return;
}

struct DispatchScope {
    DispatchScope(std::vector<CapturedCall>* cap, bool ret) {
        g_capture = cap;
        g_return  = ret;
        plb::LegacyChatReplySink::set_dispatch(&capture_dispatch);
    }
    ~DispatchScope() {
        plb::LegacyChatReplySink::set_dispatch(nullptr);
        g_capture = nullptr;
        g_return  = true;
    }
};

}  // namespace

TEST_CASE("LegacyChatReplySink: declines when no dispatch is installed",
          "[integration][chat][fixture]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink::set_dispatch(nullptr);  // explicit
    plb::LegacyChatReplySink sink(tbl);
    pac::WhisperReplyContext ctx{"alice", "WAR3", "enUS", "bob"};
    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::TargetOffline, ctx)
            == false);
}

TEST_CASE("LegacyChatReplySink: dispatches the default reply text",
          "[integration][chat][fixture]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink sink(tbl);

    std::vector<CapturedCall> captured;
    DispatchScope scope(&captured, /*ret=*/true);

    pac::WhisperReplyContext ctx{"alice", "WAR3", "enUS", "bob"};

    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::TargetOffline, ctx));
    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::TargetDnd, ctx));
    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::IgnoredByTarget, ctx));

    REQUIRE(captured.size() == 3);
    REQUIRE(captured[0].sender == "alice");
    REQUIRE(captured[0].text == "That user is not logged on.");
    REQUIRE(captured[1].text
            == "bob is unavailable (Do Not Disturb).");
    REQUIRE(captured[2].text
            == "That user is not accepting your messages.");
}

TEST_CASE("LegacyChatReplySink: honours locale fallback chain",
          "[integration][chat][fixture]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink::seed_ru_strings(tbl);
    plb::LegacyChatReplySink sink(tbl);

    std::vector<CapturedCall> captured;
    DispatchScope scope(&captured, /*ret=*/true);

    // `ruRU` -> language-only `ru` entry.
    pac::WhisperReplyContext ru_ctx{"alice", "WAR3", "ruRU", "bob"};
    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::TargetOffline, ru_ctx));
    REQUIRE(captured.back().text == "Polzovatel ne v seti.");

    // Unknown locale -> default English.
    pac::WhisperReplyContext fr_ctx{"alice", "WAR3", "frFR", "bob"};
    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::TargetOffline, fr_ctx));
    REQUIRE(captured.back().text == "That user is not logged on.");
}

TEST_CASE("LegacyChatReplySink: returns false when dispatch reports "
          "failure (bridge will fall back to legacy)",
          "[integration][chat][fixture]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink sink(tbl);

    std::vector<CapturedCall> captured;
    DispatchScope scope(&captured, /*ret=*/false);

    pac::WhisperReplyContext ctx{"alice", "WAR3", "enUS", "bob"};
    REQUIRE(sink.emit_whisper_reply(
                pac::WhisperReplyReason::NoTarget, ctx)
            == false);
    // Dispatch was still called (the sink does not pre-validate the
    // sender online state); the bridge interprets the false return
    // as "legacy continues".
    REQUIRE(captured.size() == 1);
}

TEST_CASE("LegacyChatReplySink: every WhisperReplyReason maps to a key "
          "that yields non-empty text",
          "[integration][chat][fixture]") {
    pai::MapStringTable tbl;
    plb::LegacyChatReplySink::seed_default_strings(tbl);
    plb::LegacyChatReplySink sink(tbl);

    std::vector<CapturedCall> captured;
    DispatchScope scope(&captured, /*ret=*/true);

    pac::WhisperReplyContext ctx{"alice", "WAR3", "enUS", "bob"};
    for (auto r : {pac::WhisperReplyReason::NoTarget,
                   pac::WhisperReplyReason::EmptyBody,
                   pac::WhisperReplyReason::SelfWhisper,
                   pac::WhisperReplyReason::TargetOffline,
                   pac::WhisperReplyReason::TargetDnd,
                   pac::WhisperReplyReason::IgnoredBySender,
                   pac::WhisperReplyReason::IgnoredByTarget}) {
        REQUIRE(sink.emit_whisper_reply(r, ctx));
    }
    REQUIRE(captured.size() == 7);
    for (const auto& c : captured) {
        REQUIRE(!c.text.empty());
    }
}
