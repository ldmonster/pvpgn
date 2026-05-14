// SPDX-License-Identifier: GPL-2.0-or-later
//
// CHAT vertical bridge (Batch 18a + 19a + 20a): wires the legacy
// `_client_message` handler in `handle_bnet.cpp` to the pure
// `application/chat::classify_chat_command` +
// `application/chat::decide_whisper` seams.
//
// Consumption status (per arm):
//   * `EmptyAction`           -> return 1 (drop silently).
//   * `WhisperAction`         -> run through `decide_whisper`.
//       - `EmptyBody`         -> return 1 (drop /w with blank body).
//       - `SelfWhisper`       -> return 0 (legacy emits the
//                                 "what, are you talking to
//                                  yourself?" reply; v3 owns the
//                                  classification but defers the
//                                  outgoing packet until 21a).
//       - all other verdicts  -> return 0 (target-state arms need
//                                 the lookup adapter, deferred).
//   * `CommandAction` /
//     `ChannelMessageAction`  -> return 0 (legacy path runs).
//
// Sender name is read via the legacy `conn_get_username` so the
// `SelfWhisper` rule can fire for /w targeting the caller; the
// target-state snapshot is left empty for now (caller-controlled,
// no online/dnd/ignored lookup yet -- see 21a).

#include <cstddef>
#include <cstdint>
#include <atomic>  // unused after 30d (was for 27e warn-once latch); kept to avoid churn in unrelated TU history
#include <string>
#include <string_view>
#include <variant>

#include "application/chat/chat_command.hpp"
#include "application/chat/chat_reply_sink.hpp"
#include "application/chat/whisper_use_case.hpp"
#include "application/chat/whisper_target_lookup.hpp"
#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/legacy_whisper_target_lookup.hpp"

#include "common/setup_before.h"
#include "bnetd/connection.h"
#include "bnetd/i18n.h"
#include "common/tag.h"
#include "common/setup_after.h"

// Legacy bnetd API is nested in ``pvpgn::bnetd``. Bring it into the
// unqualified scope so this file's bare ``t_connection`` /
// ``conn_get_username`` references resolve.
using namespace pvpgn::bnetd;

namespace pac = pvpgn::application::chat;
namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

/// Process-global chat reply sink override (24a). Default
/// `nullptr` means "no v3 reply ownership; let legacy handle the
/// text". Production composition root will eventually install a
/// `LegacyChatReplySink` here once the i18n table is reachable.
pac::IChatReplySink* g_reply_sink = nullptr;

}  // namespace

namespace pvpgn::integration::legacy_bnetd {

void set_chat_reply_sink_override(
    pac::IChatReplySink* sink) noexcept {
    g_reply_sink = sink;
}

}  // namespace pvpgn::integration::legacy_bnetd

namespace {

const char* kind_name(const pac::ChatAction& a) {
    if (std::holds_alternative<pac::EmptyAction>(a))          return "Empty";
    if (std::holds_alternative<pac::WhisperAction>(a))        return "Whisper";
    if (std::holds_alternative<pac::CommandAction>(a))        return "Command";
    if (std::holds_alternative<pac::ChannelMessageAction>(a)) return "Channel";
    return "Unknown";
}

const char* verdict_name(pac::WhisperVerdict v) {
    switch (v) {
        case pac::WhisperVerdict::Delivered:        return "Delivered";
        case pac::WhisperVerdict::EmptyBody:        return "EmptyBody";
        case pac::WhisperVerdict::NoTarget:         return "NoTarget";
        case pac::WhisperVerdict::SelfWhisper:      return "SelfWhisper";
        case pac::WhisperVerdict::TargetOffline:    return "TargetOffline";
        case pac::WhisperVerdict::TargetDnd:        return "TargetDnd";
        case pac::WhisperVerdict::IgnoredBySender:  return "IgnoredBySender";
        case pac::WhisperVerdict::IgnoredByTarget:  return "IgnoredByTarget";
    }
    return "?";
}

}  // namespace

extern "C" int pvpgn_v3_chat_command_try(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr || body_size == 0) return 0;

    // The legacy `_client_message` extracts the chat text via
    // `packet_get_str_const` then forwards it as a NUL-terminated
    // C string. To match that behaviour the bridge treats `body` as
    // a length-bounded UTF-8 view; if a NUL appears earlier we trim
    // there.
    auto const* p   = static_cast<const char*>(body);
    std::size_t len = 0;
    while (len < body_size && p[len] != '\0') ++len;

    auto action = pac::classify_chat_command(std::string_view{p, len});

    {
        // 31d: structured log of the classifier outcome so log
        // pipelines can count chat-input shapes without parsing
        // free-form text. Keeping the level at Debug so production
        // emitters can drop these unless investigating.
        const std::string len_str = std::to_string(len);
        const pvpgn::core::ILogger::Field cls_fields[] = {
            {"kind", std::string_view{kind_name(action)}},
            {"len",  std::string_view{len_str}},
        };
        plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
            "v3_chat_command_bridge",
            "chat input classified",
            {cls_fields[0], cls_fields[1]});
    }

    if (std::holds_alternative<pac::EmptyAction>(action)) {
        return 1;
    }

    if (auto* w = std::get_if<pac::WhisperAction>(&action)) {
        auto* c = static_cast<t_connection*>(conn_ptr);
        char const* sender_c = conn_get_username(c);
        std::string_view sender = sender_c ? std::string_view{sender_c}
                                           : std::string_view{};

        pac::WhisperRequest req{
            sender,
            std::string_view{w->target},
            std::string_view{w->body},
            plb::LegacyWhisperTargetLookup{}.lookup(sender,
                                                    std::string_view{w->target})
        };
        auto verdict = pac::decide_whisper(req);

        {
            const pvpgn::core::ILogger::Field fields[] = {
                {"verdict", std::string_view{verdict_name(verdict)}},
                {"sender",  sender},
                {"target",  std::string_view{w->target}},
            };
            plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
                "v3_chat_command_bridge",
                "whisper verdict",
                {fields[0], fields[1], fields[2]});
        }

        if (verdict == pac::WhisperVerdict::EmptyBody) {
            return 1;
        }

        // 24a: if a reply sink is installed, ask it to emit the
        // user-visible rejection and claim the verdict
        // (return 1 = handled, legacy path skipped). The production
        // composition root leaves `g_reply_sink == nullptr` so the
        // legacy localize() / message_send_text() path still owns
        // the text for now; tests can swap in a recording sink to
        // assert the bridge fired the right reason.
        if (g_reply_sink != nullptr
            && verdict != pac::WhisperVerdict::Delivered) {
            // 27b: derive clienttag + locale from the legacy
            // connection so reply text can be locale-aware. The
            // `clienttag` is a 4-byte t_clienttag uint; the locale
            // is the *localized* gamelang (i.e. ruRU is preserved
            // for clients on Russian builds, and English clients
            // fall through to the empty-locale default).
            char ctag_buf[5] = {0, 0, 0, 0, 0};
            char lang_buf[5] = {0, 0, 0, 0, 0};
            if (auto t = ::pvpgn::bnetd::conn_get_clienttag(c)) {
                pvpgn::tag_uint_to_str(ctag_buf, t);
            }
            if (auto lg = ::pvpgn::bnetd::conn_get_gamelang_localized(c)) {
                pvpgn::tag_uint_to_str(lang_buf, lg);
            }
            pac::WhisperReplyContext rctx{
                sender,
                std::string_view{ctag_buf},
                std::string_view{lang_buf},
                std::string_view{w->target},
            };
            if (g_reply_sink->emit_whisper_reply(
                    pac::verdict_to_reply_reason(verdict), rctx)) {
                return 1;
            }
            // 30d: when the sink is installed but declines, claim
            // the message anyway and *log loud*. The legacy text
            // path is officially retired for whisper rejection
            // replies -- any rare miss here surfaces as a missing
            // client reply, which is preferable to a silent
            // divergence between the v3 string table and the
            // legacy hard-coded English text.
            //
            // Until telemetry confirms zero occurrences over a
            // reasonable production window we keep the log at
            // Error level (was one-shot Warn during 27e).
            plb::bridge_log(pvpgn::core::LogLevel::Error,
                            "v3_chat_command_bridge",
                            "reply sink declined whisper -- "
                            "client will receive NO reply "
                            "(legacy text path retired in 30d)");
            return 1;
        }

        // SelfWhisper + target-state verdicts: legacy path retains
        // ownership when no reply sink is installed.
        return 0;
    }

    // CommandAction / ChannelMessageAction: legacy.
    return 0;
}

