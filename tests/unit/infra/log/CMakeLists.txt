if(TARGET infra_log)
    pvpgn_v3_add_test(test_infra_log_spdlog
        SOURCES spdlog_logger_test.cpp
        DEPS    infra_log)
endif()

pvpgn_v3_add_test(test_infra_log_json_line
    SOURCES json_line_logger_test.cpp
    DEPS    infra_log_simple)

pvpgn_v3_add_test(test_infra_log_json_line_composition
    SOURCES json_line_logger_composition_test.cpp
    DEPS    infra_log_simple)
