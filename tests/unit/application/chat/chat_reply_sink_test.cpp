// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit-level coverage for the `IChatReplySink` port and
// its verdict-to-reason mapping. The bridge wiring is exercised via
// the parity tests when the legacy bnetd library is in
// configure; here we focus on the application-pure surface.

#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/chat/chat_reply_sink.hpp"

using pvpgn::application::chat::IChatReplySink;
using pvpgn::application::chat::NullChatReplySink;
using pvpgn::application::chat::WhisperReplyContext;
using pvpgn::application::chat::WhisperReplyReason;
using pvpgn::application::chat::WhisperVerdict;
using pvpgn::application::chat::verdict_to_reply_reason;

namespace {

struct RecordingSink final : IChatReplySink {
    struct Record {
        WhisperReplyReason reason;
        std::string sender_name;
        std::string target_name;
    };
    std::vector<Record> records;
    bool return_value = true;

    bool emit_whisper_reply(
        WhisperReplyReason reason,
        const WhisperReplyContext& ctx) noexcept override {
        records.push_back(Record{
            reason,
            std::string{ctx.sender_name},
            std::string{ctx.target_name},
        });
        return return_value;
    }
};

}  // namespace

TEST_CASE("chat_reply_sink: NullChatReplySink always returns false",
          "[application][chat][reply_sink]") {
    NullChatReplySink sink;
    WhisperReplyContext ctx{"alice", "STAR", "enUS", "bob"};
    REQUIRE_FALSE(sink.emit_whisper_reply(
        WhisperReplyReason::TargetOffline, ctx));
}

TEST_CASE("chat_reply_sink: verdict_to_reply_reason maps each "
          "rejection arm",
          "[application][chat][reply_sink][mapping]") {
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::NoTarget)
            == WhisperReplyReason::NoTarget);
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::EmptyBody)
            == WhisperReplyReason::EmptyBody);
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::SelfWhisper)
            == WhisperReplyReason::SelfWhisper);
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::TargetOffline)
            == WhisperReplyReason::TargetOffline);
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::TargetDnd)
            == WhisperReplyReason::TargetDnd);
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::IgnoredBySender)
            == WhisperReplyReason::IgnoredBySender);
    REQUIRE(verdict_to_reply_reason(WhisperVerdict::IgnoredByTarget)
            == WhisperReplyReason::IgnoredByTarget);
}

TEST_CASE("chat_reply_sink: RecordingSink captures reason + identity",
          "[application][chat][reply_sink]") {
    RecordingSink sink;
    WhisperReplyContext ctx{"alice", "STAR", "enUS", "bob"};
    REQUIRE(sink.emit_whisper_reply(
        WhisperReplyReason::TargetDnd, ctx));
    REQUIRE(sink.records.size() == 1);
    REQUIRE(sink.records[0].reason == WhisperReplyReason::TargetDnd);
    REQUIRE(sink.records[0].sender_name == "alice");
    REQUIRE(sink.records[0].target_name == "bob");
}

TEST_CASE("chat_reply_sink: sink that refuses to send returns false",
          "[application][chat][reply_sink]") {
    RecordingSink sink;
    sink.return_value = false;
    WhisperReplyContext ctx{"alice", "STAR", "enUS", "bob"};
    REQUIRE_FALSE(sink.emit_whisper_reply(
        WhisperReplyReason::IgnoredByTarget, ctx));
    REQUIRE(sink.records.size() == 1);
}
