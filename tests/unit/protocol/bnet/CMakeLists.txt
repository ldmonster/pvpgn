pvpgn_v3_add_test(test_protocol_bnet_codec
    SOURCES codec_test.cpp
    DEPS    protocol_bnet)

# Plan 10: in-tree property tests (round-trip identity + decode robustness).
pvpgn_v3_add_test(test_protocol_bnet_codec_property
    SOURCES codec_property_test.cpp
    DEPS    protocol_bnet)

# M1: the production ISessionContext (encode -> finalize -> egress). Guards the
# double-finalize regression that left every BnetFsm reply un-sent.
pvpgn_v3_add_test(test_protocol_bnet_session_context_impl
    SOURCES session_context_impl_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_init_wire_types
    SOURCES init_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_init_codec
    SOURCES init_codec_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_bot_wire_types
    SOURCES bot_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_anongame_wire_types
    SOURCES anongame_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_w3route_wire_types
    SOURCES w3route_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_auth_wire_types
    SOURCES auth_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_account_wire_types
    SOURCES account_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_cdkey_wire_types
    SOURCES cdkey_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_realm_wire_types
    SOURCES realm_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_chat_wire_types
    SOURCES chat_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_game_wire_types
    SOURCES game_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_clan_wire_types
    SOURCES clan_wire_types_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_misc_wire_types
    SOURCES misc_wire_types_test.cpp
    DEPS    protocol_bnet)

# Disabled: BnetFsm tests require proper use case mocks (LoginUser, ChangePassword, etc.)
# The make_test_context() helper returns all nullptrs, causing handle() to fail
# These tests need either:
# 1. Mock implementations of all use cases
# 2. Integration with actual use case implementations
# 3. Refactoring BnetFsm to not require all use cases for basic state transitions
pvpgn_v3_add_test(test_protocol_bnet_fsm
    SOURCES fsm_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_fsm_golden
    SOURCES fsm_golden_test.cpp capturing_session_context.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_anongame
    SOURCES anongame_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_anongame_tags
    SOURCES anongame_tags_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_golden_replay
    SOURCES golden_replay_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_fsm_channel
    SOURCES fsm_channel_test.cpp
    DEPS    protocol_bnet)
