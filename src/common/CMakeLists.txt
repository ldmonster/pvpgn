set(COMMON_SOURCES
	# addr.cpp split into focused sub-modules (plan 15 §3 / SOLID-S)
	addr_core.cpp addr_netaddr.cpp addr_list.cpp
	addr.cpp addr.h anongame_protocol.h
	bnethashconv.cpp bnethashconv.h bnethash.cpp bnethash.h bnet_protocol.h
	bnettime.cpp bnettime.h bn_type.cpp bn_type.h bot_protocol.h conf.cpp
	conf.h d2char_checksum.cpp d2char_checksum.h d2char_file.h
	d2cs_bnetd_protocol.h d2cs_d2dbs_ladder.h d2cs_d2gs_character.h
	d2cs_d2gs_protocol.h d2cs_protocol.h d2game_protocol.h elist.h
	eventlog.cpp eventlog.h fdwatch.cpp fdwatch_epoll.cpp fdwatch_epoll.h
	fdwatch.h fdwatch_kqueue.cpp fdwatch_kqueue.h fdwatch_poll.cpp
	fdwatch_poll.h fdwatch_select.cpp fdwatch_select.h fdwbackend.cpp
	fdwbackend.h field_sizes.h file_protocol.h flags.h
	give_up_root_privileges.cpp give_up_root_privileges.h hashtable.cpp
	hashtable.h hash_tuple.hpp hexdump.cpp hexdump.h init_protocol.h introtate.h
	irc_protocol.h list.cpp list.h lstr.h network.cpp network.h
	# packet.cpp split into focused sub-modules (plan 15 §3 / SOLID-S)
	packet_buffer.cpp packet_writer.cpp packet_reader.cpp
	packet.cpp packet.h proginfo.cpp proginfo.h queue.cpp queue.h rcm.cpp rcm.h
	rlimit.cpp rlimit.h setup_after.h
	setup_before.h systemerror.cpp systemerror.h
	# tag.cpp split into focused sub-modules (plan 15 §3 / SOLID-S)
	tag_core.cpp tag_clienttag.cpp tag_wol.cpp
	tag.cpp tag.h token.cpp
	token.h tracker.h trans.cpp trans.h udp_protocol.h
	# util.cpp split into focused sub-modules (plan 15 §3 / SOLID-S)
	util_string.cpp util_file.cpp util_hex.cpp util_time.cpp
	util.cpp util.h
	version.h wolhash.cpp wolhash.h
	xstring.cpp xstring.h gui_printf.h gui_printf.cpp
	bigint.cpp bigint.h bnetsrp3.cpp bnetsrp3.h peerchat.cpp peerchat.h
    wol_gameres_protocol.h
    # pugixml vendored copy (moved from src/common/ to vendor/pugixml/)
    ${CMAKE_SOURCE_DIR}/vendor/pugixml/pugixml.cpp
    ${CMAKE_SOURCE_DIR}/vendor/pugixml/pugiconfig.h
    ${CMAKE_SOURCE_DIR}/vendor/pugixml/pugixml.h)

add_library(common STATIC ${COMMON_SOURCES})

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor/pugixml)

target_link_libraries(common PRIVATE fmt)