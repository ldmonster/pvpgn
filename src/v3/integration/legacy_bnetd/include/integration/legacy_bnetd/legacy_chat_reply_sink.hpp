// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_chat_reply_sink.hpp
/// Production adapter for `application::chat::IChatReplySink`
/// (Batch 25a). When installed it owns the user-visible whisper
/// rejection text and queues it onto the legacy
/// `message_send_text(c, message_type_info, c, ...)` path, mirroring
/// the strings the legacy `localize()` table emits today.
///
/// The adapter is opt-in: bnetd composition root only installs it
/// when the environment variable `PVPGN_V3_CHAT_REPLIES=1` is set,
/// so production behaviour is byte-identical to legacy unless the
/// operator opts in. The strings come from an injected
/// `application::i18n::IStringTable` so future translation work can
/// hot-swap the table without touching the adapter.

#include <string>
#include <string_view>

#include "application/chat/chat_reply_sink.hpp"
#include "application/i18n/string_table.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// `IStringTable`-driven adapter. Resolves a key per
/// `WhisperReplyReason` and dispatches via the legacy
/// `message_send_text` API. The constructor takes a string-table
/// reference (so the same table can power other future v3 replies)
/// and the lookup keys are stable:
///   * "whisper.reply.no_target"
///   * "whisper.reply.empty_body"
///   * "whisper.reply.self_whisper"
///   * "whisper.reply.target_offline"
///   * "whisper.reply.target_dnd"
///   * "whisper.reply.ignored_by_sender"
///   * "whisper.reply.ignored_by_target"
class LegacyChatReplySink final
    : public application::chat::IChatReplySink {
public:
    explicit LegacyChatReplySink(
        const application::i18n::IStringTable& tbl) noexcept
        : tbl_(tbl) {}

    /// Populate `tbl` with a canonical English fallback set
    /// (default-locale entries) matching the legacy reply text.
    /// Call this once at composition-root time before installing
    /// the sink. Operators can override individual entries by
    /// `tbl.set("xxYY", key, "translated")` afterwards.
    static inline void seed_default_strings(
        application::i18n::MapStringTable& tbl) {
        // Canonical English fallback. `{0}` -> target name,
        // `{1}` -> sender name. Kept in the header (`inline`) so
        // pure unit tests can exercise the strings without linking
        // the bnetd-bound .cpp.
        tbl.set("", "whisper.reply.no_target",
                "That user is not logged on.");
        tbl.set("", "whisper.reply.empty_body",
                "You must enter the message you wish to whisper.");
        tbl.set("", "whisper.reply.self_whisper",
                "What, are you talking to yourself?");
        tbl.set("", "whisper.reply.target_offline",
                "That user is not logged on.");
        tbl.set("", "whisper.reply.target_dnd",
                "{0} is unavailable (Do Not Disturb).");
        tbl.set("", "whisper.reply.ignored_by_sender",
                "You are ignoring {0}.");
        tbl.set("", "whisper.reply.ignored_by_target",
                "That user is not accepting your messages.");
    }

    /// Optional German translations (`deDE` / `de`).
    /// Operators opt in by calling this *after* `seed_default_strings()`.
    /// Strings reviewed against the legacy `i18n/bnetd_deDE.lua` ad-hoc
    /// table; if a translation is unset or empty here, the default
    /// English entry is used via the locale-fallback chain.
    static inline void seed_de_strings(
        application::i18n::MapStringTable& tbl) {
        tbl.set("de", "whisper.reply.no_target",
                "Dieser Benutzer ist nicht angemeldet.");
        tbl.set("de", "whisper.reply.empty_body",
                "Sie muessen die Fluester-Nachricht eingeben.");
        tbl.set("de", "whisper.reply.self_whisper",
                "Reden Sie mit sich selbst?");
        tbl.set("de", "whisper.reply.target_offline",
                "Dieser Benutzer ist nicht angemeldet.");
        tbl.set("de", "whisper.reply.target_dnd",
                "{0} ist nicht verfuegbar (Nicht stoeren).");
        tbl.set("de", "whisper.reply.ignored_by_sender",
                "Sie ignorieren {0}.");
        tbl.set("de", "whisper.reply.ignored_by_target",
                "Dieser Benutzer akzeptiert Ihre Nachrichten nicht.");
    }

    /// Optional Russian translations (`ruRU` / `ru`). Same caveats
    /// as `seed_de_strings`. ASCII-only romanisation -- the v3 string
    /// table is byte-oriented and the legacy whisper transport does
    /// not encode UTF-8 reliably, so we play it safe.
    static inline void seed_ru_strings(
        application::i18n::MapStringTable& tbl) {
        tbl.set("ru", "whisper.reply.no_target",
                "Polzovatel ne v seti.");
        tbl.set("ru", "whisper.reply.empty_body",
                "Vvedite tekst soobshcheniya.");
        tbl.set("ru", "whisper.reply.self_whisper",
                "Vy razgovarivaete sami s soboy?");
        tbl.set("ru", "whisper.reply.target_offline",
                "Polzovatel ne v seti.");
        tbl.set("ru", "whisper.reply.target_dnd",
                "{0} sejchas nedostupen (Ne bespokoit).");
        tbl.set("ru", "whisper.reply.ignored_by_sender",
                "Vy ignoriruete {0}.");
        tbl.set("ru", "whisper.reply.ignored_by_target",
                "Etot polzovatel ne prinimaet vashi soobshcheniya.");
    }

    bool emit_whisper_reply(
        application::chat::WhisperReplyReason reason,
        const application::chat::WhisperReplyContext& ctx) noexcept override;

    /// Hook the adapter uses to enqueue the resolved text on the
    /// sender's connection. Production: forwards to
    /// `message_send_text`. Tests: can be overridden via the
    /// `set_legacy_chat_reply_sink_dispatch` seam below to capture
    /// output without legacy globals.
    using DispatchFn = bool (*)(std::string_view sender_name,
                                std::string_view text) noexcept;

    /// Replace the dispatch function (test seam). The default
    /// dispatch resolves the sender by name via
    /// `connlist_find_connection_by_accountname` and calls
    /// `message_send_text(sender_c, message_type_info, sender_c,
    /// text)`. Pass `nullptr` to restore the default.
    static void set_dispatch(DispatchFn fn) noexcept;

private:
    const application::i18n::IStringTable& tbl_;
};

}  // namespace pvpgn::integration::legacy_bnetd
