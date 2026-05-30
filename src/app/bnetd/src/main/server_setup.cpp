// SPDX-License-Identifier: GPL-2.0-or-later
// main/server_setup.cpp — ServerConfig builder and BnetUseCaseContext factory.

#include "main/server_setup.hpp"

namespace pvpgn::app::bnetd {

ServerConfig build_config(const CliArgs& args) {
    ServerConfig cfg;
    if (args.bnet_port != 0)     cfg.bnet_port      = args.bnet_port;
    if (!args.data_dir.empty())  cfg.data_dir        = args.data_dir;
    if (!args.log_level.empty()) cfg.log_level       = args.log_level;
    if (args.threads != 0)       cfg.worker_threads  = args.threads;
    return cfg;
}

protocol::bnet::BnetUseCaseContext
build_use_cases(services::bnetd::BnetdService& svc) {
    return svc.make_use_case_context();
}

} // namespace pvpgn::app::bnetd
