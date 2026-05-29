#include "services/d2dbs/d2dbs_composition.hpp"
#include "infra/persistence/realm/filesystem_save_store.hpp"

namespace pvpgn::services::d2dbs {

std::string D2dbsComposition::service_name() const {
    return "d2dbs";
}

core::Result<void, std::string> D2dbsComposition::init(const runtime::ServiceConfig& config) {
    // Create the save file store (filesystem-based)
    // TODO: Make this configurable (in-memory vs filesystem)
    std::string save_dir = "./saves";
    
    try {
        save_store_ = std::make_unique<pvpgn::infra::persistence::realm::FilesystemSaveStore>(save_dir);
        
        // Create the character persistence use case
        persistence_ = std::make_unique<pvpgn::application::realm::CharacterPersistenceUseCase>(*save_store_);
        
        return core::Result<void, std::string>();
    } catch (const std::exception& e) {
        return core::fail(std::string("Failed to initialize d2dbs: ") + e.what());
    }
}

core::Result<void, std::string> D2dbsComposition::start() {
    running_ = true;
    // TODO: Initialize any background tasks or connections
    return core::Result<void, std::string>();
}

void D2dbsComposition::stop() {
    running_ = false;
    // TODO: Cleanup any resources
}

void D2dbsComposition::shutdown() {
    stop();
    save_store_.reset();
    persistence_.reset();
}

std::string D2dbsComposition::status() const {
    return running_ ? "running" : "stopped";
}

} // namespace pvpgn::services::d2dbs
