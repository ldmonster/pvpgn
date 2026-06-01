# Version information (moved from src/common/ in Plan 02)
# Header-only library
add_library(core_version INTERFACE)
target_include_directories(core_version INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
