# src/v3/services/CMakeLists.txt
#
# Service composition roots — each subdirectory defines a static library
# that wires infrastructure adapters together for a specific server process.
#
# Subdirectories
# --------------
#   bnetd    — BnetdService (composition root for the bnetd server)
#   d2cs     — D2csComposition (composition root for the D2CS realm server)
#   d2dbs    — D2dbsComposition (composition root for the D2DBS database server)
#   combined — CombinedComposition (single-binary mode, optional)

add_subdirectory(bnetd)
add_subdirectory(d2cs)
add_subdirectory(d2dbs)
add_subdirectory(combined)
