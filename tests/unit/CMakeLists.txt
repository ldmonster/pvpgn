# Test sub-tree. Only built when PVPGN_V3_BUILD_TESTS=ON.

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# Treat Catch2 headers as system headers so our strict warnings
# (-Wnon-virtual-dtor, -Wold-style-cast, ...) don't fire on them.
foreach(_t Catch2 Catch2WithMain)
    if(TARGET ${_t})
        set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
    endif()
endforeach()

enable_testing()
include(CTest)

add_subdirectory(core)
add_subdirectory(infra)
add_subdirectory(protocol)
add_subdirectory(domain)
add_subdirectory(application)
add_subdirectory(integration)
