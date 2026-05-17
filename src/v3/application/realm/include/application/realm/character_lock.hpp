#pragma once
#include "domain/realm/character.hpp"
#include "core/result.hpp"
#include <functional>
#include <string>
#include <vector>

namespace pvpgn::application::realm {

// Port (interface) for character repository
class ICharacterRepository {
public:
    virtual ~ICharacterRepository() = default;
    virtual core::Result<domain::realm::Character, core::Error>
        find(const domain::realm::CharacterId& id) = 0;
    virtual core::Result<void, core::Error>
        save(const domain::realm::Character& character) = 0;
    virtual core::Result<void, core::Error>
        remove(const domain::realm::CharacterId& id) = 0;
    virtual core::Result<std::vector<domain::realm::Character>, core::Error>
        list_for_account(std::string_view account_name) = 0;
};

struct LockCharacterCommand {
    std::string account_name;
    std::string char_name;
    std::string gs_address;  // Game server requesting the lock
};

struct UnlockCharacterCommand {
    std::string account_name;
    std::string char_name;
    std::string gs_address;
};

class CharacterLockUseCase {
public:
    explicit CharacterLockUseCase(ICharacterRepository& repo);
    
    core::Result<void, core::Error> lock(const LockCharacterCommand& cmd);
    core::Result<void, core::Error> unlock(const UnlockCharacterCommand& cmd);
    core::Result<bool, core::Error> is_locked(const domain::realm::CharacterId& id);

private:
    ICharacterRepository& repo_;
};

} // namespace pvpgn::application::realm
