#include "services/d2cs/d2cs_composition.hpp"
#include "infra/persistence/realm/inmemory_character_repository.hpp"

namespace pvpgn::services::d2cs {

core::Result<void, std::string> D2csComposition::init(const runtime::ServiceConfig& config) {
    // Initialize in-memory character repository
    char_repo_ = std::make_unique<infra::persistence::realm::InMemoryCharacterRepository>();
    
    // Initialize character lock use case
    char_lock_ = std::make_unique<application::realm::CharacterLockUseCase>(*char_repo_);
    
    // Initialize game server queue
    gs_queue_ = std::make_unique<application::realm::GameServerQueue>();
    
    return core::Result<void, std::string>();
}

core::Result<void, std::string> D2csComposition::start() {
    // Start accepting connections, etc.
    return core::Result<void, std::string>();
}

void D2csComposition::stop() {
    // Stop accepting new connections
}

void D2csComposition::shutdown() {
    // Cleanup resources
    gs_queue_.reset();
    char_lock_.reset();
    char_repo_.reset();
}

std::string D2csComposition::status() const {
    return "d2cs service running";
}

} // namespace pvpgn::services::d2cs
