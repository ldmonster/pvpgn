// SPDX-License-Identifier: GPL-2.0-or-later
//
// Repository abstraction over the legacy `AdBannerList` global.
// Lets the integration bridge inject any backend (legacy banner
// list, a future TOML-driven source, an in-test fake) without the
// application layer depending on legacy types.

#pragma once

#include "application/ads/ad_pick.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace pvpgn::application::ads {

/// Interface implemented by adapters over the legacy `AdBannerList`
/// global (or a test double). The dispatcher itself stays stateless;
/// callers pre-resolve candidates by calling `list_for`.
class IAdsRepository {
public:
    virtual ~IAdsRepository() = default;

    /// Return all candidates for the given client_tag / lang_tag,
    /// already filtered by the format rule (MNG only for WAR3,
    /// non-MNG for others) so the application dispatcher does not
    /// need to know about extension tags.
    virtual std::vector<AdCandidate> list_for(std::uint32_t client_tag,
                                              std::uint32_t lang_tag) const = 0;

    /// Lookup a single ad by id. Used by the click path so it does
    /// not have to walk the whole list each time.
    virtual std::optional<AdCandidate> find_by_id(std::uint32_t client_tag,
                                                  std::uint32_t lang_tag,
                                                  std::uint32_t ad_id) const = 0;
};

}  // namespace pvpgn::application::ads
