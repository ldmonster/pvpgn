// SPDX-License-Identifier: GPL-2.0-or-later

#include "integration/legacy_bnetd/legacy_message_sink.hpp"

#include <string>
#include <string_view>

#include "common/setup_before.h"
#include "connection.h"
#include "message.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

void LegacyMessageSink::send(void* connection,
                             Severity severity,
                             std::string_view text) const
{
    if (connection == nullptr) return;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(connection);
    const auto mt = (severity == Severity::Error)
                        ? ::pvpgn::bnetd::message_type_error
                        : ::pvpgn::bnetd::message_type_info;
    const std::string buffer{text};  // need NUL-terminated for legacy API
    ::pvpgn::bnetd::message_send_text(c, mt, c, buffer.c_str());
}

}  // namespace pvpgn::integration::legacy_bnetd
