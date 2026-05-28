pvpgn_v3_add_test(test_application_admin_commands_router
    SOURCES router_test.cpp
    DEPS    application_admin_commands)

pvpgn_v3_add_test(test_application_admin_commands_help_corpus_parser
    SOURCES help_corpus_parser_test.cpp
    DEPS    application_admin_commands)

pvpgn_v3_add_test(test_application_admin_commands_file_help_responder
    SOURCES file_help_responder_test.cpp
    DEPS    application_admin_commands)
