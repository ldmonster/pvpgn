// =====================================================================
// auth_account.cpp — Account creation handlers
// Split from auth.cpp (plan 15 §3 / SOLID-S refactor)
// Each function handles one account-creation packet type.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_createaccountw3(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch(c, "createaccountw3");
			char const *username;
			char const *plainpass;
			t_hash sc_hash;
			unsigned int i;
			const char *account_salt;
			const char *account_verifier;
			t_account *account;
			bool presume_plainpass = true;

			if (packet_get_size(packet) < sizeof(t_client_createaccount_w3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCOUNT_W3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_createaccount_w3), packet_get_size(packet));
				return -1;
			}

			username = packet_get_str_const(packet, sizeof(t_client_createaccount_w3), UNCHECKED_NAME_STR);
			if (!username) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCOUNT_W3 (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			/* PELISH: We are testing if username missing and if packetsize is good
					   so we does not need to test if salt and verifier are present */
			account_salt = (const char *)packet_get_data_const(packet, offsetof(t_client_createaccount_w3, salt), 32);
			account_verifier = (const char *)packet_get_data_const(packet, offsetof(t_client_createaccount_w3, password_verifier), 32);

			for (i = 0; i<16; i++){
				int value = (unsigned char)account_verifier[i];
				if (value == 0)
					break;
				presume_plainpass &= (std::isprint(value)>0);
			}

			for (i = 16; (i < 32) && presume_plainpass; i++){
				int value = account_verifier[i];
				presume_plainpass &= (value == 0);
			}

			if (presume_plainpass)
				eventlog(eventlog_level_debug, __FUNCTION__, "This looks more like a plaintext password than like a verifier");

			plainpass = packet_get_str_const(packet, offsetof(t_client_createaccount_w3, password_verifier), 16);
			if (presume_plainpass && !plainpass) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCOUNT_W3 (missing password)", conn_get_socket(c));
				return -1;
			}

			t_packet* const rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;

			packet_set_size(rpacket, sizeof(t_server_createaccount_w3));
			packet_set_type(rpacket, SERVER_CREATEACCOUNT_W3);

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\"", conn_get_socket(c), username);

			if (prefs_v3::allow_new_accounts() == 0)
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (disabled)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_EXIST);
				if (pvpgn_v3_send_createaccount_w3(c,
						bn_int_get(rpacket->u.server_createaccount_w3.result)) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				return 0;
			}

			if (account_check_name(username) < 0)
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (invalid symbols)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_INVALID);
				if (pvpgn_v3_send_createaccount_w3(c,
						bn_int_get(rpacket->u.server_createaccount_w3.result)) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				return 0;
			}

			char lpass[20] = {};
			if (plainpass)
			{
				/* convert plaintext password to lowercase for sc etc. */
				std::snprintf(lpass, sizeof lpass, "%s", plainpass);
				strtolower(lpass);
			}

			//set password hash for sc etc.
			bnet_hash(&sc_hash, std::strlen(lpass), lpass);
			if (!(account = accountlist_create_account(username, hash_get_str(sc_hash))))
			{
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_EXIST);
			}
			else
			{
				if (presume_plainpass)
				{
					BigInt salt = BigInt((unsigned char*)account_salt, 32, 4, false);
					BnetSRP3 srp3 = BnetSRP3(username, plainpass);
					srp3.setSalt(salt);
					BigInt verifier = srp3.getVerifier();
					account_verifier = (const char*)verifier.getData(32, 4, false);
				}
				else {
					//NEED TO TAG ACCOUNT AS "SC PASS BROKEN"
				}
				account_set_salt(account, account_salt);
				account_set_verifier(account, account_verifier);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account created", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_OK);
			}

			{
				unsigned int v3_result = bn_int_get(
					rpacket->u.server_createaccount_w3.result);
				if (pvpgn_v3_send_createaccount_w3(c, v3_result) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
			}
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		int _client_createacctreq1(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch(c, "createacctreq1");
			t_packet *rpacket;
			char const *username;
			t_hash newpasshash1;

			if (packet_get_size(packet) < sizeof(t_client_createacctreq1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCTREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_createacctreq1), packet_get_size(packet));
				return -1;
			}

			if (!(username = packet_get_str_const(packet, sizeof(t_client_createacctreq1), UNCHECKED_NAME_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCTREQ1 (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\"", conn_get_socket(c), username);

			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;
			packet_set_size(rpacket, sizeof(t_server_createacctreply1));
			packet_set_type(rpacket, SERVER_CREATEACCTREPLY1);

			if (prefs_v3::allow_new_accounts() == 0) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (disabled)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply1.result, SERVER_CREATEACCTREPLY1_RESULT_NO);
				goto out;
			}

			bnhash_to_hash(packet->u.client_createacctreq1.password_hash1, &newpasshash1);
			if (!accountlist_create_account(username, hash_get_str(newpasshash1))) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (failed)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply1.result, SERVER_CREATEACCTREPLY1_RESULT_NO);
				goto out;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account created", conn_get_socket(c));
			bn_int_set(&rpacket->u.server_createacctreply1.result, SERVER_CREATEACCTREPLY1_RESULT_OK);

		out:
			{
				unsigned int v3_result = bn_int_get(
					rpacket->u.server_createacctreply1.result);
				int v3rc = pvpgn_v3_send_createacctreply1(c, v3_result);
				if (v3rc == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
			}
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		int _client_createacctreq2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch(c, "createacctreq2");
			t_packet *rpacket;
			char const *username;
			t_hash newpasshash1;

			if (packet_get_size(packet) < sizeof(t_client_createacctreq2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_CREATEACCTREQ2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_createacctreq2), packet_get_size(packet));
				return -1;
			}

			username = packet_get_str_const(packet, sizeof(t_client_createacctreq2), UNCHECKED_NAME_STR);
			if (!username) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCTREQ2 (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\"", conn_get_socket(c), username);

			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;
			packet_set_size(rpacket, sizeof(t_server_createacctreply2));
			packet_set_type(rpacket, SERVER_CREATEACCTREPLY2);

			if (prefs_v3::allow_new_accounts() == 0) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (disabled)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply2.result, SERVER_CREATEACCTREPLY2_RESULT_EXIST);
				goto out;
			}

			if (account_check_name(username) < 0) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (invalid symbols)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCTREPLY2_RESULT_INVALID);
				goto out;
			}

			bnhash_to_hash(packet->u.client_createacctreq2.password_hash1, &newpasshash1);
			if (!accountlist_create_account(username, hash_get_str(newpasshash1))) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (failed)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply2.result, SERVER_CREATEACCTREPLY2_RESULT_EXIST);	/* FIXME: return reason for failure */
				goto out;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account created", conn_get_socket(c));
			bn_int_set(&rpacket->u.server_createacctreply2.result, SERVER_CREATEACCTREPLY2_RESULT_OK);

		out:
			{
				unsigned int v3_result = bn_int_get(
					rpacket->u.server_createacctreply2.result);
				int v3rc = pvpgn_v3_send_createacctreply2(c, v3_result);
				if (v3rc == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
			}
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

}} // namespace pvpgn::bnetd
