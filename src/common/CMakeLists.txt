set(COMMON_SOURCES
	# Wire protocol headers and packet utilities (Plan 02 - kept in src/common/)
	# These are true wire-protocol constants and packet handling utilities
	# that are still needed by legacy code and v3 protocol decoders.
	# Future: Move to src/protocol/<family>/include/ when consumers are updated.
	
	# Packet handling (core wire protocol utilities)
	packet_buffer.cpp packet_writer.cpp packet_reader.cpp
	packet.cpp packet.h
	
	# Wire protocol headers (Battle.net, D2, IRC, UDP, File, WOL)
	anongame_protocol.h bnet_protocol.h bot_protocol.h init_protocol.h
	irc_protocol.h udp_protocol.h file_protocol.h wol_gameres_protocol.h
	
	# D2 protocol headers
	d2char_checksum.cpp d2char_checksum.h d2char_file.h
	d2cs_bnetd_protocol.h d2cs_d2dbs_ladder.h d2cs_d2gs_character.h
	d2cs_d2gs_protocol.h d2cs_protocol.h d2game_protocol.h
	
	# Wire protocol field definitions and utilities
	field_sizes.h lstr.h tracker.h
	
	# Setup headers (compatibility layer - Plan 02 kept in src/common/)
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
