// SPDX-License-Identifier: GPL-2.0-or-later

#include "application/anongame_inforeply/type_composer.hpp"

#include <cstddef>

namespace pvpgn::application::anongame_inforeply {

namespace pb = pvpgn::protocol::bnet;

namespace {

// Section selectors mirror the legacy filter expressions in
// `anongame_infos_load`:
//   PG: prefix[1] == 0  &&  prefix[4] == 0
//   AT: prefix[1] == 0  &&  prefix[4] != 0
//   TY: prefix[1] == 1
bool is_pg(const std::array<std::uint8_t, 5>& p) {
    return p[1] == 0 && p[4] == 0;
}
bool is_at(const std::array<std::uint8_t, 5>& p) {
    return p[1] == 0 && p[4] != 0;
}
bool is_ty(const std::array<std::uint8_t, 5>& p) {
    return p[1] == 1;
}

void emit_section(
    pb::AnonGameTypePayload& out,
    std::uint8_t section_id,
    bool (*selector)(const std::array<std::uint8_t, 5>&),
    std::span<const std::vector<std::uint8_t>> queue_map_indices,
    const std::array<std::array<std::uint8_t, 5>,
                     kAnonGameQueueCount>& prefix_table) {
    pb::AnonGameTypeSection section;
    section.section_id = section_id;
    for (std::size_t j = 0; j < kAnonGameQueueCount; ++j) {
        if (queue_map_indices[j].empty()) continue;
        if (!selector(prefix_table[j])) continue;
        pb::AnonGameTypeGamestyle g;
        g.prefix      = prefix_table[j];
        g.map_indices = queue_map_indices[j];
        section.gamestyles.push_back(std::move(g));
    }
    if (!section.gamestyles.empty()) {
        out.sections.push_back(std::move(section));
    }
}

}  // namespace

pb::AnonGameTypePayload compose_type_payload(
    std::span<const std::vector<std::uint8_t>> queue_map_indices,
    const std::array<std::array<std::uint8_t, 5>,
                     kAnonGameQueueCount>& prefix_table) {
    pb::AnonGameTypePayload out;
    if (queue_map_indices.size() != kAnonGameQueueCount) return out;
    emit_section(out, 0x00, &is_pg, queue_map_indices, prefix_table);
    emit_section(out, 0x01, &is_at, queue_map_indices, prefix_table);
    emit_section(out, 0x02, &is_ty, queue_map_indices, prefix_table);
    return out;
}

}  // namespace pvpgn::application::anongame_inforeply
