// SPDX-License-Identifier: GPL-2.0-or-later
//
// R186.a: linked-half implementation of the
// `application::bnet_packet_pump::InitSideEffects` port. Each
// entry thunks to the corresponding legacy bnetd function.
// Compiled only inside `integration_legacy_bnetd_linked`.
//
// The driver itself never sees `t_connection*`; this TU is the
// ONE place where the opaque `void* conn` it carries is cast
// back to the concrete legacy type. That cast is the moral
// equivalent of the cast inside the old `apply_via_legacy`
// handler -- this file just hosts it behind a function-pointer
// table instead of a registered handler callback.

#include "application/bnet_packet_pump/init_side_effects.hpp"

#include "integration/legacy_bnetd/init_side_effects_link.hpp"

#include "common/setup_before.h"
#include "common/eventlog.h"
#include "bnetd/connection.h"
#include "bnetd/handle_d2cs.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

using ::pvpgn::application::bnet_packet_pump::ConnClass;

::pvpgn::bnetd::t_conn_class to_legacy(ConnClass c) noexcept
{
    using ::pvpgn::bnetd::conn_class_init;
    using ::pvpgn::bnetd::conn_class_bnet;
    using ::pvpgn::bnetd::conn_class_file;
    using ::pvpgn::bnetd::conn_class_bot;
    using ::pvpgn::bnetd::conn_class_telnet;
    using ::pvpgn::bnetd::conn_class_d2cs_bnetd;
    switch (c) {
        case ConnClass::kBnet:      return conn_class_bnet;
        case ConnClass::kFile:      return conn_class_file;
        case ConnClass::kBot:       return conn_class_bot;
        case ConnClass::kTelnet:    return conn_class_telnet;
        case ConnClass::kD2csBnetd: return conn_class_d2cs_bnetd;
        default:                    return conn_class_init;
    }
}

char const* kind_string(ConnClass c) noexcept
{
    switch (c) {
        case ConnClass::kBnet:      return "bnet";
        case ConnClass::kFile:      return "file download";
        case ConnClass::kBot:       return "chat bot";
        case ConnClass::kTelnet:    return "telnet";
        case ConnClass::kD2csBnetd: return "d2cs_bnetd";
        default:                    return "unknown";
    }
}

void sx_set_connected(void* conn) noexcept
{
    if (conn == nullptr) return;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(conn);
    ::pvpgn::bnetd::conn_set_state(c, ::pvpgn::bnetd::conn_state_connected);
}

void sx_set_class(void* conn, ConnClass cclass) noexcept
{
    if (conn == nullptr) return;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(conn);
    ::pvpgn::bnetd::conn_set_class(c, to_legacy(cclass));
}

int sx_apply_d2cs_init(void* conn) noexcept
{
    if (conn == nullptr) return -1;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(conn);
    if (::pvpgn::bnetd::handle_d2cs_init(c) < 0) {
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_info, __FUNCTION__,
            "[{}] failed to init d2cs connection",
            ::pvpgn::bnetd::conn_get_socket(c));
        return -1;
    }
    return 0;
}

void sx_log_accept(void* conn, ConnClass cclass) noexcept
{
    if (conn == nullptr) return;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(conn);
    ::pvpgn::eventlog(
        ::pvpgn::eventlog_level_info, __FUNCTION__,
        "[{}] client initiated {} connection (v3 driver)",
        ::pvpgn::bnetd::conn_get_socket(c),
        kind_string(cclass));
}

}  // namespace

::pvpgn::application::bnet_packet_pump::InitSideEffects const&
get_legacy_init_side_effects() noexcept
{
    static ::pvpgn::application::bnet_packet_pump::InitSideEffects const table{
        &sx_set_connected,
        &sx_set_class,
        &sx_apply_d2cs_init,
        &sx_log_accept,
    };
    return table;
}

}  // namespace pvpgn::integration::legacy_bnetd
