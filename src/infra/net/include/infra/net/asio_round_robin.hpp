// SPDX-License-Identifier: BSL-1.0
//
// Vendored from Boost.Fiber's `examples/asio/round_robin.hpp`
// (Boost 1.83.0, https://github.com/boostorg/fiber). Distributed
// under the Boost Software License 1.0; see LICENSE_1_0.txt for
// terms. The original example carries:
//
//     Copyright Oliver Kowalke 2013.
//
// Trimmed for PVPGN's needs:
//   * Removed the `yield.hpp` include (we never use the asio yield_t
//     completion token here — fibers in PVPGN block on
//     `SessionChannel`, not on raw asio operations).
//   * No other behavioural changes.
//
// Why we vendor: the file lives in Boost's `examples/` tree and is
// not installed by upstream packagers. This pragma keeps GCC quiet
// about minor pedantic issues in the example code.

#ifndef PVPGN_INFRA_NET_ASIO_ROUND_ROBIN_HPP
#define PVPGN_INFRA_NET_ASIO_ROUND_ROBIN_HPP

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#  pragma GCC diagnostic ignored "-Wcast-align"
#  pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <boost/fiber/condition_variable.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/mutex.hpp>
#include <boost/fiber/operations.hpp>
#include <boost/fiber/scheduler.hpp>

namespace boost { namespace fibers { namespace asio {

class round_robin : public algo::algorithm {
private:
    std::shared_ptr<boost::asio::io_context> io_ctx_;
    boost::asio::steady_timer                suspend_timer_;
    boost::fibers::scheduler::ready_queue_type rqueue_{};
    boost::fibers::mutex                       mtx_{};
    boost::fibers::condition_variable          cnd_{};
    std::size_t                                counter_{0};

public:
    struct service : public boost::asio::execution_context::service {
        using key_type = service;

        static boost::asio::execution_context::id id;

        // Modern replacement for the removed `io_context::work`:
        // an executor_work_guard keeps the io_context's run() from
        // returning while any guard is alive, which is exactly the
        // behaviour the original example relied on.
        std::unique_ptr<
            boost::asio::executor_work_guard<
                boost::asio::io_context::executor_type>> work_;

        service(boost::asio::execution_context& ctx)
            : boost::asio::execution_context::service(ctx),
              work_{
                  new boost::asio::executor_work_guard<
                      boost::asio::io_context::executor_type>(
                      boost::asio::make_work_guard(
                          static_cast<boost::asio::io_context&>(ctx)
                              .get_executor()))} {}

        ~service() override = default;

        service(const service&)            = delete;
        service& operator=(const service&) = delete;

        // execution_context::service uses `shutdown()` not
        // `shutdown_service()` since Boost 1.66+.
        void shutdown() noexcept override final {
            work_.reset();
        }
    };

    round_robin(const std::shared_ptr<boost::asio::io_context>& io_ctx)
        : io_ctx_(io_ctx), suspend_timer_(*io_ctx_) {
        // Modern spelling: `make_service<T>(ctx, args...)` replaces
        // the deprecated `add_service(ctx, new T(...))`.
        boost::asio::make_service<service>(*io_ctx_);
        boost::asio::post(*io_ctx_, [this]() mutable {
            while (!io_ctx_->stopped()) {
                if (has_ready_fibers()) {
                    while (io_ctx_->poll())
                        ;
                    std::unique_lock<boost::fibers::mutex> lk(mtx_);
                    cnd_.wait(lk);
                } else {
                    if (!io_ctx_->run_one()) {
                        break;
                    }
                }
            }
        });
    }

    void awakened(context* ctx) noexcept override {
        BOOST_ASSERT(nullptr != ctx);
        BOOST_ASSERT(!ctx->ready_is_linked());
        ctx->ready_link(rqueue_);
        if (!ctx->is_context(boost::fibers::type::dispatcher_context)) {
            ++counter_;
        }
    }

    context* pick_next() noexcept override {
        context* ctx(nullptr);
        if (!rqueue_.empty()) {
            ctx = &rqueue_.front();
            rqueue_.pop_front();
            BOOST_ASSERT(nullptr != ctx);
            BOOST_ASSERT(context::active() != ctx);
            if (!ctx->is_context(boost::fibers::type::dispatcher_context)) {
                --counter_;
            }
        }
        return ctx;
    }

    bool has_ready_fibers() const noexcept override {
        return 0 < counter_;
    }

    void suspend_until(
        std::chrono::steady_clock::time_point const& abs_time) noexcept override {
        if ((std::chrono::steady_clock::time_point::max)() != abs_time) {
            suspend_timer_.expires_at(abs_time);
            suspend_timer_.async_wait(
                [](boost::system::error_code const&) {
                    this_fiber::yield();
                });
        }
        cnd_.notify_one();
    }

    void notify() noexcept override {
        suspend_timer_.async_wait(
            [](boost::system::error_code const&) { this_fiber::yield(); });
        suspend_timer_.expires_at(std::chrono::steady_clock::now());
    }
};

inline boost::asio::execution_context::id round_robin::service::id;

}}}  // namespace boost::fibers::asio

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif  // PVPGN_INFRA_NET_ASIO_ROUND_ROBIN_HPP
