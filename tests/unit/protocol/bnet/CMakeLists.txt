pvpgn_v3_add_test(test_protocol_bnet_codec
    SOURCES codec_test.cpp
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
