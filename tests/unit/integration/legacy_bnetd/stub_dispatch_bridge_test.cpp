// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/stub_dispatch_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct CapturedLog {
    pvpgn::core::LogLevel level{};
    std::string module;
    std::string message;
    std::vector<std::pair<std::string, std::string>> fields;
};

class RecordingLogger : public pvpgn::core::ILogger {
public:
    void log(pvpgn::core::LogLevel level,
             std::string_view module,
             std::string_view message) noexcept override {
        records.push_back(CapturedLog{
            level, std::string{module}, std::string{message}, {}});
    }
    pvpgn::core::LogLevel level() const noexcept override {
        return pvpgn::core::LogLevel::Trace;
    }
    void set_level(pvpgn::core::LogLevel) noexcept override {}
    void log_kv(pvpgn::core::LogLevel level,
                std::string_view module,
                std::string_view message,
                std::span<const Field> fields) noexcept override {
        CapturedLog c;
        c.level   = level;
        c.module  = std::string{module};
        c.message = std::string{message};
        for (const auto& f : fields) {
            c.fields.emplace_back(std::string{f.key},
                                  std::string{f.value});
        }
        records.push_back(std::move(c));
    }
    std::vector<CapturedLog> records;
};

std::string field_value(const CapturedLog& r,
                        std::string_view key) {
    for (const auto& kv : r.fields) {
        if (kv.first == key) return kv.second;
    }
    return {};
}

}  // namespace

TEST_CASE("stub dispatch bridge null conn no-op",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(nullptr, "unknown_1b") == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("stub dispatch bridge logs unknown_1b",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(&marker, "unknown_1b") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].module == "v3_stub_dispatch_bridge");
    REQUIRE(sink.records[0].message == "stub dispatch observed");
    REQUIRE(field_value(sink.records[0], "op") == "unknown_1b");
}

TEST_CASE("stub dispatch bridge logs unknown2b",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(&marker, "unknown2b") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "unknown2b");
}

TEST_CASE("stub dispatch bridge logs unknown39",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(&marker, "unknown39") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "unknown39");
}

TEST_CASE("stub dispatch bridge logs regsnoopreply",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(&marker, "regsnoopreply") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "regsnoopreply");
}

TEST_CASE("stub dispatch bridge logs readmemory",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(&marker, "readmemory") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "readmemory");
}

TEST_CASE("stub dispatch bridge null op normalised",
          "[integration][legacy_bnetd][stub_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_stub_dispatch(&marker, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "?");
}
