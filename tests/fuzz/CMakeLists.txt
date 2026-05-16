# Fuzz targets — only built with clang and -DPVPGN_ENABLE_FUZZING=ON
option(PVPGN_ENABLE_FUZZING "Enable LibFuzzer fuzz targets" OFF)

if(PVPGN_ENABLE_FUZZING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # BNet codec fuzzer
    add_executable(fuzz_bnet_codec bnet_codec_fuzz.cpp)
    target_link_libraries(fuzz_bnet_codec PRIVATE -fsanitize=fuzzer,address)
    target_compile_options(fuzz_bnet_codec PRIVATE -fsanitize=fuzzer,address)
    set_target_properties(fuzz_bnet_codec PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz"
    )

    # D2Save codec fuzzer
    add_executable(fuzz_d2save_codec d2save_codec_fuzz.cpp)
    target_link_libraries(fuzz_d2save_codec PRIVATE -fsanitize=fuzzer,address)
    target_compile_options(fuzz_d2save_codec PRIVATE -fsanitize=fuzzer,address)
    set_target_properties(fuzz_d2save_codec PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz"
    )
endif()
