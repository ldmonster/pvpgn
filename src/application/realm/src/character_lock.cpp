#include "application/realm/character_lock.hpp"

namespace pvpgn::application::realm {

CharacterLockUseCase::CharacterLockUseCase(ICharacterRepository& repo)
    : repo_(repo)
{
}

core::Result<void, core::Error> CharacterLockUseCase::lock(const LockCharacterCommand& cmd) {
    domain::realm::CharacterId id{cmd.account_name, cmd.char_name};
    
    auto find_result = repo_.find(id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }
    
    auto character = std::move(find_result).value();
    auto lock_result = character.lock(cmd.gs_address);
    if (!lock_result) {
        return core::fail(std::move(lock_result).error());
    }
    
    return repo_.save(character);
}

core::Result<void, core::Error> CharacterLockUseCase::unlock(const UnlockCharacterCommand& cmd) {
    domain::realm::CharacterId id{cmd.account_name, cmd.char_name};
    
    auto find_result = repo_.find(id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }
    
    auto character = std::move(find_result).value();
    auto unlock_result = character.unlock(cmd.gs_address);
    if (!unlock_result) {
        return core::fail(std::move(unlock_result).error());
    }
    
    return repo_.save(character);
}

core::Result<bool, core::Error> CharacterLockUseCase::is_locked(const domain::realm::CharacterId& id) {
    auto find_result = repo_.find(id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }
    
    return core::Result<bool, core::Error>(std::move(find_result).value().is_locked());
}

} // namespace pvpgn::application::realm
