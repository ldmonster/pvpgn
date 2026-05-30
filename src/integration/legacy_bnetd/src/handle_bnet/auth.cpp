// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_createaccountw3(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch_try(c, "createaccountw3");
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
			(void)pvpgn_v3_account_dispatch_try(c, "createacctreq1");
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
			(void)pvpgn_v3_account_dispatch_try(c, "createacctreq2");
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

		int _client_changepassreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_account_dispatch_try(c, "changepassreq");
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

		int _client_echoreply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_keepalive_dispatch_try(c, "echoreply");
			if (packet_get_size(packet) < sizeof(t_client_echoreply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ECHOREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_echoreply), packet_get_size(packet));
				return -1;
			}

			{
				unsigned int now;
				unsigned int then;

				now = get_ticks();
				then = bn_int_get(packet->u.client_echoreply.ticks);
				if (!now || !then || now < then)
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] bad timing in echo reply: now={} then={}", conn_get_socket(c), now, then);
				else
					conn_set_latency(c, now - then);
			}

			return 0;
		}

		int _client_authreq1(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_auth_dispatch_try(c, "authreq1");
			eventlog(eventlog_level_trace, __FUNCTION__, "[{}] received AUTHREQ1(0x07) packet", conn_get_socket(c));

			if (packet_get_size(packet) < sizeof(t_client_authreq1))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_authreq1), packet_get_size(packet));
				return -1;
			}

			auto send_failed_packet = [](t_connection *c)
			{
				conn_set_state(c, conn_state_untrusted);
				// v3 strangler-fig: build BADVERSION reply via the
				// codec and ship through the registered handler.
				if (pvpgn_v3_send_authreply1(
				        c, SERVER_AUTHREPLY1_MESSAGE_BADVERSION,
				        nullptr) == 1)
				{
					return;
				}
				t_packet *rpacket = packet_create(packet_class_bnet);
				if (rpacket)
				{
					packet_set_size(rpacket, sizeof(t_server_authreply1));
					packet_set_type(rpacket, SERVER_AUTHREPLY1);

					bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_BADVERSION);
					packet_append_string(rpacket, "");
					packet_append_string(rpacket, ""); // undocumented extra null terminator

					conn_push_outqueue(c, rpacket);
					
					packet_del_ref(rpacket);
				}
			};

			// The following if statements are sanity checks
			// The client should have already sent this information in a previous packet and is resending it again in this packet
			if (bn_int_get(packet->u.client_authreq1.archtag) != conn_get_archtag(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch architecture)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}

			if (bn_int_get(packet->u.client_authreq1.clienttag) != conn_get_clienttag(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch client tag)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}

			if (bn_int_get(packet->u.client_authreq1.versionid) != conn_get_versionid(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch version ID)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}


			const char *exeinfo = packet_get_str_const(packet, sizeof(t_client_authreq1), MAX_EXEINFO_STR);
			if (exeinfo)
			{
				conn_set_clientexe(c, exeinfo);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (missing or too long exeinfo)", conn_get_socket(c));
				send_failed_packet(c);
				return 0;
			}

			std::string version_string = vernum_to_verstr(bn_int_get(packet->u.client_authreq1.gameversion));
			conn_set_clientver(c, version_string.c_str());
			eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_AUTHREQ1 archtag=0x{:08x} clienttag=0x{:08x} verstr={} exeinfo=\"{}\" versionid=0x{:08x} gameversion=0x{:08x} checksum=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_authreq1.archtag), bn_int_get(packet->u.client_authreq1.clienttag), version_string, exeinfo, conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));

			conn_set_versionid(c, bn_int_get(packet->u.client_authreq1.versionid));
			conn_set_checksum(c, bn_int_get(packet->u.client_authreq1.checksum));
			conn_set_gameversion(c, bn_int_get(packet->u.client_authreq1.gameversion));

			
			const VersionCheck* vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
			conn_set_versioncheck(c, vc);
			if (vc)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] client matches versiontag \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag());
			}
			else
			{
				if (prefs_v3::allow_unknown_version())
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] skipping versioncheck because allow_unknown_version is true", conn_get_socket(c));
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client failed versioncheck", conn_get_socket(c));
					send_failed_packet(c);
					return 0;
				}
			}


			t_packet *rpacket = packet_create(packet_class_bnet);
			if (rpacket)
			{
				packet_set_size(rpacket, sizeof(t_server_authreply1));
				packet_set_type(rpacket, SERVER_AUTHREPLY1);

				char *mpqfilename = nullptr;
				if (vc)
				{
					mpqfilename = autoupdate_check(conn_get_archtag(c), conn_get_clienttag(c), conn_get_gamelang(c), vc->get_version_tag().c_str(), nullptr);
				}

				// Only handle updates when there is an update file available.
				if (mpqfilename)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] an upgrade for version {} is available \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag(), mpqfilename);
					bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_UPDATE);
					packet_append_string(rpacket, mpqfilename);
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] no upgrade is available", conn_get_socket(c));
				}

				bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_OK);
				packet_append_string(rpacket, "");
				packet_append_string(rpacket, ""); // FIXME: what's the second string for?

				// v3 strangler-fig: emit the reply via the v3 codec
				// instead of the legacy packet, when a send_packet
				// handler is installed. Mirrors the legacy quirk
				// that the message code is OK even when a mpq
				// filename is present (filename is prepended; two
				// trailing empties are always appended).
				if (pvpgn_v3_send_authreply1(
				        c, SERVER_AUTHREPLY1_MESSAGE_OK,
				        mpqfilename) == 1)
				{
					packet_del_ref(rpacket);
					if (mpqfilename)
						delete[] mpqfilename;
					return 0;
				}

				if (mpqfilename)
					delete[] mpqfilename;

				conn_push_outqueue(c, rpacket);

				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_authreq109(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_auth_dispatch_try(c, "authreq109");
			if (packet_get_size(packet) < sizeof(t_client_authreq_109))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_authreq_109), packet_get_size(packet));
				return 0;
			}

			auto send_failed_packet = [](t_connection *c)
			{
				conn_set_state(c, conn_state_untrusted);
				if (pvpgn_v3_send_authreply109(
				        c, SERVER_AUTHREPLY_109_MESSAGE_BADVERSION,
				        nullptr) == 1)
				{
					return;
				}
				t_packet *rpacket = packet_create(packet_class_bnet);
				if (rpacket)
				{
					packet_set_size(rpacket, sizeof(t_server_authreply_109));
					packet_set_type(rpacket, SERVER_AUTHREPLY_109);

					bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_BADVERSION);
					packet_append_string(rpacket, "");

					conn_push_outqueue(c, rpacket);

					packet_del_ref(rpacket);
				}
			};


			std::uint32_t count = bn_int_get(packet->u.client_authreq_109.cdkey_number);
			std::size_t position = sizeof(t_client_authreq_109) + (count * sizeof(t_cdkey_info));

			const char *const exeinfo = packet_get_str_const(packet, position, MAX_EXEINFO_STR);
			if (exeinfo)
			{
				conn_set_clientexe(c, exeinfo);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 (missing or too long exeinfo)", conn_get_socket(c));
				send_failed_packet(c);
				return 0;
			}

			position += std::strlen(exeinfo) + 1;

			const char *const owner = packet_get_str_const(packet, position, MAX_OWNER_STR);
			if (owner)
			{
				conn_set_owner(c, owner);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 (missing or too long owner)", conn_get_socket(c));
				conn_set_owner(c, "");
			}

			conn_set_checksum(c, bn_int_get(packet->u.client_authreq_109.checksum));
			conn_set_gameversion(c, bn_int_get(packet->u.client_authreq_109.gameversion));
			std::string version = vernum_to_verstr(bn_int_get(packet->u.client_authreq_109.gameversion));
			conn_set_clientver(c, version.c_str());

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_AUTHREQ_109 ticks=0x{:08x}, verstr={} exeinfo=\"{}\" versionid=0x{:08x} gameversion=0x{:08x} checksum=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_authreq_109.ticks), version, exeinfo, conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));

			t_packet *rpacket;
			if ((rpacket = packet_create(packet_class_bnet)))
			{
				packet_set_size(rpacket, sizeof(t_server_authreply_109));
				packet_set_type(rpacket, SERVER_AUTHREPLY_109);


				const VersionCheck* vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
				conn_set_versioncheck(c, vc);
				if (vc)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client matches versiontag \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag());
				}
				else
				{
					if (prefs_v3::allow_unknown_version())
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] skipping versioncheck because allow_unknown_version is true", conn_get_socket(c));
					}
					else
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] client failed versioncheck", conn_get_socket(c));
						packet_del_ref(rpacket);
						send_failed_packet(c);
						return 0;
					}
				}


				char *mpqfilename = nullptr;
				if (vc)
				{
					mpqfilename = autoupdate_check(conn_get_archtag(c), conn_get_clienttag(c), conn_get_gamelang(c), conn_get_versioncheck(c)->get_version_tag().c_str(), NULL);
				}

				// Only handle updates when there is an update file available.
				if (mpqfilename)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] an upgrade for {} is available \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag(), mpqfilename);
					bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_UPDATE);
					packet_append_string(rpacket, mpqfilename);
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] no upgrade is available", conn_get_socket(c));
				}

				bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_OK);
				packet_append_string(rpacket, "");

				// v3 strangler-fig: emit via the v3 codec when the
				// send_packet handler is installed. Mirrors the
				// legacy quirk that the final message code is OK
				// regardless of whether an update filename was
				// prepended.
				if (pvpgn_v3_send_authreply109(
				        c, SERVER_AUTHREPLY_109_MESSAGE_OK,
				        mpqfilename) == 1)
				{
					if (mpqfilename)
						delete[] mpqfilename;
					packet_del_ref(rpacket);
					return 0;
				}

				if (mpqfilename)
					delete[] mpqfilename;

				conn_push_outqueue(c, rpacket);

				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_regsnoopreply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_stub_dispatch_try(c, "regsnoopreply");
			if (packet_get_size(packet) < sizeof(t_client_regsnoopreply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REGSNOOPREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_regsnoopreply), packet_get_size(packet));
				return -1;
			}
			return 0;
		}

		int _client_iconreq(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_iconreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ICONREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_iconreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_iconreply));
				packet_set_type(rpacket, SERVER_ICONREPLY);
				file_to_mod_time(c, prefs_v3::iconfile(), &rpacket->u.server_iconreply.timestamp);

				/* battle.net sends different file on iconreq for WAR3 and W3XP [Omega] */
				if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT) || (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
					packet_append_string(rpacket, prefs_v3::war3_iconfile());
				/* battle.net still sends "icons.bni" to sc/bw clients
				 * clients request icons_STAR.bni seperatly */
				/*	else if (std::strcmp(conn_get_clienttag(c),CLIENTTAG_STARCRAFT)==0)
						packet_append_string(rpacket,prefs_v3::star_iconfile());
						else if (std::strcmp(conn_get_clienttag(c),CLIENTTAG_BROODWARS)==0)
						packet_append_string(rpacket,prefs_v3::star_iconfile());
						*/
				else
					packet_append_string(rpacket, prefs_v3::iconfile());

				{
					// Pull timestamp + appended filename back out of the
					// legacy-built rpacket so all branch logic stays intact.
					unsigned long long v3_ts = bn_long_get(
						rpacket->u.server_iconreply.timestamp);
					char const* v3_filename = nullptr;
					if (packet_get_size(rpacket)
					    > sizeof(t_server_iconreply)) {
						v3_filename = reinterpret_cast<char const*>(
							packet_get_data_const(
								rpacket,
								sizeof(t_server_iconreply),
								1));
					}
					if (pvpgn_v3_send_iconreply(c, v3_ts, v3_filename) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_cdkey(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_cdkey_dispatch_try(c, "cdkey");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey), packet_get_size(packet));
				return -1;
			}

			{
				char const *cdkey;
				char const *owner;

				if (!(cdkey = packet_get_str_const(packet, sizeof(t_client_cdkey), MAX_CDKEY_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (missing or too long cdkey)", conn_get_socket(c));
					return -1;
				}
				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey)+std::strlen(cdkey) + 1, MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_cdkey(c, cdkey);
				conn_set_owner(c, owner);

				if (pvpgn_v3_send_cdkeyreply(c,
				                             SERVER_CDKEYREPLY_MESSAGE_OK,
				                             owner) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply));
					packet_set_type(rpacket, SERVER_CDKEYREPLY);
					bn_int_set(&rpacket->u.server_cdkeyreply.message, SERVER_CDKEYREPLY_MESSAGE_OK);
					packet_append_string(rpacket, owner);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}
#if 0				/* Blizzard used this to track down pirates, should only be accepted by old clients */
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_regsnoopreq));
				packet_set_type(rpacket, SERVER_REGSNOOPREQ);
				bn_int_set(&rpacket->u.server_regsnoopreq.unknown1, SERVER_REGSNOOPREQ_UNKNOWN1);	/* sequence num */
				bn_int_set(&rpacket->u.server_regsnoopreq.hkey, SERVER_REGSNOOPREQ_HKEY_CURRENT_USER);
				packet_append_string(rpacket, SERVER_REGSNOOPREQ_REGKEY);
				packet_append_string(rpacket, SERVER_REGSNOOPREQ_REGVALNAME);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
#endif
			return 0;
		}

		int _client_cdkey2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_cdkey_dispatch_try(c, "cdkey2");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey2), packet_get_size(packet));
				return -1;
			}

			{
				char const *owner;

				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey2), MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY2 packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_owner(c, owner);

				if (pvpgn_v3_send_cdkeyreply2(c,
				                              SERVER_CDKEYREPLY2_MESSAGE_OK,
				                              owner) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply2));
					packet_set_type(rpacket, SERVER_CDKEYREPLY2);
					bn_int_set(&rpacket->u.server_cdkeyreply2.message, SERVER_CDKEYREPLY2_MESSAGE_OK);
					packet_append_string(rpacket, owner);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		int _client_cdkey3(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_cdkey_dispatch_try(c, "cdkey3");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey2), packet_get_size(packet));
				return -1;
			}

			{
				char const *owner;

				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey3), MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY3 packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_owner(c, owner);

				if (pvpgn_v3_send_cdkeyreply3(c,
				                              SERVER_CDKEYREPLY3_MESSAGE_OK,
				                              nullptr) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply3));
					packet_set_type(rpacket, SERVER_CDKEYREPLY3);
					bn_int_set(&rpacket->u.server_cdkeyreply3.message, SERVER_CDKEYREPLY3_MESSAGE_OK);
					packet_append_string(rpacket, "");	/* FIXME: owner, message, ??? */
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
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

		int _client_passchangereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_passemail_dispatch_try(c, "passchangereq");
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
			(void)pvpgn_v3_passemail_dispatch_try(c, "passchangeproofreq");
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

		int _client_pingreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_keepalive_dispatch_try(c, "pingreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_pingreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PINGREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_pingreq), packet_get_size(packet));
				return -1;
			}

				if (pvpgn_v3_send_pingreply(c) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_pingreply));
					packet_set_type(rpacket, SERVER_PINGREPLY);
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
