#pragma once
#include "runtime/service_host.hpp"
#include "application/realm/character_lock.hpp"
#include "application/realm/gs_queue.hpp"
#include <memory>

using pvpgn::core::Result;

namespace pvpgn::services::d2cs {

class D2csComposition : public runtime::IServiceComposition {
public:
    std::string service_name() const override { return "d2cs"; }
    Result<void, std::string> init(const runtime::ServiceConfig& config) override;
    Result<void, std::string> start() override;
    void stop() override;
    void shutdown() override;
    std::string status() const override;

private:
    std::unique_ptr<application::realm::ICharacterRepository> char_repo_;
    std::unique_ptr<application::realm::CharacterLockUseCase> char_lock_;
    std::unique_ptr<application::realm::GameServerQueue> gs_queue_;
};

} // namespace pvpgn::services::d2cs
