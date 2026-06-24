set(COMMON_SOURCES
	# (2026-06-02): the legacy `*_protocol.h` wire-format headers
	# AND the legacy packet utilities (packet*.cpp/.h, field_sizes.h, lstr.h,
	# d2char_checksum.*) were DELETED — all dead (uncompilable / zero v3
	# consumers) and superseded by the native protocol layer:
	#   wire types   → src/protocol/<family>/include/protocol/<family>/
	#   packet codec → src/protocol/common/ (the built `protocol_common` lib:
	#                  packet.hpp / reader.hpp / writer.hpp / replay.hpp / ...)
	# What remains here is the setup compatibility shim + the deferred
	# crypto (to migrate to src/infra/crypto/) and the vendored pugixml copy.

	# Setup headers (compatibility layer - still included by win32/* and
	# infra/legacy_crypto; kept until those callers retire)
	setup_before.h setup_after.h

	# (2026-06-02): src/common no longer implements crypto.
	# The legacy bnet_hash / SRP-3 / wolhash / bigint cluster (and its dead
	# infra_legacy_crypto adapter + parity tests) were DELETED; bnettime had
	# already moved to src/core/time/. No crypto remains here. NOTE:
	# the `common` target is added by no build, so this list is vestigial.

	# pugixml vendored copy (moved from src/common/ to vendor/pugixml/)
	${CMAKE_SOURCE_DIR}/vendor/pugixml/pugixml.cpp
	${CMAKE_SOURCE_DIR}/vendor/pugixml/pugiconfig.h
	${CMAKE_SOURCE_DIR}/vendor/pugixml/pugixml.h)

add_library(common STATIC ${COMMON_SOURCES})

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor/pugixml)

target_link_libraries(common PRIVATE fmt)
