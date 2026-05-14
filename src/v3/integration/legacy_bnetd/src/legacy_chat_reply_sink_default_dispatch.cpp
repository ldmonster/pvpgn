// SPDX-License-Identifier: GPL-2.0-or-later
//
// Default `DispatchFn` for `LegacyChatReplySink` -- compiled into
// the linked variant of the integration adapter library so it
// pulls in the bnetd `t_connection` lookup + `message_send_text`
// symbols. The non-linked sink TU is then free of legacy globals,
// which lets unit tests construct a sink directly and exercise the
// reasoning code without needing `bnetd_legacy` on the test link.

#include <string>
#include <string_view>

#include "integration/legacy_bnetd/legacy_chat_reply_sink.hpp"

#include "common/setup_before.h"
#include "bnetd/connection.h"
#include "bnetd/message.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

bool dispatch_via_legacy(std::string_view sender_name,
                        std::string_view text) noexcept {
    if (sender_name.empty()) return false;
    std::string name(sender_name);
    auto* sender_c =
        ::pvpgn::bnetd::connlist_find_connection_by_accountname(
            name.c_str());
    if (sender_c == nullptr) return false;
    std::string body(text);
    ::pvpgn::bnetd::message_send_text(sender_c,
                                      ::pvpgn::bnetd::message_type_info,
                                      sender_c,
                                      body.c_str());
    return true;
}

// Installed at static-init time so the composition root never has
// to call `set_dispatch` explicitly when the linked variant is in
// the binary.
struct AutoRegister {
    AutoRegister() noexcept {
        LegacyChatReplySink::set_dispatch(&dispatch_via_legacy);
    }
} g_auto_register;

}  // namespace

}  // namespace pvpgn::integration::legacy_bnetd
