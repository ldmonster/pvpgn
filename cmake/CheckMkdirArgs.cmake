# Check mkdir arguments support
# This module checks if mkdir supports the -p flag

include(CheckCSourceCompiles)

# Check if mkdir supports -p flag (create parent directories)
check_c_source_compiles("
#include <sys/stat.h>
#include <sys/types.h>
int main() {
    mkdir(\"test\", 0755);
    return 0;
}
" HAVE_MKDIR)

if(HAVE_MKDIR)
    set(MKDIR_SUPPORTS_P TRUE)
else()
    set(MKDIR_SUPPORTS_P FALSE)
endif()
