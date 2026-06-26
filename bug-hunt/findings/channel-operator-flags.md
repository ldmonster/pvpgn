# Channel operator/member flags (DEFERRED — wave 57)

Status: confirmed divergence, deferred as a subsystem.

Observable (tests/diff/probe_opflags.py in scratchpad): in the original, the
SID_CHATEVENT FLAGS u32 (body offset 4) for EID_SHOWUSER/EID_JOIN/EID_USERFLAGS
carries the member's channel status. The FIRST user of a fresh non-permanent
channel becomes its operator and is reported with MF_GAVEL = 0x02; a later joiner
sees that operator as 0x02. v3 hardcodes the flags field to 0 in every chat event.

Oracle reference: channel.cpp:462-471 (first member -> tmpOP), channel.cpp:1588-
1626 (channel_set_userflags computes admin=MF_BLIZZARD 0x01, operator=MF_BNET
0x08, tmpOP=MF_GAVEL 0x02, voice=MF_VOICE 0x04), message.cpp:757/775/873 (wire
field = conn_get_flags | dstflags, dstflags adds the recipient-relative squelch
bit 0x20).

v3 root cause: src/protocol/bnet/src/fsm/fsm_chat.cpp — SHOWUSER/USERFLAGS/JOIN
ChatEvent constructions all pass flags=0; no per-member channel-flag state exists.

Why deferred (not a quick fix):
1. Channel aggregate (domain/chat/channel.hpp) stores members_ in an
   unordered_map — no join order, so "first joiner" needs explicit operator state
   added to the aggregate (+ with_id/rehydrate/persistence preservation).
2. Permanent/predefined channels must NOT auto-op their first user. v3 creates
   default channels via JoinChannel with empty (non-permanent) flags, so a naive
   "first user => op" would wrongly op the default channel's first user — a NEW
   divergence. Needs correct permanent-channel classification first.
3. Operator migration when the operator leaves (oracle picks a new tmpOP).
4. EID_USERFLAGS update broadcasts when flags change.
5. Full parity also needs admin/op/voice tiers, the transient MF_PLUG (0x10)
   UDP-capability bit, and the recipient-relative squelch bit (0x20) in dstflags.

Fix shape: add operator state to the Channel aggregate (set on first admit of a
non-permanent channel, migrate/clear on leave), expose operator_id(), and compute
the flags field in the 3 fsm_chat.cpp chat-event sites from the channel's
operator + (later) the squelch-relative bit. Build a real member-flags subsystem
for full parity.

## UPDATE wave 58: IMPLEMENTED (operator gavel 0x02)
The first-user-operator case is now fixed (commit in wave 58). Channel aggregate
tracks operator_id_ (first admit of a non-permanent channel; migrated on
leave/kick; cleared when empty); fsm_chat.cpp emits 0x02 in USERFLAGS/SHOWUSER/
JOIN from channel.operator_id(). Verified by tests/diff/diff_channel_op.py against
the oracle. Confirmed safe because default channels are seeded Permanent
(BnetdService) so they are not auto-op'd. STILL NOT MODELED (minor): the transient
MF_PLUG (0x10) UDP-capability bit, the admin(0x01)/op(0x08)/voice(0x04) tiers, and
the recipient-relative squelch bit (0x20) in dstflags — full member-flags parity.
