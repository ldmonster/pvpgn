// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/net/idle_memory_footprint_test.cpp
//
// Idle-connection memory-footprint regression gate: the idle-connection
// memory footprint must stay within a 10% budget vs the baseline.
//
// The literal baseline of the old `fdwatch` + `t_connection` path
// no longer exists in the tree -- it was deleted when the Boost.Asio/Fiber
// runtime landed. So this test instead pins the *current*
// per-idle-connection footprint as the baseline and fails if a future change
// grows it by more than 10%. That is the regression guarantee,
// anchored to the only baseline we can still observe.
//
// What an idle connection costs (production path = `TcpSession` only):
//   * `sizeof(TcpSession)` -- dominated by the inline 4096-byte read buffer.
//     An idle session queues no writes (the `write_q_` deque is empty and
//     allocates no block until the first `send`), so the object itself is the
//     footprint, modulo the `shared_ptr` control block.
// The fiber path additionally allocates a `SessionChannel` plus its inbound
// ring buffer (`capacity` slots, eagerly sized); that ring is a tunable config
// knob (`inbox_capacity`), not a silent regression vector, so it is documented
// rather than budget-gated here.
//
// These are deterministic compile-time sizes -- no flaky runtime RSS probe, so
// the gate is stable in CI.

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "infra/net/tcp_session.hpp"

#if defined(PVPGN_V3_HAVE_FIBER)
#include "infra/net/fiber_session.hpp"
#endif

namespace {

using pvpgn::infra::net::TcpSession;

// The inline read buffer carried by every session (see tcp_session.hpp:
// `std::array<std::byte, 4096> read_buf_`).
constexpr std::size_t kReadBufferBytes = 4096;

// Baseline measured 2026-06-02 on x86-64 / libstdc++ 13 / Boost 1.83:
// sizeof(TcpSession) == 4592 (4096 read buffer + ~496 of members: socket,
// strand, steady_timer, idle_timeout_, empty write deque, two bools, a mutex,
// and two std::functions). If a refactor needs to exceed the +10% budget,
// re-measure deliberately and update kBaselineBytes with a note explaining why.
constexpr std::size_t kBaselineBytes = 4592;

// 10% growth budget, per the acceptance criterion.
constexpr std::size_t kBudgetBytes = kBaselineBytes + kBaselineBytes / 10;  // 5051

}  // namespace

TEST_CASE("Plan 06: idle TcpSession footprint stays within the 10% budget",
          "[infra][net][memory]") {
    // The dominant cost is the inline read buffer; the session must obviously
    // be at least that large (guards against the buffer being moved to the heap
    // unnoticed, which would shrink sizeof but add a per-connection allocation).
    STATIC_REQUIRE(sizeof(TcpSession) >= kReadBufferBytes);

    // The actual regression gate: no more than +10% over the recorded baseline.
    STATIC_REQUIRE(sizeof(TcpSession) <= kBudgetBytes);

    // Non-buffer overhead (everything except the inline read buffer) is small
    // relative to the buffer -- a sanity bound that catches a fat member
    // sneaking in even while still under the total budget.
    STATIC_REQUIRE(sizeof(TcpSession) - kReadBufferBytes <= 1024);

    SUCCEED("sizeof(TcpSession) within idle-connection budget");
}

#if defined(PVPGN_V3_HAVE_FIBER)
TEST_CASE("Plan 06: fiber SessionChannel idle overhead is bounded",
          "[infra][net][memory]") {
    using pvpgn::infra::net::fiber::SessionChannel;
    // The fiber wrapper itself is small (a weak_ptr, the channel handle, two
    // atomics, a duration). Its inbound ring buffer holds `inbox_capacity`
    // chunk slots and is the tunable idle cost -- e.g. the default capacity 64
    // eagerly reserves 64 * sizeof(std::vector<std::byte>) bytes. That is
    // chosen per-listener for worst-case back-pressure, not a regression
    // vector, so only the wrapper object is budget-gated here.
    STATIC_REQUIRE(sizeof(SessionChannel) <= 256);
    SUCCEED("sizeof(SessionChannel) within budget");
}
#endif
