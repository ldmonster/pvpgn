# SPDX-License-Identifier: GPL-2.0-or-later
pvpgn_v3_add_test(test_application_chat
    SOURCES chat_command_test.cpp
    DEPS    application_chat)

pvpgn_v3_add_test(test_application_chat_whisper
    SOURCES whisper_use_case_test.cpp
    DEPS    application_chat)

pvpgn_v3_add_test(test_application_chat_whisper_lookup
    SOURCES whisper_target_lookup_test.cpp
    DEPS    application_chat)

pvpgn_v3_add_test(test_application_chat_reply_sink
    SOURCES chat_reply_sink_test.cpp
    DEPS    application_chat)

pvpgn_v3_add_test(test_application_chat_join
    SOURCES join_channel_test.cpp
    DEPS    application_chat
            infra_inmemory
            infra_storage)

pvpgn_v3_add_test(test_application_chat_post_message
    SOURCES post_message_test.cpp
    DEPS    application_chat
            infra_storage)

pvpgn_v3_add_test(test_application_chat_leave
    SOURCES leave_channel_test.cpp
    DEPS    application_chat
            infra_storage)

pvpgn_v3_add_test(test_application_chat_list
    SOURCES list_channels_test.cpp
    DEPS    application_chat
            infra_storage)

pvpgn_v3_add_test(test_application_chat_send_emote
    SOURCES send_emote_test.cpp
    DEPS    application_chat
            infra_storage)

pvpgn_v3_add_test(test_application_chat_kick
    SOURCES kick_from_channel_test.cpp
    DEPS    application_chat
            infra_storage)

pvpgn_v3_add_test(test_application_chat_ban
    SOURCES ban_from_channel_test.cpp
    DEPS    application_chat
            infra_storage)

pvpgn_v3_add_test(test_application_chat_set_topic
    SOURCES set_channel_topic_test.cpp
    DEPS    application_chat
            infra_storage)
