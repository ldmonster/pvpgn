pvpgn_v3_add_test(test_core_result    SOURCES result_test.cpp        DEPS core)
pvpgn_v3_add_test(test_core_error     SOURCES error_test.cpp         DEPS core)
pvpgn_v3_add_test(test_core_endian    SOURCES endian_test.cpp        DEPS core)
pvpgn_v3_add_test(test_core_clock     SOURCES clock_test.cpp         DEPS core)
pvpgn_v3_add_test(test_core_strong    SOURCES strong_typedef_test.cpp DEPS core)
pvpgn_v3_add_test(test_core_bytes     SOURCES bytes_test.cpp         DEPS core)
pvpgn_v3_add_test(test_core_logging   SOURCES logging_test.cpp       DEPS core)
pvpgn_v3_add_test(test_core_version   SOURCES version_test.cpp       DEPS core)
pvpgn_v3_add_test(test_core_event_bus SOURCES event_bus_test.cpp     DEPS core)
pvpgn_v3_add_test(test_core_scheduler SOURCES scheduler_test.cpp     DEPS core)
pvpgn_v3_add_test(test_core_format    SOURCES format_test.cpp        DEPS core)
pvpgn_v3_add_test(test_core_bnettime  SOURCES bnettime_test.cpp      DEPS core)
pvpgn_v3_add_test(test_core_hexdump      SOURCES hexdump_test.cpp         DEPS core)
pvpgn_v3_add_test(test_core_string_utils SOURCES string_utils_test.cpp    DEPS core)
pvpgn_v3_add_test(test_core_contract     SOURCES contract_test.cpp        DEPS core)
# R331: Secret<T> — prevents accidental logging of sensitive values
pvpgn_v3_add_test(test_core_secret       SOURCES secret_test.cpp          DEPS core)
# Plan 08: canonical CSPRNG
pvpgn_v3_add_test(test_core_secure_random SOURCES secure_random_test.cpp  DEPS core)
# Plan 11: tracing (parent propagation + sampling)
pvpgn_v3_add_test(test_core_trace       SOURCES trace_test.cpp           DEPS core)

# R213 (plans/01-modern-cpp-baseline.md S3): every public header in
# src/v3/core/include/core/ must compile in isolation. Adding a new
# header? Append it to the HEADERS list below.
pvpgn_v3_add_header_selfcheck(test_core_headers_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/core/include
    HEADERS
        core/bnettime.hpp
        core/bytes.hpp
        core/clock.hpp
        core/contract.hpp
        core/crypto/password_hasher.hpp
        core/crypto/secure_random.hpp
        core/cxx.hpp
        core/endian.hpp
        core/error.hpp
        core/event_bus.hpp
        core/format.hpp
        core/hexdump.hpp
        core/logging.hpp
        core/result.hpp
        core/scheduler.hpp
        core/secret.hpp
        core/string_utils.hpp
        core/strong_typedef.hpp
        core/trace.hpp
        core/version.hpp
    DEPS core
)
