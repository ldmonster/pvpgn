pvpgn_v3_add_test(test_domain_shared_value_objects
    SOURCES value_objects_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_client_tag
    SOURCES client_tag_test.cpp
    DEPS    domain_shared)

# R213a (plans/01-modern-cpp-baseline.md S3): every public header in
# src/v3/domain/shared/include/domain/shared/ must compile in isolation.
pvpgn_v3_add_header_selfcheck(test_domain_shared_headers_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/v3/domain/shared/include
    HEADERS
        domain/shared/ban.hpp
        domain/shared/bn_hash.hpp
        domain/shared/chat_message.hpp
        domain/shared/client_tag.hpp
        domain/shared/events.hpp
        domain/shared/ids.hpp
        domain/shared/ip_address.hpp
        domain/shared/locale.hpp
        domain/shared/match_report.hpp
        domain/shared/user_name.hpp
    DEPS domain_shared
)
