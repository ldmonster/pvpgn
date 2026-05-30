// =====================================================================
// auth_login.cpp — Login / logon handlers
// Split from auth.cpp (plan 15 §3 / SOLID-S refactor)
// Handles: loginreq1 (SC/BW legacy), loginreq2 (D2/W2), loginreqw3 (W3 SRP),
//          logonproofreq (W3 SRP proof), client_init_email helper
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		void client_init_email(t_connection * c, t_account * account)
		{
			t_packet *packet;
			char const *email;

			if (!c || !account)
				return;
			if (!(email = account_get_email(account))) {
				if ((packet = packet_create(packet_class_bnet))) {
					packet_set_size(packet, sizeof(t_server_setemailreq));
					packet_set_type(packet, SERVER_SETEMAILREQ);
					conn_push_outqueue(c, packet);
					packet_del_ref(packet);
				}
			}
			return;
		}

		int _client_loginreq1(t_connection * c, t_packet const *const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_loginreq1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_loginreq1), packet_get_size(packet));
				return -1;
			}

			// 28c strangler-fig: offer the v3 login path first.
			// Scaffold-only (returns 0 -> legacy runs) unless
			// `PVPGN_V3_LOGIN=1` registers a handler.
			PVPGN_V3_BRIDGE_TRY(login_user, c, packet,
				packet_get_size(packet));

			{
				char const *username;
				t_account *account;

				if (!(username = packet_get_str_const(packet, sizeof(t_client_loginreq1), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ1 (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				t_packet* const rpacket = packet_create(packet_class_bnet);
				if (!rpacket)
				{
					return -1;
				}

				packet_set_size(rpacket, sizeof(t_server_loginreply1));
				packet_set_type(rpacket, SERVER_LOGINREPLY1);

				// too many logins? [added by NonReal]
				if (prefs_v3::max_concurrent_logins() > 0)
				{
					if (prefs_v3::max_concurrent_logins() <= connlist_login_get_length())
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login denied, too many concurrent logins. max: {}. current: {}.", conn_get_socket(c), prefs_v3::max_concurrent_logins(), connlist_login_get_length());
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
						if (pvpgn_v3_send_loginreply1(
								c, SERVER_LOGINREPLY1_MESSAGE_FAIL) == 1) {
							packet_del_ref(rpacket);
							return -1;
						}
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
						return -1;
					}
				}

				/* fail if no account */
				if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no such account)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
				}
				else
					/* already logged in */
				if (connlist_find_connection_by_account(account) && prefs_v3::kick_old_login() == 0) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (already logged in)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
				}
				else if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
				}
				else if (conn_get_sessionkey(c) != bn_int_get(packet->u.client_loginreq1.sessionkey)) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] login for \"{}\" refused (expected session key 0x{:08x}, got 0x{:08x})", conn_get_socket(c), username, conn_get_sessionkey(c), bn_int_get(packet->u.client_loginreq1.sessionkey));
					bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
				}
				else {
					struct {
						bn_int ticks;
						bn_int sessionkey;
						bn_int passhash1[5];
					} temp;
					char const *oldstrhash1;
					t_hash oldpasshash1;
					t_hash oldpasshash2;
					t_hash trypasshash2;

					if ((oldstrhash1 = account_get_pass(account))) {
						bn_int_set(&temp.ticks, bn_int_get(packet->u.client_loginreq1.ticks));
						bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_loginreq1.sessionkey));
						if (hash_set_str(&oldpasshash1, oldstrhash1) < 0) {
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (corrupted passhash1?)", conn_get_socket(c), username);
							bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
						}
						else {
							hash_to_bnhash((t_hash const *)&oldpasshash1, temp.passhash1);	/* avoid warning */

							bnet_hash(&oldpasshash2, sizeof(temp), &temp);	/* do the double hash */
							bnhash_to_hash(packet->u.client_loginreq1.password_hash2, &trypasshash2);

							if (hash_eq(trypasshash2, oldpasshash2) == 1) {
								conn_login(c, account, username);
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (correct password)", conn_get_socket(c), username);
								bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_SUCCESS);
#ifdef WITH_LUA
								if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
								{
									// feature to break login from Lua
									conn_set_state(c, conn_state_destroy);
									return -1;
								}
#endif
#ifdef WIN32_GUI
								guiOnUpdateUserList();
#endif
							}
							else {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (wrong password)", conn_get_socket(c), username);
								conn_increment_passfail_count(c);
								bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
							}
						}
					}
					else {
						conn_login(c, account, username);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (no password)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_SUCCESS);
#ifdef WIN32_GUI
						guiOnUpdateUserList();
#endif
					}
				}
				{
					// Strangler-fig: ship the same 8 bytes via the
					// v3 codec. Read the final message field out of
					// the legacy-built rpacket so all branch logic
					// above stays intact.
					unsigned int v3_msg = bn_int_get(
						rpacket->u.server_loginreply1.message);
					int v3rc = pvpgn_v3_send_loginreply1(c, v3_msg);
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

		int _client_loginreq2(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;
			int success = 0;

			if (packet_get_size(packet) < sizeof(t_client_loginreq2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_loginreq2), packet_get_size(packet));
				return -1;
			}

			PVPGN_V3_BRIDGE_TRY(login_user, c, packet,
				packet_get_size(packet));

			{
				char const *username;
				t_account *account;
				char supports_locked_reply = 0;
				t_clienttag clienttag = conn_get_clienttag(c);

				if (clienttag == CLIENTTAG_STARCRAFT_UINT || clienttag == CLIENTTAG_BROODWARS_UINT || clienttag == CLIENTTAG_SHAREWARE_UINT || 
					clienttag == CLIENTTAG_DIABLORTL_UINT || clienttag == CLIENTTAG_DIABLOSHR_UINT || clienttag == CLIENTTAG_WARCIIBNE_UINT || 
					clienttag == CLIENTTAG_DIABLO2DV_UINT || clienttag == CLIENTTAG_STARJAPAN_UINT || clienttag == CLIENTTAG_DIABLO2ST_UINT ||
					clienttag == CLIENTTAG_DIABLO2XP_UINT || clienttag == CLIENTTAG_WARCRAFT3_UINT || clienttag == CLIENTTAG_WAR3XP_UINT )
				{
					if (conn_get_versionid(c) >= 0x0000000b)
						supports_locked_reply = 1;
				}

				if (!(username = packet_get_str_const(packet, sizeof(t_client_loginreq2), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ2 (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_loginreply2));
				packet_set_type(rpacket, SERVER_LOGINREPLY2);

				// too many logins? [added by NonReal]
				if (prefs_v3::max_concurrent_logins() > 0) {
					if (prefs_v3::max_concurrent_logins() <= connlist_login_get_length()) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login denied, too many concurrent logins. max: {}. current: {}.", conn_get_socket(c), prefs_v3::max_concurrent_logins(), connlist_login_get_length());
						if (supports_locked_reply) {
							bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
							packet_append_string(rpacket, "Too many concurrent logins. Try again later.");
						}
						else {
							bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
						}

						packet_del_ref(rpacket);
						return -1;
					}
				}

				/* fail if no account */
				if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no such account)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_NONEXIST);
				}
				/* already logged in */
				else if (connlist_find_connection_by_account(account) && prefs_v3::kick_old_login() == 0) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (already logged in)", conn_get_socket(c), username);
					if (supports_locked_reply) {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
						packet_append_string(rpacket, "This account is already logged in.");
					}
					else {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
					}
				}
				else if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
					if (supports_locked_reply) {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
						packet_append_string(rpacket, "This account is barred from bnet access.");
					}
					else {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
					}
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
					if (supports_locked_reply)
					{
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
						std::string msgtemp = localize(c, "This account has been locked");
						msgtemp += account_get_locktext(c, account, true);
						packet_append_string(rpacket, msgtemp.c_str());
					}
					else {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
					}
				}
				else if (conn_get_sessionkey(c) != bn_int_get(packet->u.client_loginreq2.sessionkey)) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] login for \"{}\" refused (expected session key 0x{:08x}, got 0x{:08x})", conn_get_socket(c), username, conn_get_sessionkey(c), bn_int_get(packet->u.client_loginreq2.sessionkey));
					bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
				}
				else {
					struct {
						bn_int ticks;
						bn_int sessionkey;
						bn_int passhash1[5];
					} temp;
					char const *oldstrhash1;
					t_hash oldpasshash1;
					t_hash oldpasshash2;
					t_hash trypasshash2;

					if ((oldstrhash1 = account_get_pass(account))) {
						bn_int_set(&temp.ticks, bn_int_get(packet->u.client_loginreq2.ticks));
						bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_loginreq2.sessionkey));
						if (hash_set_str(&oldpasshash1, oldstrhash1) < 0) {
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (corrupted passhash1?)", conn_get_socket(c), username);
							bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
						}
						else {
							hash_to_bnhash((t_hash const *)&oldpasshash1, temp.passhash1);	/* avoid warning */

							bnet_hash(&oldpasshash2, sizeof(temp), &temp);	/* do the double hash */
							bnhash_to_hash(packet->u.client_loginreq2.password_hash2, &trypasshash2);

							if (hash_eq(trypasshash2, oldpasshash2) == 1) {
								conn_login(c, account, username);
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (correct password)", conn_get_socket(c), username);
								bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_SUCCESS);
								success = 1;
							}
							else {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (wrong password)", conn_get_socket(c), username);
								conn_increment_passfail_count(c);
								bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
							}
						}
					}
					else {
						conn_login(c, account, username);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (no password)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_SUCCESS);
						success = 1;
					}
				}
				if (success && account) {

#ifdef WITH_LUA
					if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
					{
						// feature to break login from Lua
						conn_set_state(c, conn_state_destroy);
						packet_del_ref(rpacket);
						return -1;
					}
#endif

#ifdef WIN32_GUI
					guiOnUpdateUserList();
#endif
					client_init_email(c, account);
				}

				{
					// Strangler-fig: ship LOGINREPLY2 via v3 codec
					// for the no-reason case. If legacy already
					// appended a reason cstring (supports_locked_reply
					// branches), fall back to legacy emit so the
					// reason bytes are preserved verbatim.
					unsigned int v3_size = packet_get_size(rpacket);
					if (v3_size == sizeof(t_server_loginreply2)) {
						unsigned int v3_msg = bn_int_get(
							rpacket->u.server_loginreply2.message);
						int v3rc = pvpgn_v3_send_loginreply2(
							c, v3_msg, nullptr);
						if (v3rc == 1) {
							packet_del_ref(rpacket);
							return 0;
						}
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_loginreqw3(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_loginreq_w3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_LOGINREQ_W3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_loginreq_w3), packet_get_size(packet));
				return -1;
			}

			PVPGN_V3_BRIDGE_TRY(login_user, c, packet,
				packet_get_size(packet));

			{
				char const *username;
				t_account *account;
				char const *account_salt;
				char const *account_verifier;
				const char *conn_client_public_key;
				int i;

				/* PELISH: Does not need to check conn_client_public_key != NULL because we testing packet size */
				conn_client_public_key = (char *)packet_get_data_const(packet, offsetof(t_client_loginreq_w3, client_public_key), 32);


				if (!(username = packet_get_str_const(packet, sizeof(t_client_loginreq_w3), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_LOGINREQ_W3 (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_loginreply_w3));
				packet_set_type(rpacket, SERVER_LOGINREPLY_W3);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.salt[i], 0);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.server_public_key[i], 0);

				{
					/* too many logins? */
					if (prefs_v3::max_concurrent_logins() > 0 && prefs_v3::max_concurrent_logins() <= connlist_login_get_length()) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login denied, too many concurrent logins. max: {}. current: {}.", conn_get_socket(c), prefs_v3::max_concurrent_logins(), connlist_login_get_length());
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
					}
					else
						/* fail if no account */
					if (!(account = accountlist_find_account(username))) {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" refused (no such account)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
					}
					else
						/* already logged in */
					if (connlist_find_connection_by_account(account) && prefs_v3::kick_old_login() == 0) {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" refused (already logged in)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_ALREADY);
					}
					else if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
					}
					else if (!(account_salt = account_get_salt(account)))  {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account check (even though account has no salt)", conn_get_socket(c), username);
						conn_set_loggeduser(c, username);
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
					}
					else if (!(account_verifier = account_get_verifier(account)))  {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account check (even though account has no verifier)", conn_get_socket(c), username);
						conn_set_loggeduser(c, username);
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
						delete[] const_cast<char*>(account_salt);
					}
					else {

						for (i = 0; i < 32; i++){
							bn_byte_set(&rpacket->u.server_loginreply_w3.salt[i], account_salt[i]);
						}

						BigInt salt = BigInt((unsigned char*)account_salt, 32, 4, false);
						BigInt verifier = BigInt((unsigned char*)account_verifier, 32, 1, false);
						BnetSRP3 srp3 = BnetSRP3(username, salt);

						BigInt client_public_key = BigInt((unsigned char*)conn_client_public_key, 32, 1, false);
						BigInt server_public_key = srp3.getServerSessionPublicKey(verifier);

						server_public_key.getData((unsigned char*)&rpacket->u.server_loginreply_w3.server_public_key, 32, 4, false);

						BigInt hashed_server_secret_ = srp3.getHashedServerSecret(client_public_key, verifier);
						BigInt client_proof = srp3.getClientPasswordProof(client_public_key, server_public_key, hashed_server_secret_);
						BigInt server_proof = srp3.getServerPasswordProof(client_public_key, client_proof, hashed_server_secret_);

						char * conn_client_proof = (char*)client_proof.getData(20, 4, false);
						char * conn_server_proof = (char*)server_proof.getData(20, 4, false);

						conn_set_client_proof(c, conn_client_proof);
						conn_set_server_proof(c, conn_server_proof);

						delete[] const_cast<char*>(account_verifier);
						delete[] const_cast<char*>(account_salt);
						delete[] conn_client_proof;
						delete[] conn_server_proof;

						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account check", conn_get_socket(c), username);
						conn_set_loggeduser(c, username);
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
					}
				}

				{
					// Strangler-fig: ship the same 72 bytes via the
					// v3 codec. Read message + salt + B back out of
					// the legacy-built rpacket so all branch logic
					// stays intact.
					unsigned int v3_msg = bn_int_get(
						rpacket->u.server_loginreply_w3.message);
					unsigned char const* v3_salt =
						reinterpret_cast<unsigned char const*>(
							&rpacket->u.server_loginreply_w3.salt[0]);
					unsigned char const* v3_spk =
						reinterpret_cast<unsigned char const*>(
							&rpacket->u.server_loginreply_w3.server_public_key[0]);
					int v3rc = pvpgn_v3_send_loginreply_w3(
						c, v3_msg, v3_salt, v3_spk);
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

		int _client_logonproofreq(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_logonproofreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGONPROOFREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_logonproofreq), packet_get_size(packet));
				return -1;
			}

			{
				char const *username;
				t_account *account;

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] logon proof requested", conn_get_socket(c));

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_logonproofreply));
				packet_set_type(rpacket, SERVER_LOGONPROOFREPLY);

				std::memset(&rpacket->u.server_logonproofreply.server_password_proof, 0, sizeof(rpacket->u.server_logonproofreply.server_password_proof));

				bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_BADPASS);

				if (!(username = conn_get_loggeduser(c))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got NULL username, 0x54ff before 0x53ff?", conn_get_socket(c));
				}
				else if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login in 0x54ff for \"{}\" refused (no such account)", conn_get_socket(c), username);
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_CUSTOM);
					std::string msgtemp = localize(c, "This account has been locked");
					msgtemp += account_get_locktext(c, account, true);
					packet_append_string(rpacket, msgtemp.c_str());
				}
				else {
					t_hash serverhash;
					t_hash clienthash;
					int i;
					const char * client_password_proof;

					/* PELISH: This can not occur - We already tested packet size which must be wrong firstly.
					   Also pvpgn will crash when will dereferencing NULL pointer (so we cant got this errorlog message)
					   I vote for deleting this "if" */
					if (!(client_password_proof = (const char*)packet_get_data_const(packet, offsetof(t_client_logonproofreq, client_password_proof), 20))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] (W3) got bad LOGONPROOFREQ packet (missing hash)", conn_get_socket(c));
						packet_del_ref(rpacket);
						return -1;
					}

					clienthash[0] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[0]);
					clienthash[1] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[4]);
					clienthash[2] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[8]);
					clienthash[3] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[12]);
					clienthash[4] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[16]);

					hash_set_str(&serverhash, account_get_pass(account));

					if (conn_get_client_proof(c) && std::memcmp(client_password_proof, conn_get_client_proof(c), 20) == 0) {
						const char * server_proof = conn_get_server_proof(c);

						for (i = 0; i < 20; i++){
							bn_byte_set(&rpacket->u.server_logonproofreply.server_password_proof[i], server_proof[i]);
						}

						conn_login(c, account, username);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" logged in (right client password proof)", conn_get_socket(c), username);
						if ((conn_get_versionid(c) >= 0x0000000D) && (account_get_email(account) == NULL))
							bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_EMAIL);
						else
							bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_OK);
						// by amadeo updates the userlist
#ifdef WIN32_GUI
						guiOnUpdateUserList();
#endif
					}
					else if (hash_eq(clienthash, serverhash)) {

						conn_login(c, account, username);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" logged in (right password)", conn_get_socket(c), username);
						if ((conn_get_versionid(c) >= 0x0000000D) && (account_get_email(account) == NULL))
							bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_EMAIL);
						else
							bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_OK);
#ifdef WITH_LUA
						if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
						{
							// feature to break login from Lua
							conn_set_state(c, conn_state_destroy);
							packet_del_ref(rpacket);
							return -1;
						}
#endif
						// by amadeo updates the userlist
#ifdef WIN32_GUI
						guiOnUpdateUserList();
#endif

					}
					else {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got wrong client password proof for \"{}\"", conn_get_socket(c), username);
						conn_increment_passfail_count(c);
					}
				}
				{
					// Strangler-fig: ship via v3 codec. Read response
					// + proof back out of the legacy-built rpacket.
					// When the legacy CUSTOM-lock branch was taken,
					// `packet_append_string` extended the rpacket past
					// `sizeof(t_server_logonproofreply)` -- in that
					// case skip the v3 path and let the legacy emit
					// preserve the appended reason verbatim.
					if (packet_get_size(rpacket)
					    == sizeof(t_server_logonproofreply)) {
						unsigned int v3_resp = bn_int_get(
							rpacket->u.server_logonproofreply.response);
						unsigned char const* v3_proof =
							reinterpret_cast<unsigned char const*>(
								&rpacket->u.server_logonproofreply
								         .server_password_proof[0]);
						int v3rc = pvpgn_v3_send_logonproof_reply(
							c, v3_resp, v3_proof, nullptr);
						if (v3rc == 1) {
							packet_del_ref(rpacket);
							conn_set_client_proof(c, NULL);
							conn_set_server_proof(c, NULL);
							clan_send_status_window(c);
							return 0;
						}
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				conn_set_client_proof(c, NULL);
				conn_set_server_proof(c, NULL);
			}
			clan_send_status_window(c);

			return 0;
		}

}} // namespace pvpgn::bnetd
