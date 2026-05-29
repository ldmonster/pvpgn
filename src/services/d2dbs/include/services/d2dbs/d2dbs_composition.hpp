#pragma once
#include "runtime/service_host.hpp"
#include "application/realm/character_persistence.hpp"
#include "domain/realm/dupe_checker.hpp"
#include <memory>

namespace pvpgn::services::d2dbs {

class D2dbsComposition : public runtime::IServiceComposition {
public:
    std::string service_name() const override;
    core::Result<void, std::string> init(const runtime::ServiceConfig& config) override;
    core::Result<void, std::string> start() override;
    void stop() override;
    void shutdown() override;
    std::string status() const override;

private:
    std::unique_ptr<domain::realm::ISaveFileStore> save_store_;
    std::unique_ptr<application::realm::CharacterPersistenceUseCase> persistence_;
    bool running_ = false;
};

} // namespace pvpgn::services::d2dbs
