# tests/unit/app/CMakeLists.txt
# Unit tests for the app/ composition-root layer.

add_subdirectory(bnetd)
add_subdirectory(d2cs)
if(TARGET app_d2dbs)
    add_subdirectory(d2dbs)
endif()
