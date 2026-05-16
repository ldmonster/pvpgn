#include "application/realm/character_persistence.hpp"
#include "protocol/d2save/codec.hpp"

namespace pvpgn::application::realm {

CharacterPersistenceUseCase::CharacterPersistenceUseCase(domain::realm::ISaveFileStore& store)
    : store_(store) {
}

core::Result<void, core::Error> CharacterPersistenceUseCase::save(const SaveCharacterCommand& cmd) {
    if (cmd.account_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Account name cannot be empty"
        ));
    }

    if (cmd.char_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Character name cannot be empty"
        ));
    }

    if (cmd.save_data.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Save data cannot be empty"
        ));
    }

    // Validate the save file format
    auto parse_result = pvpgn::protocol::d2save::D2SaveCodec::parse(cmd.save_data);
    if (!parse_result.has_value()) {
        return core::fail(parse_result.error());
    }

    // TODO: Implement dupe checking if enabled
    // if (cmd.check_dupes) {
    //     // Load other characters and check for dupes
    // }

    // Store the save file
    return store_.store(cmd.account_name, cmd.char_name, cmd.save_data);
}

core::Result<LoadCharacterResult, core::Error> CharacterPersistenceUseCase::load(const LoadCharacterCommand& cmd) {
    if (cmd.account_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Account name cannot be empty"
        ));
    }

    if (cmd.char_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Character name cannot be empty"
        ));
    }

    // Load the save file
    auto load_result = store_.load(cmd.account_name, cmd.char_name);
    if (!load_result.has_value()) {
        return core::fail(load_result.error());
    }

    auto save_data = load_result.value();

    // Parse the save file to extract metadata
    auto parse_result = pvpgn::protocol::d2save::D2SaveCodec::parse(save_data);
    if (!parse_result.has_value()) {
        return core::fail(parse_result.error());
    }

    auto save_file = parse_result.value();

    // Extract character metadata
    auto level_result = pvpgn::protocol::d2save::D2SaveCodec::extract_level(save_data);
    if (!level_result.has_value()) {
        return core::fail(level_result.error());
    }

    auto class_result = pvpgn::protocol::d2save::D2SaveCodec::extract_class(save_data);
    if (!class_result.has_value()) {
        return core::fail(class_result.error());
    }

    auto expansion_result = pvpgn::protocol::d2save::D2SaveCodec::is_expansion(save_data);
    if (!expansion_result.has_value()) {
        return core::fail(expansion_result.error());
    }

    auto hardcore_result = pvpgn::protocol::d2save::D2SaveCodec::is_hardcore(save_data);
    if (!hardcore_result.has_value()) {
        return core::fail(hardcore_result.error());
    }

    LoadCharacterResult result;
    result.save_data = save_data;
    result.char_name = cmd.char_name;
    result.level = level_result.value();
    result.char_class = class_result.value();
    result.is_expansion = expansion_result.value();
    result.is_hardcore = hardcore_result.value();

    return core::Result<LoadCharacterResult, core::Error>(result);
}

core::Result<void, core::Error> CharacterPersistenceUseCase::remove(const DeleteCharacterCommand& cmd) {
    if (cmd.account_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Account name cannot be empty"
        ));
    }

    if (cmd.char_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Character name cannot be empty"
        ));
    }

    return store_.remove(cmd.account_name, cmd.char_name);
}

core::Result<bool, core::Error> CharacterPersistenceUseCase::exists(std::string_view account, std::string_view char_name) {
    if (account.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Account name cannot be empty"
        ));
    }

    if (char_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Character name cannot be empty"
        ));
    }

    return store_.exists(account, char_name);
}

core::Result<std::vector<std::string>, core::Error> 
CharacterPersistenceUseCase::list_characters(std::string_view account_name) {
    if (account_name.empty()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Account name cannot be empty"
        ));
    }

    // TODO: Implement character listing
    // This would require a method on ISaveFileStore to list characters
    return core::Result<std::vector<std::string>, core::Error>(std::vector<std::string>());
}

} // namespace pvpgn::application::realm
