// SPDX-License-Identifier: GPL-2.0-or-later
// Linked half of the v3 init-conn dispatch strangler.
//
// After R186.a the v3 driver path (`init_packet_dispatch_link.cpp`)
// no longer invokes `apply_via_legacy` -- the driver applies its
// own side effects via the `InitSideEffects` port. This handler
// stays alive ONLY to serve the legacy server.cpp packet pump,
// which still reaches `bnetd/handle_init.cpp` -> the C ABI
// `pvpgn_v3_init_conn_apply_ex` dispatcher -> this handler.
//
// R187.a: collapsed the per-class switch -- this function now
// delegates to the same `InitSideEffects` thunks the v3 driver
// uses, eliminating the duplicated state/class transition + log
// blob and ensuring both call paths produce identical observable
// behaviour. When `bnetd/handle_init.cpp` is finally rerouted to
// the driver, this whole TU goes away.

#include "integration/legacy_bnetd/init_conn_bridge.hpp"
#include "integration/legacy_bnetd/init_side_effects_link.hpp"

#include "application/bnet_packet_pump/init_side_effects.hpp"
#include "application/init/init_conn_dispatch.hpp"

namespace pvpgn::integration::legacy_bnetd {

namespace {

using ::pvpgn::application::bnet_packet_pump::ConnClass;
using ::pvpgn::application::bnet_packet_pump::InitSideEffects;
using ::pvpgn::application::init::dispatch_init_conn;
using ::pvpgn::application::init::InitConnRequest;
using ::pvpgn::application::init::InitDecision;

constexpr ConnClass decision_to_class(InitDecision d) noexcept {
    switch (d) {
        case InitDecision::kBnet:       return ConnClass::kBnet;
        case InitDecision::kFile:       return ConnClass::kFile;
        case InitDecision::kBot:        return ConnClass::kBot;
        case InitDecision::kTelnet:     return ConnClass::kTelnet;
        case InitDecision::kD2csBnetd:  return ConnClass::kD2csBnetd;
        default:                        return ConnClass::kNone;
    }
}

int apply_via_legacy(void* conn_ptr, std::uint8_t cclass) noexcept {
    if (conn_ptr == nullptr) return 0;

    const auto resp = dispatch_init_conn(InitConnRequest{cclass});
    const ConnClass cl = decision_to_class(resp.decision);
    if (cl == ConnClass::kNone) return 0;

    InitSideEffects const& sx = get_legacy_init_side_effects();
    if (sx.log_accept)     sx.log_accept(conn_ptr, cl);
    if (sx.set_connected)  sx.set_connected(conn_ptr);
    if (sx.set_class)      sx.set_class(conn_ptr, cl);

    if (resp.decision == InitDecision::kD2csBnetd && sx.apply_d2cs_init) {
        if (sx.apply_d2cs_init(conn_ptr) < 0) return -1;
    }
    return 1;
}

}  // namespace

void install_legacy_init_conn_apply_handler() noexcept {
    set_init_conn_apply_handler(&apply_via_legacy);
}

}  // namespace pvpgn::integration::legacy_bnetd
