// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/list_characters.hpp"

namespace pvpgn::application::realm {

ListCharactersUseCase::ListCharactersUseCase(ICharacterListRepository& list_repo)
    : list_repo_(list_repo)
{
}

core::Result<ListCharactersResult, core::Error>
ListCharactersUseCase::execute(const ListCharactersQuery& query) {
    if (query.account_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Account name cannot be empty"));
    }

    auto list_result = list_repo_.find_for_account(query.account_name);
    if (!list_result) {
        return core::fail(std::move(list_result).error());
    }

    const auto& char_list = list_result.value();
    auto sorted = char_list.list(query.sort_mode);

    ListCharactersResult result;
    result.characters.reserve(sorted.size());

    for (const auto* ch : sorted) {
        CharacterSummary summary;
        summary.char_name  = ch->id().char_name;
        summary.char_class = ch->stats().char_class;
        summary.level      = ch->stats().level;
        summary.expansion  = (ch->stats().expansion == domain::realm::CharacterExpansion::lod);
        summary.hardcore   = (ch->stats().hardcore  == domain::realm::CharacterHardcore::hardcore);
        summary.dead       = ch->stats().dead;
        result.characters.push_back(std::move(summary));
    }

    return core::Result<ListCharactersResult, core::Error>(std::move(result));
}

} // namespace pvpgn::application::realm
