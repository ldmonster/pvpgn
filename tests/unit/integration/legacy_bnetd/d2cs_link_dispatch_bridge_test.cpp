// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/d2cs_link_dispatch_bridge.hpp"

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

TEST_CASE("d2cs link dispatch bridge null conn no-op",
          "[integration][legacy_bnetd][d2cs_link_dispatch_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_link_dispatch_try(nullptr, "authreply") == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("d2cs link dispatch bridge logs authreply",
          "[integration][legacy_bnetd][d2cs_link_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_link_dispatch_try(&marker, "authreply") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].module == "v3_d2cs_link_dispatch_bridge");
    REQUIRE(sink.records[0].message == "d2cs link dispatch observed");
    REQUIRE(field_value(sink.records[0], "op") == "authreply");
}

TEST_CASE("d2cs link dispatch bridge logs accountloginreq",
          "[integration][legacy_bnetd][d2cs_link_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_link_dispatch_try(&marker, "accountloginreq") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "accountloginreq");
}

TEST_CASE("d2cs link dispatch bridge logs charloginreq",
          "[integration][legacy_bnetd][d2cs_link_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_link_dispatch_try(&marker, "charloginreq") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "charloginreq");
}

TEST_CASE("d2cs link dispatch bridge logs gameinforeply",
          "[integration][legacy_bnetd][d2cs_link_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_link_dispatch_try(&marker, "gameinforeply") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "gameinforeply");
}

TEST_CASE("d2cs link dispatch bridge null op normalised",
          "[integration][legacy_bnetd][d2cs_link_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_link_dispatch_try(&marker, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "?");
}
