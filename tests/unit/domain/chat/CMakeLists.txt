pvpgn_v3_add_test(test_domain_chat_channel
    SOURCES channel_test.cpp
    DEPS    domain_chat)

pvpgn_v3_add_test(test_domain_chat_whisper
    SOURCES whisper_test.cpp
    DEPS    domain_chat)
