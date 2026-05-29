#include "domain/realm/character.hpp"

namespace pvpgn::domain::realm {

Character::Character(CharacterId id, CharacterStats stats, core::SystemTime now)
    : id_(std::move(id))
    , stats_(stats)
    , created_at_(now)
    , last_played_(now)
{
}

core::Result<void, core::Error> Character::lock(std::string gs_address) {
    if (locked_by_gs_.has_value()) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::FailedPrecondition,
                           "Character is already locked by " + locked_by_gs_.value())
        );
    }
    locked_by_gs_ = std::move(gs_address);
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> Character::unlock(std::string_view gs_address) {
    if (!locked_by_gs_.has_value()) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::FailedPrecondition, "Character is not locked")
        );
    }
    if (locked_by_gs_.value() != gs_address) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::PermissionDenied,
                           "Character is locked by different GS: " + locked_by_gs_.value())
        );
    }
    locked_by_gs_.reset();
    return core::Result<void, core::Error>();
}

} // namespace pvpgn::domain::realm
