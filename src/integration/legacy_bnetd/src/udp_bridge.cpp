// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/udp_bridge.hpp"

#include <atomic>
#include <memory>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "infra/net/io_runtime.hpp"
#include "infra/net/udp_endpoint.hpp"
#include "integration/legacy_bnetd/legacy_udp_dispatcher.hpp"

// Reach into the legacy server for the reserved UDP fd list.
#include "common/setup_before.h"
#include "bnetd/server.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

class UdpBridgeImpl {
public:
    UdpBridgeImpl() = default;
    explicit UdpBridgeImpl(infra::net::IoRuntime& runtime) noexcept
        : external_runtime_(&runtime) {}

    ~UdpBridgeImpl() { stop(); }

    core::Result<std::size_t> start() {
        if (running_.exchange(true)) {
            return core::fail(core::Error{
                core::StatusCode::FailedPrecondition,
                "UdpBridge already installed"});
        }

        const auto fds = pvpgn::bnetd::server_get_bnet_udp_fds();
        if (fds.empty()) {
            running_ = false;
            return core::fail(core::Error{
                core::StatusCode::FailedPrecondition,
                "no reserved bnet UDP fds; did you forget "
                "server_set_skip_legacy_udp_fdwatch(true)?"});
        }

        // Use the externally provided runtime if one was supplied,
        // otherwise fall back to owning our own.
        infra::net::IoRuntime* runtime_ref = external_runtime_;
        if (runtime_ref == nullptr) {
            runtime_ = std::make_unique<infra::net::IoRuntime>();
            runtime_ref = runtime_.get();
        }
        endpoints_.reserve(fds.size());
        dispatchers_.reserve(fds.size());

        std::size_t installed = 0;
        for (int fd : fds) {
            auto ep = std::make_unique<infra::net::UdpEndpoint>(*runtime_ref);
            auto adopted = ep->adopt_native_handle(fd);
            if (!adopted.has_value()) {
                continue;
            }
            auto disp = std::make_unique<LegacyUdpDispatcher>(*ep, fd);
            disp->start();
            ep->start();
            endpoints_.push_back(std::move(ep));
            dispatchers_.push_back(std::move(disp));
            ++installed;
        }

        if (installed == 0) {
            runtime_.reset();
            running_ = false;
            return core::fail(core::Error{
                core::StatusCode::Internal,
                "UdpBridge: no UDP endpoints could be adopted"});
        }

        // `IoRuntime::run()` is idempotent: if the runtime is shared
        // with another bridge that has already started it, this is a
        // no-op.
        runtime_ref->run(1);
        return installed;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        for (auto& ep : endpoints_) ep->close();
        // Only stop the runtime when we own it; an externally supplied
        // runtime is the caller's responsibility.
        if (runtime_) runtime_->stop();
        dispatchers_.clear();
        endpoints_.clear();
        runtime_.reset();
    }

    std::size_t endpoint_count() const noexcept { return endpoints_.size(); }

private:
    std::atomic<bool>                                          running_{false};
    infra::net::IoRuntime*                                     external_runtime_{nullptr};
    std::unique_ptr<infra::net::IoRuntime>                     runtime_;
    std::vector<std::unique_ptr<infra::net::UdpEndpoint>>      endpoints_;
    std::vector<std::unique_ptr<LegacyUdpDispatcher>>          dispatchers_;
};

UdpBridge::UdpBridge() : impl_(std::make_unique<UdpBridgeImpl>()) {}
UdpBridge::UdpBridge(infra::net::IoRuntime& runtime)
    : impl_(std::make_unique<UdpBridgeImpl>(runtime)) {}
UdpBridge::~UdpBridge() = default;

core::Result<std::size_t> UdpBridge::install() { return impl_->start(); }
void                       UdpBridge::shutdown() { impl_->stop(); }
std::size_t                UdpBridge::endpoint_count() const noexcept {
    return impl_->endpoint_count();
}

}  // namespace pvpgn::integration::legacy_bnetd
