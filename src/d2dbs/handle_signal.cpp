/*
 * Copyright (C) 2000,2001	Onlyer	(onlyer@263.net)
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
#include "common/setup_before.h"
#include "setup.h"
#include "handle_signal.h"

#include <cstring>
#include <ctime>
#include <string>

#include <csignal>

#include "common/eventlog.h"
#include "prefs_v3_shim.h"
#include "d2ladder.h"
#include "cmdline.h"
#include "common/setup_after.h"

#ifdef PVPGN_V3_D2DBS_INTEGRATION
// R232(3): observation bridges for d2dbs signal init + dispatch.
extern "C" int pvpgn_v3_d2dbs_handle_signal_init_try(void) noexcept;
extern "C" int pvpgn_v3_d2dbs_handle_signal_try(void) noexcept;
#endif

namespace pvpgn
{

	namespace d2dbs
	{

		static void on_signal(int s);

		static volatile struct
		{
			unsigned char	do_quit;
			unsigned char	cancel_quit;
			unsigned char	reload_config;
			unsigned char	save_ladder;
			time_t			exit_time;
		} signal_data = { 0, 0, 0, 0, 0 };

		extern int d2dbs_handle_signal(void)
		{
#ifdef PVPGN_V3_D2DBS_INTEGRATION
			(void)pvpgn_v3_d2dbs_handle_signal_try();
#endif
			std::time_t		now;
			char const * levels;
			char const * tok;


			if (signal_data.cancel_quit) {
				signal_data.cancel_quit = 0;
				if (!signal_data.exit_time) {
					eventlog(eventlog_level_info, __FUNCTION__, "there is no previous shutdown to be canceled");
				}
				else {
					signal_data.exit_time = 0;
					eventlog(eventlog_level_info, __FUNCTION__, "shutdown was canceled due to std::signal");
				}
			}
			if (signal_data.do_quit) {
				signal_data.do_quit = 0;
				now = std::time(NULL);
				if (!signal_data.exit_time) {
					signal_data.exit_time = now + pvpgn::d2dbs::prefs_v3::shutdown_delay();
				}
				else {
					signal_data.exit_time -= pvpgn::d2dbs::prefs_v3::shutdown_decr();
				}
				eventlog(eventlog_level_info, __FUNCTION__, "the server is going to shutdown in {} minutes", (signal_data.exit_time - now) / 60);
			}
			if (signal_data.exit_time) {
				now = std::time(NULL);
				if (now >= (signed)signal_data.exit_time) {
					signal_data.exit_time = 0;
					eventlog(eventlog_level_info, __FUNCTION__, "shutdown server due to std::signal");
					return -1;
				}
			}
			if (signal_data.reload_config) {
				signal_data.reload_config = 0;
				eventlog(eventlog_level_info, __FUNCTION__, "reloading configuartion file due to std::signal");
#ifdef PVPGN_V3_D2DBS_INTEGRATION
				{
					/* R157: reload v3 TOML snapshot instead of legacy parser. */
					std::string toml_path = cmdline_get_preffile();
					auto dot = toml_path.rfind('.');
					auto sep = toml_path.find_last_of("/\\");
					if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
						toml_path.replace(dot, std::string::npos, ".toml");
					else
						toml_path += ".toml";
					if (pvpgn_v3_d2dbs_prefs_load_toml(toml_path.c_str()) < 0) {
						eventlog(eventlog_level_error, __FUNCTION__, "error reload v3 TOML config '{}',exitting", toml_path);
						return -1;
					}
					eventlog(eventlog_level_info, __FUNCTION__, "v3 TOML config snapshot after reload:");
					pvpgn_v3_d2dbs_prefs_dump(nullptr, [](void*, const char* line) {
						eventlog(eventlog_level_info, "d2dbs_config", "  {}", line);
					});
				}
#else
				if (d2dbs_prefs_reload(cmdline_get_preffile()) < 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "error reload configuration file,exitting");
					return -1;
				}
#endif
				eventlog_clear_level();
				if ((levels = pvpgn::d2dbs::prefs_v3::loglevels()))
				{
					std::string temp(levels);
					tok = std::strtok(temp.empty() ? nullptr : &temp[0], ","); /* std::strtok modifies the string it is passed */

					while (tok)
					{
						if (eventlog_add_level(tok) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not add std::log level \"{}\"", tok);
						tok = std::strtok(NULL, ",");
					}
				}
#ifdef DO_DAEMONIZE
				if (!cmdline_get_foreground())
#endif
					eventlog_open(pvpgn::d2dbs::prefs_v3::logfile());
			}
			if (signal_data.save_ladder) {
				signal_data.save_ladder = 0;
				eventlog(eventlog_level_info, __FUNCTION__, "save ladder data due to std::signal");
				d2ladder_saveladder();
			}
			return 0;
		}

#ifdef WIN32
		extern void d2dbs_signal_quit_wrapper(void)
		{
			signal_data.do_quit = 1;
		}

		extern void d2dbs_signal_reload_config_wrapper(void)
		{
			signal_data.reload_config = 1;
		}

		extern void d2dbs_signal_save_ladder_wrapper(void)
		{
			signal_data.save_ladder = 1;
		}

		extern void d2dbs_signal_exit_wrapper(void)
		{
			signal_data.exit_time = 1;
		}
#else
		extern int d2dbs_handle_signal_init(void)
		{
#ifdef PVPGN_V3_D2DBS_INTEGRATION
			(void)pvpgn_v3_d2dbs_handle_signal_init_try();
#endif
			std::signal(SIGINT, on_signal);
			std::signal(SIGTERM, on_signal);
			std::signal(SIGABRT, on_signal);
			std::signal(SIGHUP, on_signal);
			std::signal(SIGUSR1, on_signal);
			std::signal(SIGPIPE, on_signal);
			return 0;
		}

		static void on_signal(int s)
		{
			switch (s) {
			case SIGINT:
				eventlog(eventlog_level_debug, __FUNCTION__, "sigint received");
				signal_data.do_quit = 1;
				break;
			case SIGTERM:
				eventlog(eventlog_level_debug, __FUNCTION__, "sigint received");
				signal_data.do_quit = 1;
				break;
			case SIGABRT:
				eventlog(eventlog_level_debug, __FUNCTION__, "sigabrt received");
				signal_data.cancel_quit = 1;
				break;
			case SIGHUP:
				eventlog(eventlog_level_debug, __FUNCTION__, "sighup received");
				signal_data.reload_config = 1;
				break;
			case SIGUSR1:
				eventlog(eventlog_level_debug, __FUNCTION__, "sigusr1 received");
				signal_data.save_ladder = 1;
				break;
			case SIGPIPE:
				eventlog(eventlog_level_debug, __FUNCTION__, "sigpipe received");
				break;
			}
			std::signal(s, on_signal);
		}
#endif

	}

}
