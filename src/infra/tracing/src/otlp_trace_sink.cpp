// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef PVPGN_V3_WITH_OTLP

#include "infra/tracing/otlp_trace_sink.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <spdlog/spdlog.h>

#include "core/trace.hpp"

namespace pvpgn::infra::tracing {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Escape a string for embedding in a JSON value (no surrounding quotes).
std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

/// Convert a system_clock time_point to nanoseconds since Unix epoch.
std::int64_t to_unix_ns(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
}

/// Build a minimal OTLP/HTTP JSON payload for a single span.
/// Follows the OTLP JSON encoding spec (opentelemetry-proto).
std::string build_otlp_json(const ::pvpgn::core::trace::Span& span) {
    const auto& ctx = span.context();

    std::ostringstream j;
    j << R"({"resourceSpans":[{"resource":{},"scopeSpans":[{"scope":{},"spans":[{)";
    j << R"("traceId":")" << json_escape(ctx.trace_id) << R"(",)";
    j << R"("spanId":")"  << json_escape(ctx.span_id)  << R"(",)";
    if (!ctx.parent_span_id.empty()) {
        j << R"("parentSpanId":")" << json_escape(ctx.parent_span_id) << R"(",)";
    }
    j << R"("name":")"           << json_escape(span.name())          << R"(",)";
    j << R"("startTimeUnixNano":)" << to_unix_ns(span.start_time())   << R"(,)";
    j << R"("endTimeUnixNano":")"  << to_unix_ns(span.end_time())     << R"(",)";
    j << R"("status":{"code":)"
      << (span.is_error() ? "2" : "1");  // 1=OK, 2=ERROR per OTLP spec
    if (span.is_error() && !span.error_message().empty()) {
        j << R"(,"message":")" << json_escape(span.error_message()) << R"(")";
    }
    j << "}";
    j << "}]}]}]}";
    return j.str();
}

/// Parse "http://host:port/path" into (host, port_str, path).
/// Returns false on parse failure.
bool parse_http_url(const std::string& url,
                    std::string& host,
                    std::string& port,
                    std::string& path) {
    // Expect "http://<host>[:<port>]<path>"
    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) return false;

    auto rest = url.substr(prefix.size());
    auto slash = rest.find('/');
    std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : rest.substr(slash);

    auto colon = authority.rfind(':');
    if (colon == std::string::npos) {
        host = authority;
        port = "80";
    } else {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
    }
    return !host.empty() && !port.empty();
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// OtlpTraceSink
// ---------------------------------------------------------------------------

OtlpTraceSink::OtlpTraceSink(std::string endpoint)
    : endpoint_(std::move(endpoint)) {}

OtlpTraceSink::~OtlpTraceSink() = default;

void OtlpTraceSink::record(const ::pvpgn::core::trace::Span& span) noexcept {
    try {
        std::string host, port, path;
        if (!parse_http_url(endpoint_, host, port, path)) {
            SPDLOG_WARN("[otlp] invalid endpoint URL: {}", endpoint_);
            return;
        }

        const std::string body = build_otlp_json(span);

        // Synchronous HTTP POST using Boost.Beast.
        namespace beast = boost::beast;
        namespace http  = beast::http;
        namespace net   = boost::asio;
        using tcp       = net::ip::tcp;

        net::io_context ioc;
        tcp::resolver   resolver{ioc};
        beast::tcp_stream stream{ioc};

        auto const results = resolver.resolve(host, port);
        stream.connect(results);

        http::request<http::string_body> req{http::verb::post, path, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "pvpgn-v3-otlp/1.0");
        req.set(http::field::content_type, "application/json");
        req.content_length(body.size());
        req.body() = body;
        req.prepare_payload();

        http::write(stream, req);

        beast::flat_buffer buf;
        http::response<http::string_body> res;
        http::read(stream, buf, res);

        if (res.result_int() >= 400) {
            SPDLOG_WARN("[otlp] collector returned HTTP {}", res.result_int());
        }

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        // Ignore shutdown errors — connection may already be closed.

    } catch (const std::exception& ex) {
        SPDLOG_WARN("[otlp] export failed: {}", ex.what());
    } catch (...) {
        SPDLOG_WARN("[otlp] export failed: unknown exception");
    }
}

}  // namespace pvpgn::infra::tracing

#endif  // PVPGN_V3_WITH_OTLP
