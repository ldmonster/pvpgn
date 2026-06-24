// SPDX-License-Identifier: GPL-2.0-or-later
//
// Stateless decision function: given a single entrant + the
// current queue snapshot + the bracket size, decides
// kDuplicate / kRejected / kQueued / kPromoted. The repository
// abstraction in `lobby_repository.hpp` is what the bridge layer
// uses to wire this dispatcher up against the legacy
// `anongame_queue` global; the dispatcher itself never touches
// any state.

#include "application/anongame_lobby/lobby.hpp"

#include <algorithm>

namespace pvpgn::application::anongame_lobby {

LobbyAdmitResponse dispatch_admit(LobbyAdmitRequest const& req) {
    LobbyAdmitResponse out;

    // Malformed request -> kRejected (the default).
    if (req.bracket_size < 2u)        return out;
    if (req.entrant.account_id == 0u) return out;

    // Duplicate check: account_id already in the queue.
    auto const already_queued = std::any_of(
        req.current_queue.begin(),
        req.current_queue.end(),
        [&](LobbyEntry const& q) {
            return q.account_id == req.entrant.account_id;
        });
    if (already_queued) {
        out.status = LobbyAdmitStatus::kDuplicate;
        return out;
    }

    // Bracket would overflow if existing queue >= bracket_size:
    // treat as kRejected (caller's queue accounting is wrong).
    if (req.current_queue.size() + 1u > req.bracket_size) {
        return out;  // kRejected
    }

    // Promote: queue would be exactly full after appending the
    // entrant. The promoted_party is current_queue ++ entrant in
    // repository order (legacy FIFO promotion).
    if (req.current_queue.size() + 1u == req.bracket_size) {
        out.status = LobbyAdmitStatus::kPromoted;
        out.promoted_party.reserve(req.bracket_size);
        for (auto const& q : req.current_queue) {
            out.promoted_party.push_back(q);
        }
        out.promoted_party.push_back(req.entrant);
        return out;
    }

    // Otherwise just queue.
    out.status = LobbyAdmitStatus::kQueued;
    return out;
}

}  // namespace pvpgn::application::anongame_lobby
