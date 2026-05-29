add_executable(test_sandbox
    sandbox_test.cpp
)
target_link_libraries(test_sandbox
    PRIVATE
        pvpgn_sandbox
        GTest::gtest_main
)
target_include_directories(test_sandbox
    PRIVATE ${CMAKE_SOURCE_DIR}/src/infra/sandbox/include
)
add_test(NAME SandboxTest COMMAND test_sandbox)
