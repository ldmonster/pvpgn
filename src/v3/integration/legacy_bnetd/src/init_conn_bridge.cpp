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
#include <string>

#include "application/init/init_conn_dispatch.hpp"
#include "core/logging.hpp"
#include "core/format.hpp"

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

namespace {

pvpgn::application::init::InitConnRequest
make_request(std::uint8_t cclass,
             unsigned int conn_count,
             unsigned int max_conns_per_ip,
             int d2cs_ip_allowed) noexcept {
    pvpgn::application::init::InitConnRequest req{cclass};
    req.conn_count        = conn_count;
    req.max_conns_per_ip  = max_conns_per_ip;
    req.d2cs_ip_allowed   = d2cs_ip_allowed != 0;
    return req;
}

}  // namespace

extern "C" int pvpgn_v3_init_conn_decide_ex(std::uint8_t cclass,
                                            unsigned int conn_count,
                                            unsigned int max_conns_per_ip,
                                            int d2cs_ip_allowed,
                                            std::uint8_t* out_decision) noexcept {
    using pvpgn::application::init::dispatch_init_conn;
    using pvpgn::application::init::InitDecision;

    const auto resp = dispatch_init_conn(
        make_request(cclass, conn_count, max_conns_per_ip, d2cs_ip_allowed));
    if (out_decision != nullptr) {
        out_decision[0] = static_cast<std::uint8_t>(resp.decision);
    }
    return (resp.decision == InitDecision::kRejected ||
            resp.decision == InitDecision::kRateLimited ||
            resp.decision == InitDecision::kD2csIpDenied)
               ? 0
               : 1;
}

extern "C" int pvpgn_v3_init_conn_decide(std::uint8_t cclass,
                                         std::uint8_t* out_decision) noexcept {
    // Conservative defaults: rate-limit disabled, d2cs IP allowed.
    return pvpgn_v3_init_conn_decide_ex(cclass, 0u, 0u, 1, out_decision);
}

extern "C" int pvpgn_v3_init_conn_apply_ex(void* conn_ptr,
                                           std::uint8_t cclass,
                                           unsigned int conn_count,
                                           unsigned int max_conns_per_ip,
                                           int d2cs_ip_allowed) noexcept {
    using pvpgn::application::init::dispatch_init_conn;
    using pvpgn::application::init::InitDecision;

    if (conn_ptr == nullptr) return 0;

    // Reject early without touching the handler at all: the handler
    // is only responsible for accepted decisions.
    const auto resp = dispatch_init_conn(
        make_request(cclass, conn_count, max_conns_per_ip, d2cs_ip_allowed));
    if (resp.decision == InitDecision::kRejected)     return 0;
    // R169.a: rate-limit and realmlist denials are now authoritative
    // -- close the connection instead of falling through to legacy.
    if (resp.decision == InitDecision::kRateLimited ||
        resp.decision == InitDecision::kD2csIpDenied) return -1;

    auto* h = pvpgn::integration::legacy_bnetd::get_init_conn_apply_handler();
    if (h == nullptr) {
        // R168.a: under v3, the legacy switch in `handle_init.cpp` is
        // not expected to handle accepted connections. If we reach
        // this point with a NULL handler, startup wiring is wrong --
        // reject the connection and log once so the operator notices.
        static std::atomic_flag warned = ATOMIC_FLAG_INIT;
        if (!warned.test_and_set(std::memory_order_acq_rel)) {
            std::string msg =
                "pvpgn_v3_init_conn_apply: no handler installed; "
                "rejecting accepted cclass ";
            msg += std::to_string(static_cast<unsigned int>(cclass));
            msg += " (startup wiring bug?)";
            pvpgn::core::log_msg(pvpgn::core::LogLevel::Error,
                                 "v3.init_conn", msg);
        }
        return -1;
    }
    return h(conn_ptr, cclass);
}

extern "C" int pvpgn_v3_init_conn_apply(void* conn_ptr,
                                        std::uint8_t cclass) noexcept {
    return pvpgn_v3_init_conn_apply_ex(conn_ptr, cclass, 0u, 0u, 1);
}
