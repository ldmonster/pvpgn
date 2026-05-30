// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		typedef struct {
			t_connection *c;
			unsigned lnews;
			unsigned fnews;
		} t_motd_data;

		int _news_cb(std::time_t date, t_lstr * lstr, void *data)
		{
			t_packet *rpacket;
			t_motd_data *motdd = (t_motd_data *)data;

			if (date < motdd->lnews)
				return -1;		/* exit traversing */

			// Strangler-fig: emit per-news SERVER_MOTD_W3 via the v3 codec.
			// `packet_append_lstr` writes (len - 1) text bytes + a NUL
			// terminator -- byte-identical to `write_cstring(lstr_get_str)`
			// when lstr is a normal NUL-terminated string (the only shape
			// produced by lstr_set_str in this codebase).
			{
				int v3rc = pvpgn_v3_send_motdw3(
				    motdd->c,
				    SERVER_MOTD_W3_MSGTYPE,
				    static_cast<unsigned int>(now),
				    static_cast<unsigned int>(motdd->fnews),
				    static_cast<unsigned int>(date),
				    static_cast<unsigned int>(date),
				    lstr_get_str(lstr));
				if (v3rc == 1) {
					return 0;
				}
			}

			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;

			packet_set_size(rpacket, sizeof(t_server_motd_w3));
			packet_set_type(rpacket, SERVER_MOTD_W3);

			bn_byte_set(&rpacket->u.server_motd_w3.msgtype, SERVER_MOTD_W3_MSGTYPE);
			bn_int_set(&rpacket->u.server_motd_w3.curr_time, now);

			bn_int_set(&rpacket->u.server_motd_w3.first_news_time, motdd->fnews);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp, date);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp2, date);

			/* Append news to packet, we used the already cached len in the lstr */
			packet_append_lstr(rpacket, lstr);

			/* Send news packet */
			conn_push_outqueue(motdd->c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		// motd for warcraft 3 (http://img21.imageshack.us/img21/1808/j2py.png)
		int _client_motdw3(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_handshake_dispatch(c, "motdw3");
			t_packet *rpacket;
			t_clienttag ctag;
			t_motd_data motdd;

			if (packet_get_size(packet) < sizeof(t_client_motd_w3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_MOTD_W3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_motd_w3), packet_get_size(packet));
				return -1;
			}

			ctag = conn_get_clienttag(c);
			/* if in a game, remove user from his game */
			if (conn_get_game(c) != NULL)
				conn_set_game(c, NULL, NULL, NULL, game_type_none, 0);

			/* News */
			motdd.lnews = bn_int_get(packet->u.client_motd_w3.last_news_time);
			motdd.fnews = news_get_firstnews();
			motdd.c = c;

			eventlog(eventlog_level_trace, __FUNCTION__, "lastnews() {} news_time {}", news_get_lastnews(), motdd.lnews);
			news_traverse(_news_cb, &motdd);

			/* Welcome Message */
			{
				int v3rc = pvpgn_v3_send_motdw3(
				    c,
				    SERVER_MOTD_W3_MSGTYPE,
				    static_cast<unsigned int>(now),
				    static_cast<unsigned int>(motdd.fnews),
				    static_cast<unsigned int>(motdd.fnews + 1),
				    SERVER_MOTD_W3_WELCOME,
				    nullptr);
				if (v3rc == 1) {
					return 0;
				}
			}
			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;

			packet_set_size(rpacket, sizeof(t_server_motd_w3));
			packet_set_type(rpacket, SERVER_MOTD_W3);

			//bn_int_set(&rpacket->u.server_motd_w3.ticks,get_ticks());
			bn_byte_set(&rpacket->u.server_motd_w3.msgtype, SERVER_MOTD_W3_MSGTYPE);
			bn_int_set(&rpacket->u.server_motd_w3.curr_time, now);
			bn_int_set(&rpacket->u.server_motd_w3.first_news_time, motdd.fnews);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp, motdd.fnews + 1);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp2, SERVER_MOTD_W3_WELCOME);


			// read text from bnmotd_w3.txt
			{
				std::string serverinfo;

				std::string filename = i18n_filename(prefs_v3::motdw3file(), conn_get_gamelang_localized(c));
				std::FILE* fp = std::fopen(filename.c_str(), "r");
				if (fp)
				{
					while (char* buff = file_get_line(fp))
					{
						char* line = message_format_line(c, buff);
						// R211: replaced fmt::memory_buffer + fmt::format_to
						// with std::string + std::format_to. The original
						// legacy code used pointer arithmetic on a literal
						// ("{}" + '\n'); the modern code is bounds-safe.
						std::format_to(std::back_inserter(serverinfo), "{}\n", (line + 1));
						delete[] line;
					}

					if (std::fclose(fp) == EOF)
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not close motdw3 file \"{}\" after reading (std::fopen: {})", filename, std::strerror(errno));
					}
				}
				packet_append_string(rpacket, serverinfo.c_str());
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		int _client_profilereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_profile_dispatch(c, "profilereq");
			t_packet *rpacket;
			int count;
			char const *username;
			t_account *account;
			t_clanmember *clanmember;
			t_clan *clan;
			bn_int clanTAG;

			if (packet_get_size(packet) < sizeof(t_client_profilereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROFILEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_profilereq), packet_get_size(packet));
				return -1;
			}

			count = bn_int_get(packet->u.client_profilereq.count);

			if (!(username = packet_get_str_const(packet, sizeof(t_client_profilereq), MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROFILEREQ (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_error, __FUNCTION__, "requested profile for non-existant account");
				return -1;
			}
			{
				unsigned int v3_clan_tag = 0u;
				if ((clanmember = account_get_clanmember(account)) && (clan = clanmember_get_clan(clanmember)))
					v3_clan_tag = static_cast<unsigned int>(clan_get_clantag(clan));
				int v3rc = pvpgn_v3_send_profilereply(
					c,
					static_cast<unsigned int>(count),
					0u,
					account_get_desc(account).c_str(),
					account_get_loc(account).c_str(),
					v3_clan_tag);
				if (v3rc == 1) {
					return 0;
				}
			}
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_profilereply));
				packet_set_type(rpacket, SERVER_PROFILEREPLY);
				bn_int_set(&rpacket->u.server_profilereply.count, count);
				bn_byte_set(&rpacket->u.server_profilereply.fail, 0);
				packet_append_string(rpacket, account_get_desc(account).c_str());
				packet_append_string(rpacket, account_get_loc(account).c_str());
				if ((clanmember = account_get_clanmember(account)) && (clan = clanmember_get_clan(clanmember)))
					bn_int_set(&clanTAG, clan_get_clantag(clan));
				else
					bn_int_set(&clanTAG, 0);
				packet_append_data(rpacket, clanTAG, 4);

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_adreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_ad_dispatch(c, "adreq");
			if (packet_get_size(packet) < sizeof(t_client_adreq))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adreq), packet_get_size(packet));
				return -1;
			}
			
			/*
			eventlog(eventlog_level_debug, __FUNCTION__, "[%d] SID_CHECKAD { %d %d %d %d }", conn_get_socket(c), bn_int_get(packet->u.client_adreq.archtag),
				bn_int_get(packet->u.client_adreq.clienttag), bn_int_get(packet->u.client_adreq.prev_adid), bn_int_get(packet->u.client_adreq.ticks));
			*/

			// R171.c/e + R188.b + R189: v3 ads dispatcher is
			// MANDATORY when compiled with PVPGN_V3_BNETD_INTEGRATION.
			// `install_ads_handlers()` is called unconditionally in
			// `server.cpp` so the bridge ABI returns -1 only if
			// startup wiring is broken (matches the R168.a init
			// pattern -- surface the bug, do NOT silently fall back
			// to legacy `AdBannerList.pick`).
			{
				PvpgnV3AdPickOut v3out{};
				int const rc = pvpgn_v3_ads_pick_apply(
				    static_cast<unsigned int>(conn_get_clienttag(c)),
				    static_cast<unsigned int>(conn_get_gamelang(c)),
				    bn_int_get(packet->u.client_adreq.prev_adid),
				    &v3out);
				if (rc < 0) {
					eventlog(eventlog_level_error, __FUNCTION__,
					    "[{}] v3 ads dispatcher not installed (rc={}); rejecting ADREQ",
					    conn_get_socket(c), rc);
					return -1;
				}
				if (v3out.found == 0) {
					return 0;
				}
				t_packet* const rpacket = packet_create(packet_class_bnet);
				if (!rpacket) {
					eventlog(eventlog_level_error, __FUNCTION__, "Could not create a packet");
					return -1;
				}
				packet_set_size(rpacket, sizeof(t_server_adreply));
				packet_set_type(rpacket, SERVER_ADREPLY);
				bn_int_set(&rpacket->u.server_adreply.adid, v3out.id);
				bn_int_set(&rpacket->u.server_adreply.extensiontag, v3out.extension_tag);
				file_to_mod_time(c, v3out.filename, &rpacket->u.server_adreply.timestamp);
				packet_append_string(rpacket, v3out.filename);
				packet_append_string(rpacket, v3out.url);
				if (pvpgn_v3_send_adreply(c,
				        v3out.id,
				        v3out.extension_tag,
				        bn_long_get(rpacket->u.server_adreply.timestamp),
				        v3out.filename,
				        v3out.url) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				return 0;
			}
			}

		int _client_adack(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_ad_dispatch(c, "adack");
			if (packet_get_size(packet) < sizeof(t_client_adack)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADACK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adack), packet_get_size(packet));
				return -1;
			}

			/*
			   {
			   char const * tname;

			   eventlog(eventlog_level_info,__FUNCTION__,"[%d] ad acknowledgement for adid 0x%04x from \"%s\"",conn_get_socket(c),bn_int_get(packet->u.client_adack.adid),(tname = conn_get_chatname(c)));
			   conn_unget_chatname(c,tname);
			   }
			   */
			return 0;
		}

		int _client_adclick(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_ad_dispatch(c, "adclick");
			if (packet_get_size(packet) < sizeof(t_client_adclick)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADCLICK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adclick), packet_get_size(packet));
				return -1;
			}

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] ad click for adid 0x{:04x} from \"{}\"", conn_get_socket(c), bn_int_get(packet->u.client_adclick.adid), conn_get_username(c));

			return 0;
		}

		int _client_adclick2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_ad_dispatch(c, "adclick2");
			if (packet_get_size(packet) < sizeof(t_client_adclick2))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADCLICK2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adclick2), packet_get_size(packet));
				return -1;
			}

			eventlog(eventlog_level_trace, __FUNCTION__, "[{}] ad click2 for adid 0x{:04x} from \"{}\"", conn_get_socket(c), bn_int_get(packet->u.client_adclick2.adid), conn_get_username(c));

			// R171.c/e + R188.b + R189: v3 click dispatcher MANDATORY
			// under PVPGN_V3_BNETD_INTEGRATION (see comment above in
			// `_client_adreq`).
			{
				PvpgnV3AdClickOut v3out{};
				int const rc = pvpgn_v3_ads_click_apply(
				    static_cast<unsigned int>(conn_get_clienttag(c)),
				    static_cast<unsigned int>(conn_get_gamelang(c)),
				    bn_int_get(packet->u.client_adclick2.adid),
				    &v3out);
				if (rc < 0) {
					eventlog(eventlog_level_error, __FUNCTION__,
					    "[{}] v3 ads dispatcher not installed (rc={}); rejecting ADCLICK2",
					    conn_get_socket(c), rc);
					return -1;
				}
				if (v3out.accepted == 0) return 0;
				t_packet* const rpacket = packet_create(packet_class_bnet);
				if (!rpacket) {
					eventlog(eventlog_level_error, __FUNCTION__, "Could not create a packet");
					return -1;
				}
				packet_set_size(rpacket, sizeof(t_server_adclickreply2));
				packet_set_type(rpacket, SERVER_ADCLICKREPLY2);
				bn_int_set(&rpacket->u.server_adclickreply2.adid, v3out.id);
				packet_append_string(rpacket, v3out.click_url);
				if (pvpgn_v3_send_adclick2reply(c, v3out.id, v3out.click_url) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				return 0;
			}
		}

		int _client_statsupdate(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_profile_dispatch(c, "statsupdate");
			if (packet_get_size(packet) < sizeof(t_client_statsupdate)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSUPDATE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_statsupdate), packet_get_size(packet));
				return -1;
			}

			{
				char const *name;
				char const *key;
				char const *val;
				unsigned int name_count;
				unsigned int key_count;
				unsigned int i, j;
				unsigned int name_off;
				unsigned int keys_off;
				unsigned int key_off;
				unsigned int vals_off;
				unsigned int val_off;
				t_account *account;

				name_count = bn_int_get(packet->u.client_statsupdate.name_count);
				key_count = bn_int_get(packet->u.client_statsupdate.key_count);

				if (name_count != 1)
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious STATSUPDATE packet (name_count={})", conn_get_socket(c), name_count);

				for (i = 0, name_off = sizeof(t_client_statsupdate); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1);
				if (i < name_count) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSUPDATE packet (only {} names of {})", conn_get_socket(c), i, name_count);
					return -1;
				}
				keys_off = name_off;

				for (i = 0, key_off = keys_off; i < key_count && (key = packet_get_str_const(packet, key_off, MAX_ATTRKEY_STR)); i++, key_off += std::strlen(key) + 1);
				if (i < key_count) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSUPDATE packet (only {} keys of {})", conn_get_socket(c), i, key_count);
					return -1;
				}
				vals_off = key_off;

				if ((account = conn_get_account(c))) {
					if (account_get_auth_changeprofile(account) == 0) {	/* default to true */
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] stats update for \"{}\" refused (no change profile access)", conn_get_socket(c), conn_get_username(c));
						return -1;
					}
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] updating player profile for \"{}\"", conn_get_socket(c), conn_get_username(c));

					for (i = 0, name_off = sizeof(t_client_statsupdate); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1)
					for (j = 0, key_off = keys_off, val_off = vals_off; j < key_count && (key = packet_get_str_const(packet, key_off, MAX_ATTRKEY_STR)) && (val = packet_get_str_const(packet, val_off, MAX_ATTRVAL_STR)); j++, key_off += std::strlen(key) + 1, val_off += std::strlen(val) + 1)
					if (std::strlen(key) < 9 || strncasecmp(key, "profile\\", 8) != 0)
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got STATSUPDATE with suspicious key \"{}\" value \"{}\"", conn_get_socket(c), key, val);
					else
						account_set_strattr(account, key, val);
				}
			}

			return 0;
		}

		int _client_playerinforeq(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_playerinforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PLAYERINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_playerinforeq), packet_get_size(packet));
				return -1;
			}

			{
				char const *username;
				char const *info;
				t_account *account;

				if (!(username = packet_get_str_const(packet, sizeof(t_client_playerinforeq), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PLAYERINFOREQ (missing or too long username)", conn_get_socket(c));
					return -1;
				}
				if (!(info = packet_get_str_const(packet, sizeof(t_client_playerinforeq)+std::strlen(username) + 1, MAX_PLAYERINFO_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PLAYERINFOREQ (missing or too long info)", conn_get_socket(c));
					return -1;
				}

				if (info[0] != '\0')
					conn_set_playerinfo(c, info);
				if (!username[0])
					username = conn_get_loggeduser(c);

				account = conn_get_account(c);

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_playerinforeply));
				packet_set_type(rpacket, SERVER_PLAYERINFOREPLY);
	
				if (account) {
					packet_append_string(rpacket, username);
					packet_append_string(rpacket, conn_get_playerinfo(c));
					packet_append_string(rpacket, username);
				}
				else {
					packet_append_string(rpacket, "");
					packet_append_string(rpacket, "");
					packet_append_string(rpacket, "");
				}
				{
					char const* v3_acct = account ? username : "";
					char const* v3_info = account ? conn_get_playerinfo(c) : "";
					char const* v3_user = account ? username : "";
					if (pvpgn_v3_send_playerinforeply(c,
					        v3_acct,
					        v3_info,
					        v3_user) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_progident2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_progident_dispatch(c, "progident2");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_progident2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROGIDENT2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_progident2), packet_get_size(packet));
				return -1;
			}

			/* d2 uses this packet with clienttag = 0 to request the channel list */
			if (bn_int_get(packet->u.client_progident2.clienttag)) {
				if (tag_check_in_list(bn_int_get(packet->u.client_progident2.clienttag), prefs_v3::allowed_clients())) {
					conn_set_state(c, conn_state_destroy);
					return 0;
				}
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] CLIENT_PROGIDENT2 clienttag=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_progident2.clienttag));

				/* Hmm... no archtag.  Hope we get it in CLIENT_AUTHREQ1 (but we won't if we use the shortcut) */

				conn_set_clienttag(c, bn_int_get(packet->u.client_progident2.clienttag));
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_channellist));
				packet_set_type(rpacket, SERVER_CHANNELLIST);
				{
					for (t_channel const* ch : channellist()) {
						if ((!(channel_get_flags(ch) & channel_flags_clan)) && (!prefs_v3::hide_temp_channels() || channel_get_permanent(ch)) && (!channel_get_clienttag(ch) || channel_get_clienttag(ch) == conn_get_clienttag(c)) && (!(channel_get_flags(ch) & channel_flags_thevoid)) &&	// don't display theVoid in channel list
							((channel_get_max(ch) != 0) || ((channel_get_max(ch) == 0) && (account_is_operator_or_admin(conn_get_account(c), channel_get_name(ch)) == 1))))	// don't display restricted channel for no admins/ops
							packet_append_string(rpacket, channel_get_name(ch));
					}
				}
				packet_append_string(rpacket, "");
				{
					// Re-collect names from the freshly-built rpacket so
					// all filter logic above stays in legacy. Walk the
					// NUL-terminated cstrings starting after the fixed
					// header until we hit the empty terminator.
					std::vector<char const*> v3_names;
					std::size_t v3_off = sizeof(t_server_channellist);
					std::size_t v3_end = packet_get_size(rpacket);
					while (v3_off < v3_end) {
						char const* s = reinterpret_cast<char const*>(
							packet_get_data_const(rpacket, v3_off, 1));
						if (s == nullptr || *s == '\0') break;
						v3_names.push_back(s);
						v3_off += std::strlen(s) + 1;
					}
					if (pvpgn_v3_send_channellist(
						    c,
						    v3_names.empty() ? nullptr : v3_names.data(),
						    static_cast<unsigned int>(v3_names.size())) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_changeclient(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_progident_dispatch(c, "changeclient");
			if (packet_get_size(packet) < sizeof(t_client_changeclient))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_CHANGECLIENT packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_changeclient), packet_get_size(packet));
				return -1;
			}

			if (conn_get_clienttag(c) != CLIENTTAG_WAR3XP_UINT
				|| bn_int_get(packet->u.client_changeclient.clienttag) != CLIENTTAG_WARCRAFT3_UINT)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] invalid attempt to change client from {X} to {X}", conn_get_socket(c), conn_get_clienttag(c), bn_int_get(packet->u.client_changeclient.clienttag));
				conn_set_state(c, conn_state_destroy);
				return -1;
			}

			conn_set_clienttag(c, bn_int_get(packet->u.client_changeclient.clienttag));

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] changed client to {X}", conn_get_socket(c), bn_int_get(packet->u.client_changeclient.clienttag));

			return 0;
		}

		int _client_crashdump(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_telemetry_dispatch(c, "crashdump");
			return 0;
		}

		int _client_setemailreply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch(c, "setemailreply");
			char const *email;
			t_account *account;

			if (!(email = packet_get_str_const(packet, sizeof(t_client_setemailreply), MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad SETEMAILREPLY packet", conn_get_socket(c));
				return -1;
			}
			if (!(account = conn_get_account(c))) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL account for connection in setemail request");
				return -1;
			}
			if (account_get_email(account)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" already have email set, ignore set email", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (account_set_email(account, email) < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] failed to init account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), email);
				return 0;
			}
			else
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] init account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), email);
			return 0;
		}

		int _client_changeemailreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch(c, "changeemailreq");
			char const *oldaddr;
			char const *newaddr;
			char const *username;
			char const *email;
			t_account *account;
			int pos;

			pos = sizeof(t_client_changeemailreq);
			if (!(username = packet_get_str_const(packet, pos, MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad username in CHANGEEMAILREQ packet", conn_get_socket(c));
				return -1;
			}
			pos += (std::strlen(username) + 1);
			if (!(oldaddr = packet_get_str_const(packet, pos, MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad old email in CHANGEEMAILREQ packet", conn_get_socket(c));
				return -1;
			}
			pos += (std::strlen(oldaddr) + 1);
			if (!(newaddr = packet_get_str_const(packet, pos, MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad new email in CHANGEEMAILREQ packet", conn_get_socket(c));
				return -1;
			}
			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] change email for \"{}\" refused (no such account)", conn_get_socket(c), username);
				return 0;
			}
			if (!(email = account_get_email(account)) || !email[0]) {
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] account \"{}\" do not have email set, ignore changing", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (strcasecmp(email, oldaddr)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" email mismatch, ignore changing", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (account_set_email(account, newaddr) < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] failed to change account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), newaddr);
				return 0;
			}
			else
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] change account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), newaddr);
			return 0;
		}

		int _client_getpasswordreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_passemail_dispatch(c, "getpasswordreq");
			char const *username;
			char const *try_email;
			char const *email;
			t_account *account;
			int pos;

			pos = sizeof(t_client_getpasswordreq);
			if (!(username = packet_get_str_const(packet, pos, MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad username in GETPASSWORDREQ packet", conn_get_socket(c));
				return -1;
			}
			pos += (std::strlen(username) + 1);
			if (!(try_email = packet_get_str_const(packet, pos, MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad email in GETPASSWORDREQ packet", conn_get_socket(c));
				return -1;
			}
			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] get password for \"{}\" refused (no such account)", conn_get_socket(c), username);
				return 0;
			}
			if (!(email = account_get_email(account)) || !email[0]) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" do not have email set, ignore get password", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (strcasecmp(email, try_email)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" email mismatch, ignore get password", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			/* TODO: send mail to user with the real password or changed password!?
			 * (as we cannot get the real password back, we should only change the password)     --Soar */
			eventlog(eventlog_level_info, __FUNCTION__, "[{}] get password for account \"{}\" to email \"{}\"", conn_get_socket(c), account_get_name(account), email);
			return 0;
		}

		int _client_extrawork(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_telemetry_dispatch(c, "extrawork");
			if (packet_get_size(packet) < sizeof(t_client_extrawork)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad EXTRAWORK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_extrawork), packet_get_size(packet));
				return -1;
			}
			{
				short gametype;
				short length;
				const char * data;
				std::string data_s;

				gametype = bn_short_get(packet->u.client_extrawork.gametype);
				length = bn_short_get(packet->u.client_extrawork.length);

				if (!(data = (const char *)packet_get_raw_data_const(packet, sizeof(t_client_extrawork)))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad EXTRAWORK packet (missing or too long data)", conn_get_socket(c));
					return -1;
				}
				// extract substring with given length
				data_s = std::string(data).substr(0, length);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] Received EXTRAWORK packet with GameType: {} and Length: {} ({})", conn_get_socket(c), gametype, length, data_s.c_str());
			
	#ifdef WITH_LUA
				lua_handle_client_extrawork(c, gametype, length, data_s.c_str());
	#endif
			}
			return 0;
		}


}} // namespace pvpgn::bnetd
