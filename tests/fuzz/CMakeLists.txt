# Fuzz targets — only built with clang and -DPVPGN_ENABLE_FUZZING=ON
option(PVPGN_ENABLE_FUZZING "Enable LibFuzzer fuzz targets" OFF)

if(PVPGN_ENABLE_FUZZING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # BNet codec fuzzer — wired to decode_client() via next_frame() (R275).
    add_executable(fuzz_bnet_codec bnet_codec_fuzz.cpp)
    target_compile_definitions(fuzz_bnet_codec PRIVATE PVPGN_FUZZING_ENABLED)
    target_link_libraries(fuzz_bnet_codec PRIVATE
        protocol_bnet
        protocol_common
        -fsanitize=fuzzer,address
    )
    target_compile_options(fuzz_bnet_codec PRIVATE -fsanitize=fuzzer,address)
    set_target_properties(fuzz_bnet_codec PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz"
    )

    # D2Save codec fuzzer — wired to D2SaveCodec::parse() (R276).
    add_executable(fuzz_d2save_codec d2save_codec_fuzz.cpp)
    target_compile_definitions(fuzz_d2save_codec PRIVATE PVPGN_FUZZING_ENABLED)
    target_link_libraries(fuzz_d2save_codec PRIVATE
        protocol_d2save
        -fsanitize=fuzzer,address
    )
    target_compile_options(fuzz_d2save_codec PRIVATE -fsanitize=fuzzer,address)
    set_target_properties(fuzz_d2save_codec PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz"
    )
else()
    # Non-fuzzing build: compile bnet_codec_fuzz.cpp with its no-op main()
    # so the translation unit is always syntax-checked by the build system.
    add_executable(fuzz_bnet_codec_stub bnet_codec_fuzz.cpp)
    target_link_libraries(fuzz_bnet_codec_stub PRIVATE
        protocol_bnet
        protocol_common
    )
    set_target_properties(fuzz_bnet_codec_stub PROPERTIES
        EXCLUDE_FROM_ALL TRUE
    )

    # Non-fuzzing build: compile d2save_codec_fuzz.cpp with its no-op main()
    # so the translation unit is always syntax-checked by the build system.
    add_executable(fuzz_d2save_codec_stub d2save_codec_fuzz.cpp)
    target_link_libraries(fuzz_d2save_codec_stub PRIVATE
        protocol_d2save
    )
    set_target_properties(fuzz_d2save_codec_stub PROPERTIES
        EXCLUDE_FROM_ALL TRUE
    )
endif()
