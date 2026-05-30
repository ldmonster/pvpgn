// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_changegameport(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_gameport_dispatch_try(c, "changegameport");
			if (packet_get_size(packet) < sizeof(t_client_changegameport)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad changegameport packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_changegameport), packet_get_size(packet));
				return -1;
			}
			{
				unsigned short port = bn_short_get(packet->u.client_changegameport.port);
				if (port < 1024) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] invalid port in changegameport packet: {}", conn_get_socket(c), (int)port);
					return -1;
				}

				conn_set_game_port(c, port);
			}

			return 0;
		}

		int _client_joinchannel(t_connection * c, t_packet const *const packet)
		{
			t_account *account;
			char const *cname;
			int found = 1;
			t_clan *user_clan;
			t_clantag clantag;
			std::uint32_t clienttag;
			t_channel *channel;

			if (packet_get_size(packet) < sizeof(t_client_joinchannel)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad JOINCHANNEL packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_joinchannel), packet_get_size(packet));
				return -1;
			}

			account = conn_get_account(c);

			if (!(cname = packet_get_str_const(packet, sizeof(t_client_joinchannel), MAX_CHANNELNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad JOINCHANNEL (missing or too long cname)", conn_get_socket(c));
				return -1;
			}

			// Observation-only: structured-log the join intent so the
			// v3 telemetry layer sees every JOINCHANNEL. Bridge always
			// returns 0 -- legacy retains full ownership of the
			// channel-state side effects.
			(void)pvpgn_v3_joinchannel_try(
				c, cname,
				static_cast<unsigned int>(bn_int_get(
					packet->u.client_joinchannel.channelflag)));

			if ((channel = conn_get_channel(c)) && (strcasecmp(channel_get_name(channel), cname) == 0))
				return 0;		//we are already in this channel

			std::string tmpstr;
			clienttag = conn_get_clienttag(c);
			if ((clienttag == CLIENTTAG_WARCRAFT3_UINT) || (clienttag == CLIENTTAG_WAR3XP_UINT)) {
				conn_update_w3_playerinfo(c);
				switch (bn_int_get(packet->u.client_joinchannel.channelflag)) {
				case CLIENT_JOINCHANNEL_NORMAL:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_NORMAL channel \"{}\"", conn_get_socket(c), cname);

					if (prefs_v3::ask_new_channel() && (!(channellist_find_channel_by_name(cname, conn_get_country(c), realm_get_name(conn_get_realm(c)))))) {
						found = 0;
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] didn't find channel \"{}\" to join", conn_get_socket(c), cname);
						message_send_text(c, message_type_channeldoesnotexist, c, cname);
					}
					break;
				case CLIENT_JOINCHANNEL_GENERIC:

					if ((user_clan = account_get_clan(account)) && (clantag = clan_get_clantag(user_clan)))
					{
						std::ostringstream ostr;
						ostr << "Clan " << clantag_to_str(clantag);
						tmpstr = ostr.str();
						cname = tmpstr.c_str();
					}
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_GENERIC channel \"{}\"", conn_get_socket(c), cname);

					/* don't have to do anything here */
					break;
				case CLIENT_JOINCHANNEL_CREATE:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_CREATE channel \"{}\"", conn_get_socket(c), cname);
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_CREATE channel \"{}\"", conn_get_socket(c), cname);
					/* don't have to do anything here */
					break;
				}

				if (found && conn_set_channel(c, cname) < 0)
					conn_set_channel(c, CHANNEL_NAME_BANNED);	/* should not fail */
			}
			else {

				// not W3
				if (conn_set_channel(c, cname) < 0)
					conn_set_channel(c, CHANNEL_NAME_BANNED);	/* should not fail */
			}
			// here we set channel flags on user
			channel_set_userflags(c);

			return 0;
		}

		int _client_message(t_connection * c, t_packet const *const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_message)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MESSAGE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_message), packet_get_size(packet));
				return -1;
			}

			// v3 strangler-fig: observe the chat input via the typed
			// `application::chat::classify_chat_command` seam. Today
			// the bridge always returns 0 (legacy stays in charge);
			// a follow-up batch will start consuming the
			// classification (drop Empty early, route Whisper through
			// a v3 use-case, etc).
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(
					packet, sizeof(t_client_message),
					sz - sizeof(t_client_message));
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(chat_command, c, bd,
						sz - sizeof(t_client_message));
				}
			}

			{
				char const *text;
				t_channel const *channel;

				if (!(text = packet_get_str_const(packet, sizeof(t_client_message), MAX_MESSAGE_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MESSAGE (missing or too long text)", conn_get_socket(c));
					return -1;
				}

				conn_set_idletime(c);

				if ((channel = conn_get_channel(c)))
					channel_message_log(channel, c, 1, text);
				/* we don't log game commands currently */



				if (text[0] == '/')
					handle_command(c, text);
				else if (channel && !conn_quota_exceeded(c, text))
					channel_message_send(channel, message_type_talk, c, text);
				/* else discard */
			}

			return 0;
		}

		int _client_leavechannel(t_connection * c, t_packet const *const packet)
		{
			// Observation-only: structured-log the leave intent.
			(void)pvpgn_v3_leavechannel_try(c);
			/* If this user in a channel, notify everyone that the user has left */
			if (conn_get_channel(c))
				conn_part_channel(c);
			return 0;
		}


}} // namespace pvpgn::bnetd
