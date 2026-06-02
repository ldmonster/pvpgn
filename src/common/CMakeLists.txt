set(COMMON_SOURCES
	# Plan 02 Phase 2 (2026-06-02): the legacy `*_protocol.h` wire-format headers
	# AND the legacy packet utilities (packet*.cpp/.h, field_sizes.h, lstr.h,
	# d2char_checksum.*) were DELETED — all dead (uncompilable / zero v3
	# consumers) and superseded by the native protocol layer:
	#   wire types   → src/protocol/<family>/include/protocol/<family>/
	#   packet codec → src/protocol/common/ (the built `protocol_common` lib:
	#                  packet.hpp / reader.hpp / writer.hpp / replay.hpp / ...)
	# What remains here is the setup compatibility shim + the Plan-08-deferred
	# crypto (to migrate to src/infra/crypto/) and the vendored pugixml copy.

	# Setup headers (compatibility layer - still included by win32/* and
	# infra/legacy_crypto; kept until those callers retire)
	setup_before.h setup_after.h

	# Crypto files (Plan 08 - deferred to crypto modernization)
	# These will be moved to src/infra/crypto/ in Plan 08
	bnethashconv.cpp bnethashconv.h bnethash.cpp bnethash.h
	bnettime.cpp bnettime.h
	bnetsrp3.cpp bnetsrp3.h bigint.cpp bigint.h
	wolhash.cpp wolhash.h
	
	# pugixml vendored copy (moved from src/common/ to vendor/pugixml/)
	${CMAKE_SOURCE_DIR}/vendor/pugixml/pugixml.cpp
	${CMAKE_SOURCE_DIR}/vendor/pugixml/pugiconfig.h
	${CMAKE_SOURCE_DIR}/vendor/pugixml/pugixml.h)

add_library(common STATIC ${COMMON_SOURCES})

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor/pugixml)

target_link_libraries(common PRIVATE fmt)
