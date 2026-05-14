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
