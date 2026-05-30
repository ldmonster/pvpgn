// =====================================================================
// auth_password.cpp — Password change handlers
// Split from auth.cpp (plan 15 §3 / SOLID-S refactor)
// Handles: changepassreq (legacy double-hash), passchangereq (W3 SRP),
//          passchangeproofreq (W3 SRP proof verification)
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_changepassreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch(c, "changepassreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_changepassreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CHANGEPASSREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_changepassreq), packet_get_size(packet));
				return -1;
			}

			// 28b strangler-fig: offer the v3 ChangePassword path
			// first. Scaffold-only today (returns 0 -> fall-through)
			// unless `PVPGN_V3_CHANGEPW=1` registers a handler at
			// process start. The legacy double-hash dance below is
			// the source of truth until the use-case learns it.
			PVPGN_V3_BRIDGE_TRY(change_password, c, packet,
				packet_get_size(packet));

			{
				char const *username;
				t_account *account;

				if (!(username = packet_get_str_const(packet, sizeof(t_client_changepassreq), UNCHECKED_NAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CHANGEPASSREQ (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change requested for \"{}\"", conn_get_socket(c), username);

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_changepassack));
				packet_set_type(rpacket, SERVER_CHANGEPASSACK);

				/* fail if logged in or no account */
				if (connlist_find_connection_by_accountname(username) || !(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (no such account)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
				}
				else if (account_get_auth_changepass(account) == 0) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (no change access)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
				}
				else if (conn_get_sessionkey(c) != bn_int_get(packet->u.client_changepassreq.sessionkey)) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] password change for \"{}\" refused (expected session key 0x{:08x}, got 0x{:08x})", conn_get_socket(c), username, conn_get_sessionkey(c), bn_int_get(packet->u.client_changepassreq.sessionkey));
					bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
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
					t_hash newpasshash1;


					if ((oldstrhash1 = account_get_pass(account))) {
						bn_int_set(&temp.ticks, bn_int_get(packet->u.client_changepassreq.ticks));
						bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_changepassreq.sessionkey));
						if (hash_set_str(&oldpasshash1, oldstrhash1) < 0) {
							bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
							account_set_pass(account, hash_get_str(newpasshash1));
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" successful (bad previous password)", conn_get_socket(c), account_get_name(account));
							bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
						}
						else {
							hash_to_bnhash((t_hash const *)&oldpasshash1, temp.passhash1);	/* avoid warning */
							bnet_hash(&oldpasshash2, sizeof(temp), &temp);	/* do the double hash */
							bnhash_to_hash(packet->u.client_changepassreq.oldpassword_hash2, &trypasshash2);

							if (hash_eq(trypasshash2, oldpasshash2) == 1) {
								bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
								account_set_pass(account, hash_get_str(newpasshash1));
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" successful (previous password)", conn_get_socket(c), account_get_name(account));
								bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
							}
							else {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (wrong password)", conn_get_socket(c), account_get_name(account));
								conn_increment_passfail_count(c);
								bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
							}
						}
					}
					else {
						bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
						account_set_pass(account, hash_get_str(newpasshash1));
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" successful (no previous password)", conn_get_socket(c), account_get_name(account));
						bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
					}
				}

				{
					unsigned int msg_v3 = static_cast<unsigned int>(
						bn_int_get(rpacket->u.server_changepassack.message));
					if (pvpgn_v3_send_changepassack(c, msg_v3) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

			}

			return 0;
		}

		int _client_passchangereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_passemail_dispatch(c, "passchangereq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_passchangereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_PASSCHANGEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_passchangereq), packet_get_size(packet));
				return -1;
			}

			{
				char const *username;
				t_account *account;
				char const *account_salt;
				char const *account_verifier;
				const char *conn_client_public_key;
				int i;

				/* PELISH: Does not need to check conn_client_public_key != NULL because we testing packet size */
				conn_client_public_key = (char *)packet_get_data_const(packet, offsetof(t_client_loginreq_w3, client_public_key), 32);


				if (!(username = packet_get_str_const(packet, sizeof(t_client_passchangereq), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_PASSCHANGEREQ (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_passchangereply));
				packet_set_type(rpacket, SERVER_PASSCHANGEREPLY);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.salt[i], 0);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.server_public_key[i], 0);

				{
					bn_int_set(&rpacket->u.server_passchangereply.message, SERVER_PASSCHANGEREPLY_MESSAGE_REJECT);

					/* fail if no account */
					if (!(account = accountlist_find_account(username))) {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no such account)", conn_get_socket(c), username);
					}
					else
						/* fail if bnetlogin is disallowed */
					if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
					}
					else
						/* fail if no salt */
					if ((account_salt = account_get_salt(account)) == NULL)  {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no SALT)", conn_get_socket(c), username);
					}
					else
						/* fail if no verifier */
					if ((account_verifier = account_get_verifier(account)) == NULL)  {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no VERIFIER)", conn_get_socket(c), username);
						delete[] const_cast<char*>(account_salt);
					}
					else {

						for (i = 0; i < 32; i++){
							bn_byte_set(&rpacket->u.server_passchangereply.salt[i], account_salt[i]);
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

						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account passchange check", conn_get_socket(c), username);
						conn_set_loggeduser(c, username);
						bn_int_set(&rpacket->u.server_passchangereply.message, SERVER_PASSCHANGEREPLY_MESSAGE_ACCEPT);
					}
				}

				{
					// Strangler-fig: ship SERVER_PASSCHANGEREPLY (0x55) via
					// the v3 codec. Read message + salt + B back out of the
					// legacy-built rpacket so all branch logic stays intact.
					unsigned int v3_msg = bn_int_get(
						rpacket->u.server_passchangereply.message);
					unsigned char const* v3_salt =
						reinterpret_cast<unsigned char const*>(
							&rpacket->u.server_passchangereply.salt[0]);
					unsigned char const* v3_spk =
						reinterpret_cast<unsigned char const*>(
							&rpacket->u.server_passchangereply.server_public_key[0]);
					int v3rc = pvpgn_v3_send_passchangereply(
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

		int _client_passchangeproofreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_passemail_dispatch(c, "passchangeproofreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_passchangeproofreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PASSCHANGEPROOFREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_passchangeproofreq), packet_get_size(packet));
				return -1;
			}

			{
				char const *username;
				t_account *account;
				const char *salt;
				const char *verifier;

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] passchange proof requested", conn_get_socket(c));

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_passchangeproofreply));
				packet_set_type(rpacket, SERVER_PASSCHANGEPROOFREPLY);

				std::memset(&rpacket->u.server_passchangeproofreply.server_password_proof, 0, sizeof(rpacket->u.server_passchangeproofreply.server_password_proof));

				bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_PASSCHANGEPROOFREPLY_RESPONSE_BADPASS);

				if (!(username = conn_get_loggeduser(c))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got NULL username, 0x56ff before 0x55ff?", conn_get_socket(c));
				}
				else if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange in 0x56ff for \"{}\" refused (no such account)", conn_get_socket(c), username);
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] passchange for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
				}
				else {
					int i;
					const char * client_password_proof;

					if (!(client_password_proof = (const char*)packet_get_data_const(packet, offsetof(t_client_passchangeproofreq, client_password_proof), 20))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] (W3) got bad PASSCHANGEPROOFREQ packet (missing hash)", conn_get_socket(c));
						packet_del_ref(rpacket);
						return -1;
					}

					if (std::memcmp(client_password_proof, conn_get_client_proof(c), 20) == 0) {
						const char * server_proof = conn_get_server_proof(c);

						for (i = 0; i < 20; i++){
							bn_byte_set(&rpacket->u.server_passchangeproofreply.server_password_proof[i], server_proof[i]);
						}

						salt = (const char *)packet_get_data_const(packet, offsetof(t_client_passchangeproofreq, salt), 32);
						verifier = (const char *)packet_get_data_const(packet, offsetof(t_client_passchangeproofreq, password_verifier), 32);
						account_set_salt(account, salt);
						account_set_verifier(account, verifier);

						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" successful passchange (right client password proof)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_passchangeproofreply.response, SERVER_PASSCHANGEPROOFREPLY_RESPONSE_OK);
					}
					else {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got wrong client password proof for \"{}\"", conn_get_socket(c), username);
						conn_increment_passfail_count(c);
					}
				}
				{
					// Strangler-fig: ship SERVER_PASSCHANGEPROOFREPLY (0x56) via
					// the v3 codec. Read response + server_password_proof back out
					// of the legacy-built rpacket so all branch logic stays intact.
					unsigned int v3_resp = bn_int_get(
						rpacket->u.server_passchangeproofreply.response);
					unsigned char const* v3_proof =
						reinterpret_cast<unsigned char const*>(
							&rpacket->u.server_passchangeproofreply.server_password_proof[0]);
					int v3rc = pvpgn_v3_send_passchangeproofreply(c, v3_resp, v3_proof);
					if (v3rc == 1) {
						packet_del_ref(rpacket);
						conn_set_client_proof(c, NULL);
						conn_set_server_proof(c, NULL);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				conn_set_client_proof(c, NULL);
				conn_set_server_proof(c, NULL);
			}
	
			return 0;
		}

}} // namespace pvpgn::bnetd
