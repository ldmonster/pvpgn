// SPDX-License-Identifier: GPL-2.0-or-later
//
// R204: relocated from `src/bnetd/handle_bot.cpp`.
//
// Strangler-fig move: the bnetd Bot/Telnet line-protocol dispatcher
// (echoing variant of the telnet flow) now lives in the v3
// `integration_legacy_bnetd_linked` library. Symbol surface preserved:
//   - `pvpgn::bnetd::handle_bot_packet`
//
// Callers in `src/bnetd/server.cpp` keep `#include "handle_bot.h"`
// (declarations only) and resolve to this definition through the
// `bnetd -> integration_legacy_bnetd_linked` link edge.
//
// The legacy `#ifdef PVPGN_V3_BNETD_INTEGRATION` observer + send-bridge
// guards are dropped: we are unconditionally inside the v3 build here.

#include "common/setup_before.h"
#include "handle_bot.h"

#include <cstring>
#include <cctype>

#include "common/eventlog.h"
#include "common/packet.h"
#include "common/bnethash.h"
#include "common/tag.h"
#include "common/xstring.h"

#include "connection.h"
#include "account.h"
#include "account_wrap.h"
#include "message.h"
#include "channel.h"
#include "command.h"
#ifdef WITH_LUA
#include "luainterface.h"
#endif
#include "common/setup_after.h"

extern "C" int pvpgn_v3_bot_dispatch_try(void* conn_ptr, char const* op) noexcept;
extern "C" int pvpgn_v3_send_raw_text(void* conn_ptr, char const* text) noexcept;
extern "C" int pvpgn_v3_send_raw_text2(void* conn_ptr, char const* prefix, char const* suffix) noexcept;


namespace pvpgn
{

	namespace bnetd
	{


		/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
		static char* handlebot(char const* s)
		{
			if (!s) return nullptr;
			std::size_t n = std::strlen(s) + 1;
			char* r = new char[n];
			std::memcpy(r, s, n);
			return r;
		}
		extern int handle_bot_packet(t_connection * c, t_packet const * const packet)
		{
			t_packet * rpacket;

			if (!c)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL connection", conn_get_socket(c));
				return -1;
			}
			if (!packet)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL packet", conn_get_socket(c));
				return -1;
			}
			if (packet_get_class(packet) != packet_class_raw)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad packet (class {})", conn_get_socket(c), (int)packet_get_class(packet));
				return -1;
			}

			(void)pvpgn_v3_bot_dispatch_try(c, conn_state_get_str(conn_get_state(c)));

			{
				char const * const linestr = packet_get_str_const(packet, 0, MAX_MESSAGE_LEN);

				if (packet_get_size(packet) < 2) /* empty line */
					return 0;
				if (!linestr)
				{
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] line too long", conn_get_socket(c));
					return 0;
				}

				switch (conn_get_state(c))
				{
				case conn_state_connected:
					conn_add_flags(c, MF_PLUG);
					conn_set_clienttag(c, CLIENTTAG_BNCHATBOT_UINT);

					{
						char const * temp = linestr;

						if (temp[0] == '\004') /* FIXME: no echo, ignore for now (we always do no echo) */
							temp = &temp[1];

						if (temp[0] == '\0') /* empty line */
						{
							conn_set_state(c, conn_state_bot_username); /* don't look for ^D or reset tag and flags */
							break;
						}

						conn_set_state(c, conn_state_bot_password);

						if (conn_set_loggeduser(c, temp) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not set username to \"{}\"", conn_get_socket(c), temp);

						{
							char const * const msg = "\r\nPassword: ";

							{
								int const _rc = pvpgn_v3_send_raw_text2(c, conn_get_loggeduser(c), msg);
								if (_rc == 1) break;
								if (_rc == -1) return -1;
							}
							if (!(rpacket = packet_create(packet_class_raw)))
							{
								eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
								break;
							}
	#if 1 /* don't echo */
							packet_append_ntstring(rpacket, conn_get_loggeduser(c));
	#endif
							packet_append_ntstring(rpacket, msg);
							conn_push_outqueue(c, rpacket);
							packet_del_ref(rpacket);
						}
					}
					break;

				case conn_state_bot_username:
					conn_set_state(c, conn_state_bot_password);

					if (conn_set_loggeduser(c, linestr) < 0)
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not set username to \"{}\"", conn_get_socket(c), linestr);

					{
						char const * const temp = "\r\nPassword: ";

						{
							int const _rc = pvpgn_v3_send_raw_text2(c, linestr, temp);
							if (_rc == 1) break;
							if (_rc == -1) return -1;
						}
						if (!(rpacket = packet_create(packet_class_raw)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
							break;
						}
	#if 1 /* don't echo */
						packet_append_ntstring(rpacket, linestr);
	#endif
						packet_append_ntstring(rpacket, temp);
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
					break;

				case conn_state_bot_password:
				{
												char const * const tempa = "\r\nLogin failed.\r\n\r\nUsername: ";
												char const * const tempb = "\r\nAccount has no bot access.\r\n\r\nUsername: ";
												char const * loggeduser = conn_get_loggeduser(c);
												t_account *        account;
												char const *       oldstrhash1;
												t_hash             trypasshash1;
												t_hash             oldpasshash1;
												char *             testpass;

												if (!loggeduser) /* error earlier in login */
												{
													/* no std::log message... */
													conn_set_state(c, conn_state_bot_username);

													{
														int const _rc = pvpgn_v3_send_raw_text(c, tempa);
														if (_rc == 1) break;
														if (_rc == -1) return -1;
													}
													if (!(rpacket = packet_create(packet_class_raw)))
													{
														eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
														break;
													}

													packet_append_ntstring(rpacket, tempa);
													conn_push_outqueue(c, rpacket);
													packet_del_ref(rpacket);
													break;
												}
												if (connlist_find_connection_by_accountname(loggeduser))
												{
													eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (already logged in)", conn_get_socket(c), loggeduser);
													conn_set_state(c, conn_state_bot_username);

													{
														int const _rc = pvpgn_v3_send_raw_text(c, tempa);
														if (_rc == 1) break;
														if (_rc == -1) return -1;
													}
													if (!(rpacket = packet_create(packet_class_raw)))
													{
														eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
														break;
													}

													packet_append_ntstring(rpacket, tempa);
													conn_push_outqueue(c, rpacket);
													packet_del_ref(rpacket);
													break;
												}
												if (!(account = accountlist_find_account(loggeduser)))
												{
													eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (bad account)", conn_get_socket(c), loggeduser);
													conn_set_state(c, conn_state_bot_username);

													{
														int const _rc = pvpgn_v3_send_raw_text(c, tempa);
														if (_rc == 1) break;
														if (_rc == -1) return -1;
													}
													if (!(rpacket = packet_create(packet_class_raw)))
													{
														eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
														break;
													}

													packet_append_ntstring(rpacket, tempa);
													conn_push_outqueue(c, rpacket);
													packet_del_ref(rpacket);
													break;
												}
												if ((oldstrhash1 = account_get_pass(account)))
												{
													if (hash_set_str(&oldpasshash1, oldstrhash1) < 0)
													{
														eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (corrupted passhash1?)", conn_get_socket(c), loggeduser);
														conn_set_state(c, conn_state_bot_username);

														{
															int const _rc = pvpgn_v3_send_raw_text(c, tempa);
															if (_rc == 1) break;
															if (_rc == -1) return -1;
														}
														if (!(rpacket = packet_create(packet_class_raw)))
														{
															eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
															break;
														}

														packet_append_ntstring(rpacket, tempa);
														conn_push_outqueue(c, rpacket);
														packet_del_ref(rpacket);
														break;
													}

													testpass = handlebot(linestr);
													{
														strtolower(testpass);
													}
													if (bnet_hash(&trypasshash1, std::strlen(testpass), testpass) < 0) /* FIXME: force to lowercase */
													{
														eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (unable to hash password)", conn_get_socket(c), loggeduser);
														conn_set_state(c, conn_state_bot_username);

														delete[] testpass;

														{
															int const _rc = pvpgn_v3_send_raw_text(c, tempa);
															if (_rc == 1) break;
															if (_rc == -1) return -1;
														}
														if (!(rpacket = packet_create(packet_class_raw)))
														{
															eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
															break;
														}

														packet_append_ntstring(rpacket, tempa);
														conn_push_outqueue(c, rpacket);
														packet_del_ref(rpacket);
														break;
													}
													delete[] testpass;
													if (hash_eq(trypasshash1, oldpasshash1) != 1)
													{
														eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (wrong password)", conn_get_socket(c), loggeduser);
														conn_set_state(c, conn_state_bot_username);

														{
															int const _rc = pvpgn_v3_send_raw_text(c, tempa);
															if (_rc == 1) break;
															if (_rc == -1) return -1;
														}
														if (!(rpacket = packet_create(packet_class_raw)))
														{
															eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
															break;
														}

														packet_append_ntstring(rpacket, tempa);
														conn_push_outqueue(c, rpacket);
														packet_del_ref(rpacket);
														break;
													}


													if (account_get_auth_botlogin(account) != 1) /* default to false */
													{
														eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (no bot access)", conn_get_socket(c), loggeduser);
														conn_set_state(c, conn_state_bot_username);

														{
															int const _rc = pvpgn_v3_send_raw_text(c, tempb);
															if (_rc == 1) break;
															if (_rc == -1) return -1;
														}
														if (!(rpacket = packet_create(packet_class_raw)))
														{
															eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
															break;
														}

														packet_append_ntstring(rpacket, tempb);
														conn_push_outqueue(c, rpacket);
														packet_del_ref(rpacket);
														break;
													}
													else if (account_get_auth_lock(account) == 1) /* default to false */
													{
														eventlog(eventlog_level_info, __FUNCTION__, "[{}] bot login for \"{}\" refused (this account is locked)", conn_get_socket(c), loggeduser);
														conn_set_state(c, conn_state_bot_username);

														{
															int const _rc = pvpgn_v3_send_raw_text(c, tempb);
															if (_rc == 1) break;
															if (_rc == -1) return -1;
														}
														if (!(rpacket = packet_create(packet_class_raw)))
														{
															eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
															break;
														}

														packet_append_ntstring(rpacket, tempb);
														conn_push_outqueue(c, rpacket);
														packet_del_ref(rpacket);
														break;
													}

													eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" bot logged in (correct password)", conn_get_socket(c), loggeduser);
#ifdef WITH_LUA
													if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
													{
														// feature to break login from Lua
														conn_set_state(c, conn_state_destroy);
														break;
													}
#endif
												}
												else
												{
													eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" bot logged in (no password)", conn_get_socket(c), loggeduser);
												}
												{
													int const _rc = pvpgn_v3_send_raw_text(c, "\r\n");
													if (_rc != 1) {
														if (!(rpacket = packet_create(packet_class_raw))) /* if we got this far, let them std::log in even if this fails */
															eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create rpacket", conn_get_socket(c));
														else
														{
															packet_append_ntstring(rpacket, "\r\n");
															conn_push_outqueue(c, rpacket);
															packet_del_ref(rpacket);
														}
													}
												}

												conn_login(c, account, loggeduser);

												message_send_text(c, message_type_uniqueid, c, loggeduser);

												if (conn_set_channel(c, CHANNEL_NAME_CHAT) < 0)
													conn_set_channel(c, CHANNEL_NAME_BANNED); /* should not fail */
				}
					break;

				case conn_state_loggedin:
				{
											t_channel const * channel;

											conn_set_idletime(c);

											if ((channel = conn_get_channel(c)))
												channel_message_log(channel, c, 1, linestr);
											/* we don't log game commands currently */

											if (linestr[0] == '/')
												handle_command(c, linestr);
											else
											if (channel && !conn_quota_exceeded(c, linestr))
												channel_message_send(channel, message_type_talk, c, linestr);
											/* else discard */
				}
					break;

				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown bot connection state {}", conn_get_socket(c), (int)conn_get_state(c));
				}
			}

			return 0;
		}

	}

}
