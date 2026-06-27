# src/v3/services/CMakeLists.txt
#
# Service composition roots — each subdirectory defines a static library
# that wires infrastructure adapters together for a specific server process.
#
# Subdirectories
# --------------
#   bnetd    — BnetdService (composition root for the bnetd server)
#   combined — CombinedComposition (single-binary mode, optional)

add_subdirectory(bnetd)
add_subdirectory(combined)
