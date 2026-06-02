pvpgn_v3_add_test(test_application_ports_clan_repository
    SOURCES clan_repository_test.cpp
    DEPS    domain_social application_ports)

# Plan 05 finalization: src/application/ports/ has been DELETED. The previous
# header-selfcontained check over application/ports/* is therefore removed.
# The genuinely-application ports that used to live here (event_loop,
# trace_sink, resolver, random_source, icon_provider) were relocated to
# src/domain/shared/include/domain/shared/ports/ and are covered there.
