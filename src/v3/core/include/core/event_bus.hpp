// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_bus.hpp
/// In-process publish/subscribe bus. Phase 1 dual-emits legacy side-effects
/// alongside `EventBus::publish<T>(evt)` so new subscribers (metrics, audit,
/// WebUI) can attach without touching legacy call sites.
///
/// Design:
///   * Header-only, no dependencies beyond the STL.
///   * Type-keyed: one channel per event type `T`.
///   * Synchronous delivery on the caller's thread. Async fan-out is a
///     Phase 2 concern (will be done via a dedicated fiber).
///   * `Subscription` is an RAII handle; destruction unsubscribes.
///   * Subscribers must not block the publisher for long; failures are
///     swallowed and logged via `core::log()` so one bad subscriber cannot
///     break the publish chain.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "logging.hpp"

namespace pvpgn::core {

class EventBus;

class Subscription {
public:
    Subscription() noexcept = default;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept { swap(other); }
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            reset();
            swap(other);
        }
        return *this;
    }
    ~Subscription() { reset(); }

    void reset() noexcept;
    explicit operator bool() const noexcept { return id_ != 0 && bus_ != nullptr; }

private:
    friend class EventBus;
    Subscription(EventBus* bus, std::type_index type, std::uint64_t id) noexcept
        : bus_(bus), type_(type), id_(id) {}

    void swap(Subscription& o) noexcept {
        std::swap(bus_, o.bus_);
        std::swap(type_, o.type_);
        std::swap(id_, o.id_);
    }

    EventBus*       bus_  = nullptr;
    std::type_index type_ = std::type_index(typeid(void));
    std::uint64_t   id_   = 0;
};

class EventBus {
public:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    template <class T, class F>
    [[nodiscard]] Subscription subscribe(F&& fn) {
        static_assert(std::is_invocable_v<F&, const T&>,
                      "Subscriber must be callable as void(const T&)");
        auto wrapper = std::make_shared<std::function<void(const void*)>>(
            [f = std::forward<F>(fn)](const void* p) {
                f(*static_cast<const T*>(p));
            });

        const std::type_index ti(typeid(T));
        const std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;

        std::unique_lock lk(mu_);
        channels_[ti].push_back({id, std::move(wrapper)});
        return Subscription{this, ti, id};
    }

    template <class T>
    void publish(const T& event) noexcept {
        std::vector<std::shared_ptr<std::function<void(const void*)>>> snapshot;
        {
            std::shared_lock lk(mu_);
            auto it = channels_.find(std::type_index(typeid(T)));
            if (it == channels_.end()) return;
            snapshot.reserve(it->second.size());
            for (auto& s : it->second) snapshot.push_back(s.fn);
        }
        for (auto& fn : snapshot) {
            try {
                (*fn)(static_cast<const void*>(&event));
            } catch (...) {
                core::log(LogLevel::Error, "event_bus", "subscriber threw");
            }
        }
    }

    std::size_t subscriber_count(std::type_index ti) const noexcept {
        std::shared_lock lk(mu_);
        auto it = channels_.find(ti);
        return it == channels_.end() ? 0u : it->second.size();
    }

private:
    friend class Subscription;

    struct Slot {
        std::uint64_t id;
        std::shared_ptr<std::function<void(const void*)>> fn;
    };

    void remove(std::type_index ti, std::uint64_t id) noexcept {
        std::unique_lock lk(mu_);
        auto it = channels_.find(ti);
        if (it == channels_.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [id](const Slot& s) { return s.id == id; }),
                  vec.end());
        if (vec.empty()) channels_.erase(it);
    }

    mutable std::shared_mutex mu_;
    std::unordered_map<std::type_index, std::vector<Slot>> channels_;
    std::atomic<std::uint64_t> next_id_{0};
};

inline void Subscription::reset() noexcept {
    if (bus_ && id_ != 0) {
        bus_->remove(type_, id_);
    }
    bus_ = nullptr;
    id_  = 0;
}

}  // namespace pvpgn::core
