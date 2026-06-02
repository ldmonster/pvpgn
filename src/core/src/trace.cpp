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

// Process-wide head sampling ratio in [0, 1]. Default 1.0 = sample everything,
// so behaviour with a sink installed is unchanged until an operator lowers it.
std::mutex g_sample_mu;
double     g_sample_ratio = 1.0;

/// Draw a sampling decision for a root span from the current ratio.
bool draw_sampled() {
    double ratio;
    {
        std::lock_guard<std::mutex> lk(g_sample_mu);
        ratio = g_sample_ratio;
    }
    if (ratio >= 1.0) return true;
    if (ratio <= 0.0) return false;

    static std::mutex su_mu;
    static std::mt19937_64 su_rng{std::random_device{}()};
    std::lock_guard<std::mutex> lk(su_mu);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(su_rng) < ratio;
}

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
    impl_->ctx.sampled        = draw_sampled(); // head sampling decision
    impl_->start_time = std::chrono::system_clock::now();
}

Span::Span(std::string_view name, const SpanContext& parent)
    : impl_(std::make_unique<Impl>()) {
    impl_->name        = std::string(name);
    // Inherit the trace and the sampling decision; new span id; link to parent.
    impl_->ctx.trace_id      = parent.trace_id;
    impl_->ctx.span_id       = random_hex(8);
    impl_->ctx.parent_span_id = parent.span_id;
    impl_->ctx.sampled        = parent.sampled;
    impl_->start_time = std::chrono::system_clock::now();
}

Span::~Span() {
    if (!impl_) return;  // moved-from

    impl_->end_time = std::chrono::system_clock::now();

    // Only export spans belonging to a sampled trace.
    if (!impl_->ctx.sampled) return;

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

void set_sample_ratio(double ratio) {
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    std::lock_guard<std::mutex> lk(g_sample_mu);
    g_sample_ratio = ratio;
}

double get_sample_ratio() {
    std::lock_guard<std::mutex> lk(g_sample_mu);
    return g_sample_ratio;
}

}  // namespace pvpgn::core::trace
