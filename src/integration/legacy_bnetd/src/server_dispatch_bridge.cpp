#include "integration/legacy_bnetd/server_dispatch_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_server_dispatch(void* opaque, char const* op) noexcept {
    (void)opaque;  // server-level observation is keyed only by op.
    std::string_view op_sv = (op != nullptr) ? std::string_view{op} : std::string_view{"?"};
    if (op_sv.empty()) op_sv = std::string_view{"?"};
    const pvpgn::core::ILogger::Field fields[] = {{"op", op_sv}};
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_server_dispatch_bridge",
        "server dispatch observed",
        {fields[0]});
    return 0;
}
