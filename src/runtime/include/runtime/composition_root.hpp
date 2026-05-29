// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// Composition Root Pattern for Service Dependency Injection
/// 
/// Each service (bnetd, d2cs, d2dbs) implements IServiceComposition to define
/// its dependencies and lifecycle. This header provides the template interface
/// that services must implement.
///
/// Example usage in d2cs:
///
///   class D2csComposition : public IServiceComposition {
///   public:
///       std::string service_name() const override { return "d2cs"; }
///       
///       Result<void, std::string> init(const ServiceConfig& config) override {
///           // Create and wire up all dependencies
///           db_ = std::make_unique<CharacterDatabase>(config);
///           game_server_ = std::make_unique<GameServer>(db_.get());
///           return Result<void, std::string>();
///       }
///       
///       Result<void, std::string> start() override {
///           return game_server_->start();
///       }
///       
///       void stop() override {
///           game_server_->stop();
///       }
///       
///       void shutdown() override {
///           game_server_.reset();
///           db_.reset();
///       }
///       
///       std::string status() const override {
///           return game_server_->status();
///       }
///   
///   private:
///       std::unique_ptr<CharacterDatabase> db_;
///       std::unique_ptr<GameServer> game_server_;
///   };

#include "runtime/service_host.hpp"
#include <memory>

namespace pvpgn::runtime {

/// Template helper to create a service composition
/// 
/// Usage:
///   int main(int argc, char** argv) {
///       return run_service<D2csComposition>(argc, argv);
///   }
template<typename CompositionT>
int run_service(int argc, char** argv)
{
    ServiceHost host;
    auto composition = std::make_unique<CompositionT>();
    return host.run(std::move(composition), argc, argv);
}

} // namespace pvpgn::runtime
