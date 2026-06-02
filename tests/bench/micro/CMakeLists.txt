# Plan 13 — microbenchmark suite. EXCLUDE_FROM_ALL: never built by `make all`
# and never registered with ctest; built + run on demand via
# scripts/dev/run-bench.sh micro (or `cmake --build <dir> --target bench_micro`).
add_executable(bench_micro EXCLUDE_FROM_ALL micro_main.cpp)
target_include_directories(bench_micro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(bench_micro PRIVATE cxx_std_23)
target_link_libraries(bench_micro
    PRIVATE
        protocol_bnet
        pvpgn_v3_plugin
        core)
