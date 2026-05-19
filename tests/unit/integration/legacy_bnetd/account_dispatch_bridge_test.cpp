// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/account_dispatch_bridge.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

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

TEST_CASE("account dispatch bridge null conn no-op",
          "[integration][legacy_bnetd][account_dispatch_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_account_dispatch_try(nullptr,
                                             "createacctreq1") == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("account dispatch bridge logs op",
          "[integration][legacy_bnetd][account_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_account_dispatch_try(&marker,
                                             "changepassreq") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].module == "v3_account_dispatch_bridge");
    REQUIRE(sink.records[0].message == "account dispatch observed");
    REQUIRE(field_value(sink.records[0], "op") == "changepassreq");
}

TEST_CASE("account dispatch bridge logs multiple ops in sequence",
          "[integration][legacy_bnetd][account_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_account_dispatch_try(&marker,
                                             "createaccountw3") == 0);
    REQUIRE(::pvpgn_v3_account_dispatch_try(&marker,
                                             "createacctreq2") == 0);
    REQUIRE(::pvpgn_v3_account_dispatch_try(&marker,
                                             "setemailreply") == 0);
    REQUIRE(::pvpgn_v3_account_dispatch_try(&marker,
                                             "changeemailreq") == 0);
    REQUIRE(sink.records.size() == 4u);
    REQUIRE(field_value(sink.records[0], "op") == "createaccountw3");
    REQUIRE(field_value(sink.records[1], "op") == "createacctreq2");
    REQUIRE(field_value(sink.records[2], "op") == "setemailreply");
    REQUIRE(field_value(sink.records[3], "op") == "changeemailreq");
}

TEST_CASE("account dispatch bridge null op normalised",
          "[integration][legacy_bnetd][account_dispatch_bridge]") {
    int marker = 0;
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_account_dispatch_try(&marker, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "op") == "?");
}
