/*
 * Copyright (C) 2000,2001	Onlyer	(onlyer@263.net)
 * Copyright (C) 2005	        Olaf Freyer (aaron@cs.tu-berlin.de)
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

#include <cerrno>
#include <cstdio>
#include <cstring>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef WIN32
# include "win32/service.h"
#endif
#ifdef WIN32_GUI
# include "win32/winmain.h"
#endif
#ifdef WIN32
# include "win32/windump.h"
#endif

#include "compat/stdfileno.h"
#include "compat/pgetpid.h"
#include "common/eventlog.h"
#include "cmdline.h"
#include "prefs_v3_shim.h"
#include "version.h"
#include "handle_signal.h"
#include "dbserver.h"
#include "common/setup_after.h"

using namespace pvpgn::d2dbs;
using namespace pvpgn;

static std::FILE * eventlog_fp;

char serviceLongName[] = "d2dbs service";
char serviceName[] = "d2dbs";
char serviceDescription[] = "Diablo 2 DataBase Server";

int g_ServiceStatus = -1;

static int init(void);
static int cleanup(void);
static int config_init(int argc, char * * argv);
static int config_cleanup(void);
static int setup_daemon(void);
static char * write_to_pidfile(void);


#ifdef DO_DAEMONIZE
static int setup_daemon(void)
{
	int pid;

	if (chdir("/") < 0) {
		eventlog(eventlog_level_error, __FUNCTION__, "can not change working directory to root directory (chdir: {})", std::strerror(errno));
		return -1;
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	if (!cmdline_get_foreground()) {
		close(STDERR_FILENO);
	}

	switch ((pid = fork())) {
	case 0:
		break;
	case -1:
		eventlog(eventlog_level_error, __FUNCTION__, "error create child process (fork: {})", std::strerror(errno));
		return -1;
	default:
		return pid;
	}
	umask(0);
	setsid();
	return 0;
}
#endif

static char * write_to_pidfile(void)
{
	const char* _pf_src = pvpgn::d2dbs::prefs_v3::pidfile(); char* pidfile = new char[std::strlen(_pf_src)+1]; std::strcpy(pidfile, _pf_src);

	if (pidfile)
	{
		if (pidfile[0] == '\0') {
			delete[] pidfile; /* avoid warning */
			return NULL;
		}
#ifdef HAVE_GETPID
		std::FILE * fp;

		if (!(fp = std::fopen(pidfile, "w"))) {
			eventlog(eventlog_level_error, __FUNCTION__, "unable to open pid file \"{}\" for writing (std::fopen: {})", pidfile, std::strerror(errno));
			delete[] pidfile; /* avoid warning */
			return NULL;
		}
		else {
			std::fprintf(fp, "%u", (unsigned int)getpid());
			if (std::fclose(fp) < 0)
				eventlog(eventlog_level_error, __FUNCTION__, "could not close pid file \"{}\" after writing (std::fclose: {})", pidfile, std::strerror(errno));
		}

#else
		eventlog(eventlog_level_warn, __FUNCTION__, "no getpid() std::system call, disable pid file in d2dbs.conf");
		delete[] pidfile; /* avoid warning */
		return NULL;
#endif
	}

	return pidfile;
}

static int init(void)
{
	return 0;
}

static int cleanup(void)
{
	return 0;
}

static int config_init(int argc, char * * argv)
{
	char const * levels;
	char const * tok;

	if (cmdline_load(argc, argv) < 0) {
		return -1;
	}

#ifdef DO_DAEMONIZE
	int		 pid;

	if (!cmdline_get_foreground()) {
		if (!((pid = setup_daemon()) == 0)) {
			return pid;
		}
	}
#endif

#ifdef PVPGN_V3_D2DBS_INTEGRATION
	{
		/* R157: v3 TOML is the sole config source under the integration
		 * build. Derive the '.toml' path from the cmdline '.conf' path
		 * (replace extension). Parse failure is fatal. */
		std::string toml_path = cmdline_get_preffile();
		auto dot = toml_path.rfind('.');
		auto sep = toml_path.find_last_of("/\\");
		if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
			toml_path.replace(dot, std::string::npos, ".toml");
		else
			toml_path += ".toml";
		if (pvpgn_v3_d2dbs_prefs_load_toml(toml_path.c_str()) == 0) {
			eventlog(eventlog_level_info, __FUNCTION__, "v3 TOML config loaded from '{}'", toml_path);
		} else {
			eventlog(eventlog_level_fatal, __FUNCTION__, "could not parse v3 TOML config '{}' (exiting)", toml_path);
			return -1;
		}
	}
#else
	if (d2dbs_prefs_load(cmdline_get_preffile()) < 0) {
		eventlog(eventlog_level_error, __FUNCTION__, "error loading configuration file {}", cmdline_get_preffile());
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

#ifdef WIN32_GUI
	if (cmdline_get_gui()){
		eventlog_add_level(eventlog_get_levelname_str(eventlog_level_gui));
	}
#endif

#ifdef DO_DAEMONIZE
	if (cmdline_get_foreground()) {
		eventlog_set(stderr);
	}
	else
#endif
	{
		if (cmdline_get_logfile()) {
			if (eventlog_open(cmdline_get_logfile()) < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "error open eventlog file {}", cmdline_get_logfile());
				return -1;
			}
		}
		else {
			if (eventlog_open(pvpgn::d2dbs::prefs_v3::logfile()) < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "error open eventlog file {}", pvpgn::d2dbs::prefs_v3::logfile());
				return -1;
			}
		}
	}
	return 0;
}

static int config_cleanup(void)
{
#ifdef PVPGN_V3_D2DBS_INTEGRATION
	pvpgn_v3_d2dbs_prefs_unload();
#else
	d2dbs_prefs_unload();
#endif
	cmdline_unload();
	eventlog_close();
	if (eventlog_fp) std::fclose(eventlog_fp);
	return 0;
}


#ifdef WIN32_GUI
extern int app_main(int argc, char ** argv)
#else
extern int main(int argc, char ** argv)
#endif
{
	int pid;
	char * pidfile;

#ifdef WIN32
	// create a dump file whenever the gateway crashes
	SetUnhandledExceptionFilter(unhandled_handler);
#endif

	eventlog_set(stderr);
	pid = config_init(argc, argv);
	if (!(pid == 0)) {
		//		if (pid==1) pid=0;
		return pid;
	}
	pidfile = write_to_pidfile();
	eventlog(eventlog_level_info, __FUNCTION__, D2DBS_VERSION);
	if (init() < 0) {
		eventlog(eventlog_level_error, __FUNCTION__, "failed to init");
		return -1;
	}
	else {
		eventlog(eventlog_level_info, __FUNCTION__, "server initialized");
	}
#ifndef WIN32
	d2dbs_handle_signal_init();
#endif
	dbs_server_main();
	cleanup();
	if (pidfile) {
		if (std::remove(pidfile) < 0)
			eventlog(eventlog_level_error, __FUNCTION__, "could not remove pid file \"{}\" (std::remove: {})", pidfile, std::strerror(errno));
		delete[] pidfile; /* avoid warning */
	}
	config_cleanup();
	return 0;
}
