// SPDX-License-Identifier: GPL-2.0-or-later
// Linked half of the v3 init-conn dispatch strangler. Owns the
// `t_connection` state-machine transitions that the legacy
// `handle_init_packet` switch used to perform inline, including
// the D2CS_BNETD realmlist allow-list check and `handle_d2cs_init`
// call.
//
// Compiled only inside `integration_legacy_bnetd_linked` (requires
// the legacy `bnetd_legacy` library to be present in the same
// configure).

#include "integration/legacy_bnetd/init_conn_bridge.hpp"

#include "application/init/init_conn_dispatch.hpp"

#include "common/setup_before.h"
#include "common/eventlog.h"
#include "common/addr.h"
#include "bnetd/connection.h"
#include "bnetd/realm.h"
#include "bnetd/handle_d2cs.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

int apply_via_legacy(void* conn_ptr, std::uint8_t cclass) noexcept {
    using ::pvpgn::bnetd::t_connection;
    using ::pvpgn::application::init::dispatch_init_conn;
    using ::pvpgn::application::init::InitConnRequest;
    using ::pvpgn::application::init::InitDecision;

    if (conn_ptr == nullptr) return 0;

    auto* c = static_cast<t_connection*>(conn_ptr);

    const auto resp = dispatch_init_conn(InitConnRequest{cclass});

    switch (resp.decision) {
    case InitDecision::kBnet:
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_info, __FUNCTION__,
            "[{}] client initiated bnet connection (v3)",
            ::pvpgn::bnetd::conn_get_socket(c));
        ::pvpgn::bnetd::conn_set_state(
            c, ::pvpgn::bnetd::conn_state_connected);
        ::pvpgn::bnetd::conn_set_class(
            c, ::pvpgn::bnetd::conn_class_bnet);
        return 1;

    case InitDecision::kFile:
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_info, __FUNCTION__,
            "[{}] client initiated file download connection (v3)",
            ::pvpgn::bnetd::conn_get_socket(c));
        ::pvpgn::bnetd::conn_set_state(
            c, ::pvpgn::bnetd::conn_state_connected);
        ::pvpgn::bnetd::conn_set_class(
            c, ::pvpgn::bnetd::conn_class_file);
        return 1;

    case InitDecision::kBot:
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_info, __FUNCTION__,
            "[{}] client initiated chat bot connection (v3)",
            ::pvpgn::bnetd::conn_get_socket(c));
        ::pvpgn::bnetd::conn_set_state(
            c, ::pvpgn::bnetd::conn_state_connected);
        ::pvpgn::bnetd::conn_set_class(
            c, ::pvpgn::bnetd::conn_class_bot);
        return 1;

    case InitDecision::kTelnet:
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_info, __FUNCTION__,
            "[{}] client initiated telnet connection (v3)",
            ::pvpgn::bnetd::conn_get_socket(c));
        ::pvpgn::bnetd::conn_set_state(
            c, ::pvpgn::bnetd::conn_state_connected);
        ::pvpgn::bnetd::conn_set_class(
            c, ::pvpgn::bnetd::conn_class_telnet);
        return 1;

    case InitDecision::kD2csBnetd:
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_info, __FUNCTION__,
            "[{}] client initiated d2cs_bnetd connection (v3)",
            ::pvpgn::bnetd::conn_get_socket(c));
        if (::pvpgn::bnetd::realmlist_find_realm_by_ip(
                ::pvpgn::bnetd::conn_get_addr(c)) == nullptr) {
            ::pvpgn::eventlog(
                ::pvpgn::eventlog_level_info, __FUNCTION__,
                "[{}] d2cs connection from unknown ip address {}",
                ::pvpgn::bnetd::conn_get_socket(c),
                ::pvpgn::addr_num_to_addr_str(
                    ::pvpgn::bnetd::conn_get_addr(c),
                    ::pvpgn::bnetd::conn_get_port(c)));
            return -1;
        }
        ::pvpgn::bnetd::conn_set_state(
            c, ::pvpgn::bnetd::conn_state_connected);
        ::pvpgn::bnetd::conn_set_class(
            c, ::pvpgn::bnetd::conn_class_d2cs_bnetd);
        if (::pvpgn::bnetd::handle_d2cs_init(c) < 0) {
            ::pvpgn::eventlog(
                ::pvpgn::eventlog_level_info, __FUNCTION__,
                "[{}] failed to init d2cs connection",
                ::pvpgn::bnetd::conn_get_socket(c));
            return -1;
        }
        return 1;

    case InitDecision::kRejected:
    default:
        // Never reached when the C ABI rejects before invoking us;
        // returning 0 here keeps the dispatcher's "decline" semantic
        // even if some future caller wires us up directly.
        return 0;
    }
}

}  // namespace

void install_legacy_init_conn_apply_handler() noexcept {
    set_init_conn_apply_handler(&apply_via_legacy);
}

}  // namespace pvpgn::integration::legacy_bnetd
