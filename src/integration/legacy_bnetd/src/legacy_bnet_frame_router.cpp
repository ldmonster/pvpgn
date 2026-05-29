// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/legacy_bnet_frame_router.hpp"

#include <mutex>
#include <span>
#include <utility>

namespace pvpgn::integration::legacy_bnetd {

namespace {

// Lock-protected because the hook is read on every dispatched frame
// (per-session io thread) and may be written by static-init in the
// linked variant or by a test fixture. `std::function` move-assign
// is not atomic, hence the mutex.
std::mutex&                            hook_mutex() {
    static std::mutex m;
    return m;
}
LegacyBnetFrameRouter::DispatchHook&   hook_slot() {
    static LegacyBnetFrameRouter::DispatchHook h;
    return h;
}

LegacyBnetFrameRouter::ClassRefresh&   class_refresh_slot() {
    static LegacyBnetFrameRouter::ClassRefresh h;
    return h;
}

}  // namespace

void LegacyBnetFrameRouter::set_dispatch_hook(DispatchHook hook) noexcept {
    std::lock_guard<std::mutex> g{hook_mutex()};
    hook_slot() = std::move(hook);
}

void LegacyBnetFrameRouter::clear_dispatch_hook() noexcept {
    std::lock_guard<std::mutex> g{hook_mutex()};
    hook_slot() = nullptr;
}

void LegacyBnetFrameRouter::set_class_refresh(ClassRefresh hook) noexcept {
    std::lock_guard<std::mutex> g{hook_mutex()};
    class_refresh_slot() = std::move(hook);
}

void LegacyBnetFrameRouter::clear_class_refresh() noexcept {
    std::lock_guard<std::mutex> g{hook_mutex()};
    class_refresh_slot() = nullptr;
}

bool LegacyBnetFrameRouter::send_outbound(
    std::span<const std::byte> bytes) noexcept {
    auto* eg = egress();
    if (eg == nullptr) return false;
    std::vector<std::byte> copy(bytes.begin(), bytes.end());
    eg->send(std::move(copy));
    return true;
}

void LegacyBnetFrameRouter::dispatch_frame(LegacyFrame frame) {
    // Copy the hook out under the lock so we don't hold it across
    // the (potentially expensive, possibly callback-into-us)
    // dispatch call. `std::function` copy is cheap when the target
    // is a small object or a function pointer; the linked variant's
    // hook is a free-function pointer wrapper.
    DispatchHook local;
    {
        std::lock_guard<std::mutex> g{hook_mutex()};
        local = hook_slot();
    }

    if (local) {
        const std::span<const std::byte> view{
            frame.payload.data(), frame.payload.size()};
        auto* eg = egress();
        if (eg != nullptr && local(conn_, view, *eg)) {
            // Hook took the frame. After dispatch the legacy
            // connection may have transitioned class (e.g.
            // conn_class_init -> conn_class_bnet after the magic
            // byte was consumed). Mirror that on the v3 side so
            // subsequent frames are framed correctly.
            ClassRefresh refresh;
            {
                std::lock_guard<std::mutex> g{hook_mutex()};
                refresh = class_refresh_slot();
            }
            if (refresh && conn_ != nullptr) {
                set_class(refresh(conn_));
            }
            return;  // hook took the frame
        }
        // Fall-through: hook installed but declined (or no egress
        // available yet -- start() not called). Record so the
        // caller can diagnose.
    }

    recorded_.push_back(std::move(frame));
}

}  // namespace pvpgn::integration::legacy_bnetd
