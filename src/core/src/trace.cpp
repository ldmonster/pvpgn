// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/trace.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pvpgn::core::trace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Generate a random hex string of `byte_count` bytes (= 2*byte_count chars).
std::string random_hex(std::size_t byte_count) {
    static std::mutex rng_mu;
    static std::mt19937_64 rng{std::random_device{}()};

    std::lock_guard<std::mutex> lk(rng_mu);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    // Each call to rng() gives 8 bytes; generate enough words.
    std::size_t words = (byte_count + 7) / 8;
    std::size_t emitted = 0;
    for (std::size_t w = 0; w < words && emitted < byte_count; ++w) {
        std::uint64_t val = rng();
        for (int b = 7; b >= 0 && emitted < byte_count; --b, ++emitted) {
            oss << std::setw(2)
                << static_cast<unsigned>((val >> (b * 8)) & 0xFFu);
        }
    }
    return oss.str();
}

// Process-wide span sink protected by a mutex.
std::mutex g_sink_mu;
SpanSink   g_sink;  // default-constructed = empty std::function = no-op

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Span::Impl
// ---------------------------------------------------------------------------

struct Span::Impl {
    std::string                              name;
    SpanContext                              ctx;
    std::chrono::system_clock::time_point   start_time;
    std::chrono::system_clock::time_point   end_time;
    std::vector<std::pair<std::string, std::string>> attributes;
    bool        is_error{false};
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Span
// ---------------------------------------------------------------------------

Span::Span(std::string_view name)
    : impl_(std::make_unique<Impl>()) {
    impl_->name       = std::string(name);
    impl_->ctx.trace_id      = random_hex(16);  // 32-char hex
    impl_->ctx.span_id       = random_hex(8);   // 16-char hex
    impl_->ctx.parent_span_id = {};             // root span
    impl_->start_time = std::chrono::system_clock::now();
}

Span::~Span() {
    if (!impl_) return;  // moved-from

    impl_->end_time = std::chrono::system_clock::now();

    // Call the global sink (if any) with the completed span.
    SpanSink sink;
    {
        std::lock_guard<std::mutex> lk(g_sink_mu);
        sink = g_sink;
    }
    if (sink) {
        sink(*this);
    }
}

Span::Span(Span&&) noexcept = default;
Span& Span::operator=(Span&&) noexcept = default;

void Span::set_attribute(std::string_view key, std::string_view value) {
    if (!impl_) return;
    impl_->attributes.emplace_back(std::string(key), std::string(value));
}

void Span::set_status_ok() {
    if (!impl_) return;
    impl_->is_error      = false;
    impl_->error_message = {};
}

void Span::set_status_error(std::string_view message) {
    if (!impl_) return;
    impl_->is_error      = true;
    impl_->error_message = std::string(message);
}

const SpanContext& Span::context() const noexcept {
    static const SpanContext empty{};
    return impl_ ? impl_->ctx : empty;
}

std::string_view Span::name() const noexcept {
    static const std::string empty{};
    return impl_ ? std::string_view{impl_->name} : std::string_view{empty};
}

std::chrono::system_clock::time_point Span::start_time() const noexcept {
    return impl_ ? impl_->start_time : std::chrono::system_clock::time_point{};
}

std::chrono::system_clock::time_point Span::end_time() const noexcept {
    return impl_ ? impl_->end_time : std::chrono::system_clock::time_point{};
}

bool Span::is_error() const noexcept {
    return impl_ && impl_->is_error;
}

std::string_view Span::error_message() const noexcept {
    static const std::string empty{};
    return impl_ ? std::string_view{impl_->error_message} : std::string_view{empty};
}

// ---------------------------------------------------------------------------
// Global SpanSink
// ---------------------------------------------------------------------------

void set_global_span_sink(SpanSink sink) {
    std::lock_guard<std::mutex> lk(g_sink_mu);
    g_sink = std::move(sink);
}

SpanSink get_global_span_sink() {
    std::lock_guard<std::mutex> lk(g_sink_mu);
    return g_sink;
}

}  // namespace pvpgn::core::trace
