/*
* Copyright (C) 2014  HarpyWar (harpywar@gmail.com)
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#define GAME_INTERNAL_ACCESS
#ifdef WITH_LUA
#include "common/setup_before.h"

#include "team.h"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include <strings.h>
#include "infra/compat/directory.hpp"
#include "common/tag.h"
#include "common/util.h"
#include "common/eventlog.h"

#include "connection.h"
#include "message.h"
#include "channel.h"
#include "game.h"
#include "account.h"
#include "account_wrap.h"
#include "timer.h"
#include "ipban.h"
#include "command_groups.h"
#include "friends.h"
#include "clan.h"
#include "prefs_v3_shim.h"


#include "luawrapper.h"
#include "luainterface.h"
#include "luafunctions.h"
#include "luaobjects.h"


#include "common/setup_after.h"


namespace pvpgn
{

	namespace bnetd
	{
		lua::vm vm;

		/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
		static char* lua_strdup(char const* s)
		{
			if (!s) return nullptr;
			std::size_t n = std::strlen(s) + 1;
			char* r = new char[n];
			std::memcpy(r, s, n);
			return r;
		}

		char _msgtemp[MAX_MESSAGE_LEN];
		char _msgtemp2[MAX_MESSAGE_LEN];


		void _register_functions();


		/* Unload all the lua scripts */
		extern void lua_unload()
		{
			// nothing to do, "vm.initialize()" already destroys lua vm before initialize
		}

		/* Initialize lua, register functions and load scripts */
		extern void lua_load(char const * scriptdir)
		{
			eventlog(eventlog_level_info, __FUNCTION__, "Loading Lua interface...");

			try
			{
				// init lua virtual machine
				vm.initialize();

				namespace dir = pvpgn::v3::infra::compat;
				auto raw_files = dir::list_files(scriptdir, ".lua", true);
				std::vector<std::string> files;
				files.reserve(raw_files.size());
				for (const auto& p : raw_files) files.push_back(p.string());
	
				// load all files from the script directory
				for (int i = 0; i < (int)files.size(); ++i)
				{
					vm.load_file(files[i].c_str());
	
					std::snprintf(_msgtemp, sizeof(_msgtemp), "%s", files[i].c_str());
					eventlog(eventlog_level_info, __FUNCTION__, "{}", _msgtemp);
				}
	
				_register_functions();
	
				std::snprintf(_msgtemp, sizeof(_msgtemp), "Lua sripts were successfully loaded (%zu files)", files.size());
				eventlog(eventlog_level_info, __FUNCTION__, "{}", _msgtemp);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}

			// handle start event
			lua_handle_server(luaevent_server_start);
		}


		/* Register C++ functions to be able use them from lua scripts */
		void _register_functions()
		{
			// register package 'api' with functions
			static const luaL_Reg api[] =
			{
				{ "message_send_text", __message_send_text },
				{ "eventlog", __eventlog },
				{ "account_get_by_id", __account_get_by_id },
				{ "account_get_by_name", __account_get_by_name },
				{ "account_get_attr", __account_get_attr },
				{ "account_set_attr", __account_set_attr },
				{ "account_get_friends", __account_get_friends },
				{ "account_get_teams", __account_get_teams },
				{ "clan_get_members", __clan_get_members },

				{ "game_get_by_id", __game_get_by_id },
				{ "game_get_by_name", __game_get_by_name },

				{ "channel_get_by_id", __channel_get_by_id },

				{ "server_get_users", __server_get_users },
				{ "server_get_games", __server_get_games },
				{ "server_get_channels", __server_get_channels },

				{ "client_kill", __client_kill },
				{ "client_readmemory", __client_readmemory },
				{ "client_requiredwork", __client_requiredwork },

				{ "command_get_group", __command_get_group },
				{ "icon_get_rank", __icon_get_rank },
				{ "describe_command", __describe_command },
				{ "messagebox_show", __messagebox_show },

				{ "localize", __localize },

				{ 0, 0 }
			};
			vm.reg("api", api);

			// register standalone functions
			//vm.reg("sum", _sum); // (test function)



			// global variables
			lua::table g(vm);
			g.update("PVPGN_SOFTWARE", PVPGN_SOFTWARE);
			g.update("PVPGN_VERSION", PVPGN_VERSION);

			// config variables from bnetd.conf
			lua::transaction bind(vm);
			bind.lookup("config");
			{
				lua::table config = bind.table();
				config.update("filedir", prefs_v3::filedir());
				config.update("i18ndir", prefs_v3::i18ndir());
				config.update("scriptdir", prefs_v3::scriptdir());
				config.update("reportdir", prefs_v3::reportdir());
				config.update("chanlogdir", prefs_v3::chanlogdir());
				config.update("userlogdir", prefs_v3::userlogdir());
				config.update("localizefile", prefs_v3::localizefile());
				config.update("motdfile", prefs_v3::motdfile());
				config.update("motdw3file", prefs_v3::motdw3file());
				config.update("issuefile", prefs_v3::issuefile());
				config.update("channelfile", prefs_v3::channelfile());
				config.update("newsfile", prefs_v3::newsfile());
				config.update("adfile", prefs_v3::adfile());
				config.update("topicfile", prefs_v3::topicfile());
				config.update("ipbanfile", prefs_v3::ipbanfile());
				config.update("helpfile", prefs_v3::helpfile());
				config.update("mpqfile", prefs_v3::mpqfile());
				config.update("logfile", prefs_v3::logfile());
				config.update("realmfile", prefs_v3::realmfile());
				config.update("maildir", prefs_v3::maildir());
				config.update("versioncheck_file", prefs_v3::versioncheck_file());
				config.update("mapsfile", prefs_v3::mapsfile());
				config.update("xplevelfile", prefs_v3::xplevel_file());
				config.update("xpcalcfile", prefs_v3::xpcalc_file());
				config.update("ladderdir", prefs_v3::ladderdir());
				config.update("command_groups_file", prefs_v3::command_groups_file());
				config.update("tournament_file", prefs_v3::tournament_file());
				config.update("statusdir", prefs_v3::outputdir());
				config.update("aliasfile", prefs_v3::aliasfile());
				config.update("anongame_infos_file", prefs_v3::anongame_infos_file());
				config.update("DBlayoutfile", prefs_v3::DBlayoutfile());
				config.update("supportfile", prefs_v3::supportfile());
				config.update("transfile", prefs_v3::transfile());
				config.update("customicons_file", prefs_v3::customicons_file());
				config.update("loglevels", prefs_v3::loglevels());
				config.update("d2cs_version", prefs_v3::d2cs_version());
				config.update("allow_d2cs_setname", prefs_v3::allow_d2cs_setname());
				config.update("iconfile", prefs_v3::iconfile());
				config.update("war3_iconfile", prefs_v3::war3_iconfile());
				config.update("star_iconfile", prefs_v3::star_iconfile());
				config.update("tosfile", prefs_v3::tosfile());
				config.update("allowed_clients", prefs_v3::allowed_clients());
				config.update("allow_bad_version", prefs_v3::allow_bad_version());
				config.update("allow_unknown_version", prefs_v3::allow_unknown_version());
				config.update("usersync", prefs_v3::user_sync_timer());
				config.update("userflush", prefs_v3::user_flush_timer());
				config.update("userflush_connected", prefs_v3::user_flush_connected());
				config.update("userstep", prefs_v3::user_step());
				config.update("latency", prefs_v3::latency());
				config.update("nullmsg", prefs_v3::nullmsg());
				config.update("shutdown_delay", prefs_v3::shutdown_delay());
				config.update("shutdown_decr", prefs_v3::shutdown_decr());
				config.update("ipban_check_int", prefs_v3::ipban_check_int());
				config.update("new_accounts", prefs_v3::allow_new_accounts());
				config.update("max_accounts", prefs_v3::max_accounts());
				config.update("kick_old_login", prefs_v3::kick_old_login());
				config.update("ask_new_channel", prefs_v3::ask_new_channel());
				config.update("report_all_games", prefs_v3::report_all_games());
				config.update("report_diablo_games", prefs_v3::report_diablo_games());
				config.update("hide_pass_games", prefs_v3::hide_pass_games());
				config.update("hide_started_games", prefs_v3::hide_started_games());
				config.update("hide_temp_channels", prefs_v3::hide_temp_channels());
				config.update("disc_is_loss", prefs_v3::discisloss());
				config.update("ladder_games", prefs_v3::ladder_games());
				config.update("ladder_prefix", prefs_v3::ladder_prefix());
				config.update("enable_conn_all", prefs_v3::enable_conn_all());
				config.update("hide_addr", prefs_v3::hide_addr());
				config.update("chanlog", prefs_v3::chanlog());
				config.update("quota", prefs_v3::quota());
				config.update("quota_lines", prefs_v3::quota_lines());
				config.update("quota_time", prefs_v3::quota_time());
				config.update("quota_wrapline", prefs_v3::quota_wrapline());
				config.update("quota_maxline", prefs_v3::quota_maxline());
				config.update("quota_dobae", prefs_v3::quota_dobae());
				config.update("mail_support", prefs_v3::mail_support());
				config.update("mail_quota", prefs_v3::mail_quota());
				config.update("log_notice", prefs_v3::log_notice());
				config.update("passfail_count", prefs_v3::passfail_count());
				config.update("passfail_bantime", prefs_v3::passfail_bantime());
				config.update("maxusers_per_channel", prefs_v3::maxusers_per_channel());
				config.update("savebyname", prefs_v3::savebyname());
				config.update("sync_on_logoff", prefs_v3::sync_on_logoff());
				config.update("hashtable_size", prefs_v3::hashtable_size());
				config.update("account_allowed_symbols", prefs_v3::account_allowed_symbols());
				config.update("account_force_username", prefs_v3::account_force_username());
				config.update("max_friends", prefs_v3::max_friends());
				config.update("track", prefs_v3::track());
				config.update("trackaddrs", prefs_v3::trackserv_addrs());
				config.update("location", prefs_v3::location());
				config.update("description", prefs_v3::description());
				config.update("url", prefs_v3::url());
				config.update("contact_name", prefs_v3::contact_name());
				config.update("contact_email", prefs_v3::contact_email());
				config.update("servername", prefs_v3::servername());
				config.update("max_connections", prefs_v3::max_connections());
				config.update("packet_limit", prefs_v3::packet_limit());
				config.update("max_concurrent_logins", prefs_v3::max_concurrent_logins());
				config.update("use_keepalive", prefs_v3::use_keepalive());
				config.update("max_conns_per_IP", prefs_v3::max_conns_per_IP());
				config.update("servaddrs", prefs_v3::bnetdserv_addrs());
				config.update("udptest_port", prefs_v3::udptest_port());
				config.update("w3routeaddr", prefs_v3::w3route_addr());
				config.update("initkill_timer", prefs_v3::initkill_timer());
				config.update("wolv1addrs", prefs_v3::wolv1_addrs());
				config.update("wolv2addrs", prefs_v3::wolv2_addrs());
				config.update("wgameresaddrs", prefs_v3::wgameres_addrs());
				config.update("apiregaddrs", prefs_v3::apireg_addrs());
				config.update("woltimezone", prefs_v3::wol_timezone());
				config.update("wollongitude", prefs_v3::wol_longitude());
				config.update("wollatitude", prefs_v3::wol_latitude());
				config.update("wol_autoupdate_serverhost", prefs_v3::wol_autoupdate_serverhost());
				config.update("wol_autoupdate_username", prefs_v3::wol_autoupdate_username());
				config.update("wol_autoupdate_password", prefs_v3::wol_autoupdate_password());
				config.update("ircaddrs", prefs_v3::irc_addrs());
				config.update("irc_network_name", prefs_v3::irc_network_name());
				config.update("hostname", prefs_v3::hostname());
				config.update("irc_latency", prefs_v3::irc_latency());
				config.update("telnetaddrs", prefs_v3::telnet_addrs());
				config.update("war3_ladder_update_secs", prefs_v3::war3_ladder_update_secs());
				config.update("XML_output_ladder", prefs_v3::XML_output_ladder());
				config.update("output_update_secs", prefs_v3::output_update_secs());
				config.update("XML_status_output", prefs_v3::XML_status_output());
				config.update("clan_newer_time", prefs_v3::clan_newer_time());
				config.update("clan_max_members", prefs_v3::clan_max_members());
				config.update("clan_channel_default_private", prefs_v3::clan_channel_default_private());
				config.update("clan_min_invites", prefs_v3::clan_min_invites());
				config.update("log_commands", prefs_v3::log_commands());
				config.update("log_command_groups", prefs_v3::log_command_groups());
				config.update("log_command_list", prefs_v3::log_command_list());

			}

		}





		/* Lua Events (called from scripts) */
#ifndef _LUA_EVENTS_

		extern int lua_handle_command(t_connection * c, char const * text, t_luaevent_type luaevent)
		{
			t_account * account;
			const char * func_name;
			int result = -2;
			switch (luaevent)
			{
			case luaevent_command:
				func_name = "handle_command";
				break;
			case luaevent_command_before:
				func_name = "handle_command_before";
				break;
			default:
				return result;
			}
			try
			{
				if (!(account = conn_get_account(c)))
					return -2;

				std::map<std::string, std::string> o_account = get_account_object(account);
				lua::transaction(vm) << lua::lookup(func_name) << o_account << text << lua::invoke >> result << lua::end; // invoke lua function

			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return result;
		}

		extern void lua_handle_game(t_game * game, t_connection * c, t_luaevent_type luaevent)
		{
			t_account * account;
			const char * func_name;
			switch (luaevent)
			{
			case luaevent_game_create:
				func_name = "handle_game_create";
				break;
			case luaevent_game_report:
				func_name = "handle_game_report";
				break;
			case luaevent_game_end:
				func_name = "handle_game_end";
				break;
			case luaevent_game_destroy:
				func_name = "handle_game_destroy";
				break;
			case luaevent_game_changestatus:
				func_name = "handle_game_changestatus";
				break;
			case luaevent_game_userjoin:
				func_name = "handle_game_userjoin";
				break;
			case luaevent_game_userleft:
				func_name = "handle_game_userleft";
				break;
			default:
				return;
			}
			try
			{
				std::map<std::string, std::string> o_game = get_game_object(game);

				// handle_game_userjoin & handle_game_userleft
				if (c)
				{
					if (!(account = conn_get_account(c)))
						return;

					std::map<std::string, std::string> o_account = get_account_object(account);
					lua::transaction(vm) << lua::lookup(func_name) << o_game << o_account << lua::invoke << lua::end; // invoke lua function
				}
				// other functions
				else
				{
					lua::transaction(vm) << lua::lookup(func_name) << o_game << lua::invoke << lua::end; // invoke lua function
				}

			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
		}

		std::vector<t_game*> lua_handle_game_list(t_connection * c)
		{
			t_account * account;
			std::vector<std::string> columns, data;
			std::vector<t_game*> result;
			try
			{
				if (!(account = conn_get_account(c)))
					return result;

				std::map<std::string, std::string> o_account = get_account_object(account);
				lua::transaction(vm) << lua::lookup("handle_game_list") << o_account << lua::invoke >> columns >> data << lua::end; // invoke lua function
			
				// check consistency of data and columns
				if (columns.size() == 0 || columns.size() != data.size() || std::floor((float)(data.size() / columns.size())) != (data.size() / columns.size()))
					return result;

				// fill map result
				for (std::vector<std::string>::size_type i = 1; i < data.size(); i += columns.size())
				{
					// init empty game struct
					t_game * game = new t_game{};
					game->id = 0;
					game->name = NULL;

					// next columns 
					for (int j = 1; j < columns.size(); j++)
					{
						if (columns[j] == "id")
							game->id = atoi(data[i+j-1].c_str());
						else if (columns[j] == "name")
							game->name = lua_strdup(data[i + j - 1].c_str());
					}
					result.push_back(game);
				}
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return result;
		}


		extern int lua_handle_channel(t_channel * channel, t_connection * c, char const * message_text, t_message_type message_type, t_luaevent_type luaevent)
		{
			int result = 0;
			t_account * account;
			const char * func_name;
			switch (luaevent)
			{
			case luaevent_channel_message:
				func_name = "handle_channel_message";
				break;
			case luaevent_channel_userjoin:
				func_name = "handle_channel_userjoin";
				break;
			case luaevent_channel_userleft:
				func_name = "handle_channel_userleft";
				break;
			default:
				return 0;
			}
			try
			{
				if (!(account = conn_get_account(c)))
					return 0;

				std::map<std::string, std::string> o_account = get_account_object(account);
				std::map<std::string, std::string> o_channel = get_channel_object(channel);

				// handle_channel_userleft & handle_channel_message
				if (message_text)
					lua::transaction(vm) << lua::lookup(func_name) << o_channel << o_account << message_text << message_type << lua::invoke >> result << lua::end; // invoke lua function
				// other functions
				else
					lua::transaction(vm) << lua::lookup(func_name) << o_channel << o_account << lua::invoke << lua::end; // invoke lua function
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return result;
		}


		extern int lua_handle_user(t_connection * c, t_connection * c_dst, char const * message_text, t_luaevent_type luaevent)
		{
			t_account * account, *account_dst;
			const char * func_name;
			int result = 0;
			switch (luaevent)
			{
			case luaevent_user_whisper:
				func_name = "handle_user_whisper";
				break;
			case luaevent_user_login:
				func_name = "handle_user_login";
				break;
			case luaevent_user_disconnect:
				func_name = "handle_user_disconnect";
				break;
			default:
				return 0;
			}
			try
			{
				if (!(account = conn_get_account(c)))
					return 0;

				std::map<std::string, std::string> o_account = get_account_object(account);

				// handle_server_whisper
				if (c_dst && message_text)
				{
					if (!(account_dst = conn_get_account(c_dst)))
						return 0;
					std::map<std::string, std::string> o_account_dst = get_account_object(account_dst);

					lua::transaction(vm) << lua::lookup(func_name) << o_account << o_account_dst << message_text << lua::invoke >> result << lua::end; // invoke lua function
				}
				// other functions
				else
					lua::transaction(vm) << lua::lookup(func_name) << o_account << lua::invoke << lua::end; // invoke lua function
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return result;
		}

		extern const char * lua_handle_user_icon(t_connection * c, const char * iconinfo)
		{
			t_account * account;
			const char * result = NULL;
			try
			{
				if (!(account = conn_get_account(c)))
					return 0;
				std::map<std::string, std::string> o_account = get_account_object(account);

				lua::transaction(vm) << lua::lookup("handle_user_icon") << o_account << iconinfo << lua::invoke >> result << lua::end; // invoke lua function
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return result;
		}



		extern void lua_handle_server(t_luaevent_type luaevent)
		{
			const char * func_name;
			switch (luaevent)
			{
			case luaevent_server_start:
				func_name = "main"; // when all lua scripts are loaded
				break;
			case luaevent_server_mainloop:
				func_name = "handle_server_mainloop"; // one time per second
				break;
			case luaevent_server_rehash:
				func_name = "handle_server_rehash"; // when restart Lua VM
				break;
			default:
				return;
			}
			try
			{
				lua::transaction(vm) << lua::lookup(func_name) << lua::invoke << lua::end; // invoke lua function
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
		}

		extern void lua_handle_client_readmemory(t_connection * c, int request_id, std::vector<int> data)
		{
			t_account * account;
			try
			{
				if (!(account = conn_get_account(c)))
					return;

				std::map<std::string, std::string> o_account = get_account_object(account);

				lua::transaction(vm) << lua::lookup("handle_client_readmemory") << o_account << request_id << data << lua::invoke << lua::end; // invoke lua function
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
		}

		extern void lua_handle_client_extrawork(t_connection * c, int gametype, int length, const char * data)
		{
			t_account * account;
			try
			{
				if (!(account = conn_get_account(c)))
					return;

				std::map<std::string, std::string> o_account = get_account_object(account);

				lua::transaction(vm) << lua::lookup("handle_client_extrawork") << o_account << gametype << length << data << lua::invoke << lua::end; // invoke lua function
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
		}
#endif


	}
}
#endif
