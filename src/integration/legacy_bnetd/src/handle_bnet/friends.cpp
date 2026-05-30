// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_friendslistreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_friends_dispatch(c, "friendslistreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_friendslistreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FRIENDSLISTREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_friendslistreq), packet_get_size(packet));
				return -1;
			}
			{
				int frienduid;
				t_friend *fr;
				t_account *account = conn_get_account(c);
				int i;
				int n = account_get_friendcount(account);
				int friendcount = 0;
				t_server_friendslistreply_status status;
				t_connection *dest_c;
				t_game *game;
				t_channel *channel;
				char stat;
	
				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
	
				packet_set_size(rpacket, sizeof(t_server_friendslistreply));
				packet_set_type(rpacket, SERVER_FRIENDSLISTREPLY);
	
				auto& flist = account_get_friends(account);

				for (i = 0; i < n; i++) {
					frienduid = account_get_friend(account, i);
					if ((fr = friendlist_find_uid(flist, frienduid)) == NULL)
						continue;
					packet_append_string(rpacket, account_get_name(friend_get_account(fr)));
					game = NULL;
					channel = NULL;

					if (!(dest_c = connlist_find_connection_by_uid(frienduid))) {
						bn_byte_set(&status.location, FRIENDSTATUS_OFFLINE);
						bn_byte_set(&status.status, 0);
						bn_int_set(&status.clienttag, 0);
					}
					else {
						bn_int_set(&status.clienttag, conn_get_clienttag(dest_c));
						stat = 0;
						if ((friend_get_mutual(fr)))
							stat |= FRIEND_TYPE_MUTUAL;
						if ((conn_get_dndstr(dest_c)))
							stat |= FRIEND_TYPE_DND;
						if ((conn_get_awaystr(dest_c)))
							stat |= FRIEND_TYPE_AWAY;
						bn_byte_set(&status.status, stat);
						if ((game = conn_get_game(dest_c))) {
							if (game_get_flag(game) != game_flag_private)
								bn_byte_set(&status.location, FRIENDSTATUS_PUBLIC_GAME);
							else
								bn_byte_set(&status.location, FRIENDSTATUS_PRIVATE_GAME);
						}
						else if ((channel = conn_get_channel(dest_c))) {
							bn_byte_set(&status.location, FRIENDSTATUS_CHAT);
						}
						else {
							bn_byte_set(&status.location, FRIENDSTATUS_ONLINE);
						}
					}

					packet_append_data(rpacket, &status, sizeof(status));

					if (game)
						packet_append_string(rpacket, game_get_name(game));
					else if (channel)
						packet_append_string(rpacket, channel_get_name(channel));
					else
						packet_append_string(rpacket, "");

					friendcount++;
					}
	
					bn_byte_set(&rpacket->u.server_friendslistreply.friendcount, friendcount);
	
					{
						// Collect entries from the legacy-built packet payload and
						// ship via v3 codec.  The payload starts after the fixed
						// header (sizeof(t_server_friendslistreply) bytes) and
						// consists of repeated (NUL-terminated name, 6-byte status,
						// NUL-terminated location_name) tuples.
						std::vector<pvpgn_v3_friend_entry> v3_entries;
						v3_entries.reserve(static_cast<std::size_t>(friendcount));
	
						// Re-walk the friend list in the same order to rebuild the
						// C-ABI entry array.
						if (!flist.empty()) {
							for (int v3i = 0; v3i < n; v3i++) {
								int v3uid = account_get_friend(account, v3i);
								t_friend *v3fr = friendlist_find_uid(flist, v3uid);
								if (!v3fr) continue;
	
								pvpgn_v3_friend_entry v3e{};
								v3e.username = account_get_name(friend_get_account(v3fr));
	
								t_connection *v3dc = connlist_find_connection_by_uid(v3uid);
								if (!v3dc) {
									v3e.status     = 0;
									v3e.location   = FRIENDSTATUS_OFFLINE;
									v3e.client_tag = 0;
									v3e.location_name = "";
								} else {
									v3e.client_tag = static_cast<unsigned int>(conn_get_clienttag(v3dc));
									char v3stat = 0;
									if (friend_get_mutual(v3fr))       v3stat |= FRIEND_TYPE_MUTUAL;
									if (conn_get_dndstr(v3dc))         v3stat |= FRIEND_TYPE_DND;
									if (conn_get_awaystr(v3dc))        v3stat |= FRIEND_TYPE_AWAY;
									v3e.status = static_cast<unsigned char>(v3stat);
									t_game    *v3g = conn_get_game(v3dc);
									t_channel *v3ch = conn_get_channel(v3dc);
									if (v3g) {
										v3e.location = (game_get_flag(v3g) != game_flag_private)
											? FRIENDSTATUS_PUBLIC_GAME : FRIENDSTATUS_PRIVATE_GAME;
										v3e.location_name = game_get_name(v3g);
									} else if (v3ch) {
										v3e.location      = FRIENDSTATUS_CHAT;
										v3e.location_name = channel_get_name(v3ch);
									} else {
										v3e.location      = FRIENDSTATUS_ONLINE;
										v3e.location_name = "";
									}
								}
								v3_entries.push_back(v3e);
							}
						}
	
						int v3rc = pvpgn_v3_send_friendslistreply(
							c,
							v3_entries.empty() ? nullptr : v3_entries.data(),
							static_cast<unsigned int>(v3_entries.size()));
						if (v3rc == 1) {
							packet_del_ref(rpacket);
							return 0;
						}
					}
	
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_friendinforeq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_friends_dispatch(c, "friendinforeq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_friendinforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FRIENDINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_friendinforeq), packet_get_size(packet));
				return -1;
			}

			{
				t_connection const *dest_c;
				t_game const *game;
				t_channel const *channel;
				t_account *account = conn_get_account(c);
				int frienduid;
				t_friend *fr;
				int n = account_get_friendcount(account);
				char type;

				if (n == 0)
					return 0;

				if (bn_byte_get(packet->u.client_friendinforeq.friendnum) > n) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] bad friend number in FRIENDINFOREQ packet", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;

				packet_set_size(rpacket, sizeof(t_server_friendinforeply));
				packet_set_type(rpacket, SERVER_FRIENDINFOREPLY);

				frienduid = account_get_friend(account, bn_byte_get(packet->u.client_friendinforeq.friendnum));
				if (frienduid < 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] friend number {} not found", conn_get_socket(c), (int)bn_byte_get(packet->u.client_friendinforeq.friendnum));
					return -1;
				}

				bn_byte_set(&rpacket->u.server_friendinforeply.friendnum, bn_byte_get(packet->u.client_friendinforeq.friendnum));

				auto& flist2 = account_get_friends(account);
				fr = friendlist_find_uid(flist2, frienduid);

				if (fr == NULL || (dest_c = connlist_find_connection_by_account(friend_get_account(fr))) == NULL) {
					bn_byte_set(&rpacket->u.server_friendinforeply.type, FRIEND_TYPE_NON_MUTUAL);
					bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_OFFLINE);
					bn_int_set(&rpacket->u.server_friendinforeply.clienttag, 0);
					packet_append_string(rpacket, "");
					{
						int v3rc = pvpgn_v3_send_friendinforeply(
							c,
							static_cast<unsigned int>(bn_byte_get(packet->u.client_friendinforeq.friendnum)),
							static_cast<unsigned int>(FRIEND_TYPE_NON_MUTUAL),
							static_cast<unsigned int>(FRIENDSTATUS_OFFLINE),
							0u,
							"");
						if (v3rc == 1) {
							packet_del_ref(rpacket);
							return 0;
						}
					}
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
					return 0;
				}

				type = FRIEND_TYPE_NON_MUTUAL;
				if (friend_get_mutual(fr))
					type |= FRIEND_TYPE_MUTUAL;
				if ((conn_get_dndstr(dest_c)))
					type |= FRIEND_TYPE_DND;
				if ((conn_get_awaystr(dest_c)))
					type |= FRIEND_TYPE_AWAY;
				bn_byte_set(&rpacket->u.server_friendinforeply.type, type);
				if ((game = conn_get_game(dest_c))) {
					if (game_get_flag(game) != game_flag_private)
						bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_PUBLIC_GAME);
					else
						bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_PRIVATE_GAME);
					packet_append_string(rpacket, game_get_name(game));
				}
				else if ((channel = conn_get_channel(dest_c))) {
					bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_CHAT);
					packet_append_string(rpacket, channel_get_name(channel));
				}
				else {
					bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_ONLINE);
					packet_append_string(rpacket, "");
				}

				bn_int_set(&rpacket->u.server_friendinforeply.clienttag, conn_get_clienttag(dest_c));
	
					{
						unsigned int v3_type   = static_cast<unsigned int>(bn_byte_get(rpacket->u.server_friendinforeply.type));
						unsigned int v3_status = static_cast<unsigned int>(bn_byte_get(rpacket->u.server_friendinforeply.status));
						unsigned int v3_ctag   = static_cast<unsigned int>(bn_int_get(rpacket->u.server_friendinforeply.clienttag));
						unsigned int v3_fixed  = static_cast<unsigned int>(sizeof(t_server_friendinforeply));
						unsigned int v3_psize  = packet_get_size(rpacket);
						char const  *v3_gname  = "";
						if (v3_psize > v3_fixed) {
							char const *p = static_cast<char const*>(
								packet_get_data_const(rpacket, v3_fixed, v3_psize - v3_fixed));
							if (p) v3_gname = p;
						}
						int v3rc = pvpgn_v3_send_friendinforeply(
							c,
							static_cast<unsigned int>(bn_byte_get(packet->u.client_friendinforeq.friendnum)),
							v3_type,
							v3_status,
							v3_ctag,
							v3_gname);
						if (v3rc == 1) {
							packet_del_ref(rpacket);
							return 0;
						}
					}
	
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
	
				}
	
				return 0;
			}

		int _client_atfriendscreen(t_connection * c, t_packet const *const packet)
		{
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got CLIENT_ARRANGEDTEAM_FRIENDSCREEN packet", conn_get_socket(c));

			const char *my_username = conn_get_username(c);
			if (!my_username)
			{
				return -1;
			}

			t_channel *my_channel = conn_get_channel(c);
			int my_channel_is_public = 1;
			if (my_channel)
			{
				my_channel_is_public = channel_get_flags(my_channel) & channel_flags_public;
			}


			t_packet *rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create friendscreen server packet", conn_get_socket(c));
				return -1;
			}
			packet_set_size(rpacket, sizeof(t_server_arrangedteam_friendscreen));
			packet_set_type(rpacket, SERVER_ARRANGEDTEAM_FRIENDSCREEN);


			std::uint8_t available_players = 0;

			// begin search for mutual and available friends
			auto& my_friend_list = account_get_friends(conn_get_account(c));
			for (t_friend* _friend : my_friend_list)
			{
				if (available_players == std::numeric_limits<decltype(available_players)>::max())
				{
					eventlog(eventlog_level_info, __FUNCTION__, "Reached maximum amount of available players to send in packet");
					break;
				}

				// skip non-mutual friends
				if (friend_get_mutual(_friend) == FRIEND_NOTMUTUAL)
				{
					continue;
				}

				t_account *friend_account = friend_get_account(_friend);
				t_connection *friend_connection = connlist_find_connection_by_account(friend_account);
				// if user is offline, then continue to next friend
				if (!friend_connection)
				{
					continue;
				}

				const std::string my_version_tag = conn_get_versioncheck(c) ? conn_get_versioncheck(c)->get_version_tag() : "";
				const std::string friend_version_tag = conn_get_versioncheck(friend_connection) ? conn_get_versioncheck(friend_connection)->get_version_tag() : "";
				// friend is using another game or is on a different version
				if (my_version_tag != friend_version_tag)
				{
					continue;
				}

				// friend has Do Not Disturb mode enabled
				if (conn_get_dndstr(friend_connection))
				{
					continue;
				}

				// friend is away
				if (conn_get_awaystr(friend_connection))
				{
					continue;
				}

				// friend is not in a channel
				t_channel *friend_channel = conn_get_channel(friend_connection);
				if (!friend_channel)
				{
					continue;
				}

				// // don't list YET if in same private channel
				if (!my_channel_is_public && (friend_channel == my_channel))
				{
					continue;
				}

				const char *friend_name = account_get_name(friend_account);
				eventlog(eventlog_level_trace, __FUNCTION__, "Friend {} is available for an AT Game.", friend_name);
				available_players += 1;
				packet_append_string(rpacket, friend_name);
			}


			// now list matching users in same private chan
			if (!my_channel_is_public)
			{
				for (t_connection *user_connection = channel_get_first(my_channel); user_connection; user_connection = channel_get_next())
				{
					if (available_players == std::numeric_limits<decltype(available_players)>::max())
					{
						eventlog(eventlog_level_info, __FUNCTION__, "Reached maximum amount of available players to send in packet");
						break;
					}

					// skip self
					if (user_connection == c)
					{
						continue;
					}

					const std::string my_version_tag = conn_get_versioncheck(c) ? conn_get_versioncheck(c)->get_version_tag() : "";
					const std::string user_version_tag = conn_get_versioncheck(user_connection) ? conn_get_versioncheck(user_connection)->get_version_tag() : "";
					// user is using another game or is on a different version
					if (my_version_tag != user_version_tag)
					{
						continue;
					}

					// user is dnd
					if (conn_get_dndstr(user_connection))
					{
						continue;
					}

					// user is away
					if (conn_get_awaystr(user_connection))
					{
						continue;
					}
					
					const char *username = account_get_name(conn_get_account(user_connection));
					if (!username)
					{
						continue;
					}

					eventlog(eventlog_level_trace, __FUNCTION__, "user {} in private channel {} is available for an AT Game.", username, channel_get_name(my_channel));
					available_players += 1;
					packet_append_string(rpacket, username);
				}
			}

			if (available_players == 0)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "No available players");
			}

			bn_byte_set(&rpacket->u.server_arrangedteam_friendscreen.f_count, available_players);

			{
				// Collect the appended player name strings from the legacy-built
				// packet payload and ship via v3 codec.
				unsigned int v3_fixed = static_cast<unsigned int>(
					sizeof(t_server_arrangedteam_friendscreen));
				unsigned int v3_psize = packet_get_size(rpacket);
				std::vector<char const*> v3_names;
				if (v3_psize > v3_fixed) {
					char const *p = static_cast<char const*>(
						packet_get_data_const(rpacket, v3_fixed, v3_psize - v3_fixed));
					char const *end = p + (v3_psize - v3_fixed);
					while (p && p < end) {
						v3_names.push_back(p);
						std::size_t slen = std::strlen(p);
						p += slen + 1;
					}
				}
				int v3rc = pvpgn_v3_send_atfriendscreenreply(
					c,
					v3_names.empty() ? nullptr : v3_names.data(),
					static_cast<unsigned int>(v3_names.size()));
				if (v3rc == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		int _client_atinvitefriend(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;
			t_clienttag ctag;

			if (packet_get_size(packet) < sizeof(t_client_arrangedteam_invite_friend)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ARRANGEDTEAM_INVITE_FRIEND packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_arrangedteam_invite_friend), packet_get_size(packet));
				return -1;
			}

			ctag = conn_get_clienttag(c);

			{
				int count_to_invite, count, id;
				char const *invited_usernames[8];
				t_account *members[MAX_TEAMSIZE];
				int i, n, offset, teammemcount;
				t_connection *dest_c;
				t_team *team;
				unsigned int teamid;

				count_to_invite = bn_byte_get(packet->u.client_arrangedteam_invite_friend.numfriends);
				count = bn_int_get(packet->u.client_arrangedteam_invite_friend.count);
				id = bn_int_get(packet->u.client_arrangedteam_invite_friend.id);
				teammemcount = count_to_invite + 1;

				if ((count_to_invite < 1) || (count_to_invite > 3)) {
					eventlog(eventlog_level_error, __FUNCTION__, "got invalid number of users to invite to game");
					return -1;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] got ARRANGEDTEAM INVITE packet for {} invitees", conn_get_socket(c), count_to_invite);

				offset = sizeof(t_client_arrangedteam_invite_friend);

				for (i = 0; i < count_to_invite; i++) {
					if (!(invited_usernames[i] = packet_get_str_const(packet, offset, MAX_USERNAME_LEN))) {
						eventlog(eventlog_level_error, "handle_bnet", "Could not get username from invite packet");
						return -1;
					}
					else {
						offset += std::strlen(invited_usernames[i]) + 1;
						eventlog(eventlog_level_debug, "handle_bnet", "Added user {} to invite array.", invited_usernames[i]);
					}
				}

				members[0] = conn_get_account(c);
				for (i = 1; i < MAX_TEAMSIZE; i++) {
					if ((i < teammemcount)) {
						if (!(members[i] = accountlist_find_account(invited_usernames[i - 1]))) {
							eventlog(eventlog_level_error, __FUNCTION__, "got invitation for non-existant user \"{}\"", invited_usernames[i - 1]);
							return -1;
						}
					}
					else
						members[i] = NULL;
				}


				if (!(team = account_find_team_by_accounts(members[0], members, ctag))) {
					team = create_team(members, ctag);	//no need to free on return -1 because it's already in teamlist

					eventlog(eventlog_level_trace, __FUNCTION__, "this team has never played before, creating new team");
				}
				else {
					eventlog(eventlog_level_trace, __FUNCTION__, "this team has already played before");
				}

				teamid = team_get_teamid(team);
				account_set_currentatteam(conn_get_account(c), team_get_teamid(team));


				//Create the packet to send to each of the users you wanted to invite
				conn_part_channel(c);

				for (i = 0; i < teammemcount; i++) {

					if (!(dest_c = account_get_conn(team_get_member(team, i))))
						continue;

					if ((dest_c == c))
						continue;

					if (!(rpacket = packet_create(packet_class_bnet)))
						return -1;

					packet_set_size(rpacket, sizeof(t_server_arrangedteam_send_invite));
					packet_set_type(rpacket, SERVER_ARRANGEDTEAM_SEND_INVITE);

					bn_int_set(&rpacket->u.server_arrangedteam_send_invite.count, count);
					bn_int_set(&rpacket->u.server_arrangedteam_send_invite.id, id);
					{			/* trans support */
						unsigned short port = conn_get_game_port(c);
						unsigned int addr = conn_get_addr(c);

						trans_net(conn_get_addr(dest_c), &addr, &port);

						bn_int_nset(&rpacket->u.server_arrangedteam_send_invite.inviterip, addr);
						bn_short_set(&rpacket->u.server_arrangedteam_send_invite.port, port);
					}
					bn_byte_set(&rpacket->u.server_arrangedteam_send_invite.numfriends, count_to_invite);

					for (n = 0; n < teammemcount; n++) {
						if (n != i)
							packet_append_string(rpacket, account_get_name(team_get_member(team, n)));
					}

					//now send packet
					conn_push_outqueue(dest_c, rpacket);
					packet_del_ref(rpacket);

					account_set_currentatteam(conn_get_account(dest_c), teamid);
				}

				//now send a ACK to the inviter
				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_arrangedteam_invite_friend_ack));
				packet_set_type(rpacket, SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK);

				bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.count, count);
				bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.id, id);
				bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.timestamp, now);
				bn_byte_set(&rpacket->u.server_arrangedteam_invite_friend_ack.teamsize, count_to_invite + 1);

				/*
				 * five int's to fill
				 * fill with uid's of all teammembers, including the inviter
				 * and the rest with FFFFFFFF
				 * to be used when sever recieves anongame search
				 * [Omega]
				 */
				for (i = 0; i < 5; i++) {

					if (i < teammemcount) {
						bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.info[i], team_get_memberuid(team, i));
					}
					else {		/* fill rest with FFFFFFFF */
						bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.info[i], 0xFFFFFFFF);
					}
				}

					{
						unsigned int v3_info[5];
						for (int v3i = 0; v3i < 5; ++v3i)
							v3_info[v3i] = static_cast<unsigned int>(
								bn_int_get(rpacket->u.server_arrangedteam_invite_friend_ack.info[v3i]));
						int v3rc = pvpgn_v3_send_atinvitefriendack(
							c,
							static_cast<unsigned int>(bn_int_get(rpacket->u.server_arrangedteam_invite_friend_ack.count)),
							static_cast<unsigned int>(bn_int_get(rpacket->u.server_arrangedteam_invite_friend_ack.id)),
							static_cast<unsigned int>(bn_int_get(rpacket->u.server_arrangedteam_invite_friend_ack.timestamp)),
							static_cast<unsigned int>(bn_byte_get(rpacket->u.server_arrangedteam_invite_friend_ack.teamsize)),
							v3_info);
						if (v3rc == 1) {
							packet_del_ref(rpacket);
							return 0;
						}
					}
				conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
	
				}
	
				return 0;
			}

		int _client_atacceptdeclineinvite(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;
			t_clienttag ctag;

			if (packet_get_size(packet) < sizeof(t_client_arrangedteam_accept_decline_invite)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ARRANGEDTEAM_ACCEPT_DECLINE_INVITE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_arrangedteam_accept_decline_invite), packet_get_size(packet));
				return -1;
			}

			ctag = conn_get_clienttag(c);

			{
				char const *inviter;
				t_connection *dest_c;

				//if user declined the invitation then
				if (bn_int_get(packet->u.client_arrangedteam_accept_decline_invite.option) == CLIENT_ARRANGEDTEAM_DECLINE) {
					inviter = packet_get_str_const(packet, sizeof(t_client_arrangedteam_accept_decline_invite), MAX_USERNAME_LEN);
					dest_c = connlist_find_connection_by_accountname(inviter);

					eventlog(eventlog_level_info, "handle_bnet", "{} declined a arranged team game with {}", conn_get_username(c), inviter);

					if (!(rpacket = packet_create(packet_class_bnet)))
						return -1;
					packet_set_size(rpacket, sizeof(t_server_arrangedteam_member_decline));
					packet_set_type(rpacket, SERVER_ARRANGEDTEAM_MEMBER_DECLINE);

					bn_int_set(&rpacket->u.server_arrangedteam_member_decline.count, bn_int_get(packet->u.client_arrangedteam_accept_decline_invite.count));
						bn_int_set(&rpacket->u.server_arrangedteam_member_decline.action, SERVER_ARRANGEDTEAM_DECLINE);
						packet_append_string(rpacket, conn_get_username(c));
	
						{
							int v3rc = pvpgn_v3_send_atmemberdecline(
								dest_c,
								static_cast<unsigned int>(bn_int_get(rpacket->u.server_arrangedteam_member_decline.count)),
								static_cast<unsigned int>(bn_int_get(rpacket->u.server_arrangedteam_member_decline.action)),
								conn_get_username(c));
							if (v3rc == 1) {
								packet_del_ref(rpacket);
								return 0;
							}
						}
	
						conn_push_outqueue(dest_c, rpacket);
						packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		int _client_atacceptinvite(t_connection * c, t_packet const *const packet)
		{
			// t_packet * rpacket;

			if (packet_get_size(packet) < sizeof(t_client_arrangedteam_accept_invite)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ARRANGEDTEAM_ACCEPT_INVITE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_arrangedteam_accept_invite), packet_get_size(packet));
				return -1;
			}
			/* conn_set_channel(c, "Arranged Teams"); */
			return 0;
		}


}} // namespace pvpgn::bnetd
