// SPDX-License-Identifier: GPL-2.0-or-later
// Observer-mode strangler: hands the byte-1 connection-class byte
// off to `pvpgn::application::init::dispatch_init_conn` and surfaces
// the v3 verdict to the legacy `handle_init_packet` for parity
// logging. No state mutation; legacy still applies the decision.
//
// Also hosts the dispatcher half of `pvpgn_v3_init_conn_apply`: the
// C ABI is here so it compiles in every v3 build; the legacy-aware
// handler is registered at runtime by
// `integration_legacy_bnetd_linked` via
// `install_legacy_init_conn_apply_handler`.

#include "integration/legacy_bnetd/init_conn_bridge.hpp"

#include <atomic>

#include "application/init/init_conn_dispatch.hpp"

namespace pvpgn::integration::legacy_bnetd {

namespace {

std::atomic<InitConnApplyHandler> g_apply_handler{nullptr};

}  // namespace

void set_init_conn_apply_handler(InitConnApplyHandler handler) noexcept {
    g_apply_handler.store(handler, std::memory_order_release);
}

InitConnApplyHandler get_init_conn_apply_handler() noexcept {
    return g_apply_handler.load(std::memory_order_acquire);
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_init_conn_decide(std::uint8_t cclass,
                                         std::uint8_t* out_decision) noexcept {
    using pvpgn::application::init::dispatch_init_conn;
    using pvpgn::application::init::InitConnRequest;
    using pvpgn::application::init::InitDecision;

    const auto resp = dispatch_init_conn(InitConnRequest{cclass});
    if (out_decision != nullptr) {
        out_decision[0] = static_cast<std::uint8_t>(resp.decision);
    }
    return (resp.decision == InitDecision::kRejected) ? 0 : 1;
}

extern "C" int pvpgn_v3_init_conn_apply(void* conn_ptr,
                                        std::uint8_t cclass) noexcept {
    using pvpgn::application::init::dispatch_init_conn;
    using pvpgn::application::init::InitConnRequest;
    using pvpgn::application::init::InitDecision;

    if (conn_ptr == nullptr) return 0;

    // Reject early without touching the handler at all: the handler
    // is only responsible for accepted decisions.
    const auto resp = dispatch_init_conn(InitConnRequest{cclass});
    if (resp.decision == InitDecision::kRejected) return 0;

    auto* h = pvpgn::integration::legacy_bnetd::get_init_conn_apply_handler();
    if (h == nullptr) return 0;
    return h(conn_ptr, cclass);
}
