if(PVPGN_SINGLE_BINARY)
    add_executable(test_combined_composition combined_composition_test.cpp)
    target_link_libraries(test_combined_composition
        PRIVATE pvpgn_combined_service GTest::gtest_main)
    add_test(NAME CombinedCompositionTest COMMAND test_combined_composition)
endif()
