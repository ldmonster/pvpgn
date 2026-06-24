// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/core/trace_test.cpp
//
// Unit tests for core::trace — span id generation, parent propagation (child
// spans share the trace and link to the parent), and head sampling (the sink
// only fires for sampled traces; children inherit the parent's decision).

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/trace.hpp"

using namespace pvpgn::core::trace;

namespace {

// RAII: capture completed spans via the global sink, restoring state on scope
// exit so tests don't leak the sink or a non-default sample ratio.
struct SinkCapture {
    std::vector<std::string> names;
    std::vector<SpanContext> contexts;

    SinkCapture() {
        set_global_span_sink([this](const Span& s) {
            names.emplace_back(s.name());
            contexts.push_back(s.context());
        });
    }
    ~SinkCapture() {
        set_global_span_sink({});
        set_sample_ratio(1.0);
    }
};

}  // namespace

TEST_CASE("trace: root span has well-formed ids and is a root", "[core][trace]") {
    SinkCapture cap;
    SpanContext ctx;
    {
        Span s{"root"};
        ctx = s.context();
    }
    CHECK(ctx.trace_id.size() == 32);          // 16 bytes hex
    CHECK(ctx.span_id.size() == 16);           // 8 bytes hex
    CHECK(ctx.parent_span_id.empty());         // root
    CHECK(ctx.sampled);                        // default ratio 1.0
    REQUIRE(cap.names.size() == 1);
    CHECK(cap.names[0] == "root");
}

TEST_CASE("trace: child span propagates the trace and links to the parent",
          "[core][trace]") {
    SinkCapture cap;
    SpanContext parent_ctx;
    SpanContext child_ctx;
    {
        Span parent{"parent"};
        parent_ctx = parent.context();
        {
            Span child{"child", parent.context()};
            child_ctx = child.context();
        }
    }
    // Same trace, linked to parent, fresh span id.
    CHECK(child_ctx.trace_id == parent_ctx.trace_id);
    CHECK(child_ctx.parent_span_id == parent_ctx.span_id);
    CHECK(child_ctx.span_id != parent_ctx.span_id);
    CHECK(child_ctx.span_id.size() == 16);
    CHECK(child_ctx.sampled == parent_ctx.sampled);

    // Child ends (and is exported) before the parent.
    REQUIRE(cap.names.size() == 2);
    CHECK(cap.names[0] == "child");
    CHECK(cap.names[1] == "parent");
}

TEST_CASE("trace: sampling ratio 0 drops spans from the sink", "[core][trace]") {
    SinkCapture cap;
    set_sample_ratio(0.0);
    {
        Span s{"unsampled"};
        CHECK_FALSE(s.context().sampled);
        // A child of an unsampled root stays unsampled.
        Span child{"child", s.context()};
        CHECK_FALSE(child.context().sampled);
    }
    CHECK(cap.names.empty());  // nothing exported
}

TEST_CASE("trace: sampling ratio 1 exports every span", "[core][trace]") {
    SinkCapture cap;
    set_sample_ratio(1.0);
    {
        Span a{"a"};
        Span b{"b"};
    }
    CHECK(cap.names.size() == 2);
}

TEST_CASE("trace: set_sample_ratio clamps to [0, 1]", "[core][trace]") {
    SinkCapture cap;  // restores ratio to 1.0 on exit
    set_sample_ratio(-5.0);
    CHECK(get_sample_ratio() == 0.0);
    set_sample_ratio(42.0);
    CHECK(get_sample_ratio() == 1.0);
}

TEST_CASE("trace: attributes and error status are recorded", "[core][trace]") {
    SinkCapture cap;
    {
        Span s{"op"};
        s.set_attribute("key", "value");
        s.set_status_error("boom");
        CHECK(s.is_error());
        CHECK(s.error_message() == "boom");
    }
    REQUIRE(cap.names.size() == 1);
}
