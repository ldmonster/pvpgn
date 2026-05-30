/*
 * Copyright (C) 2001  Erik Latoshek [forester] (laterk@inbox.lv)
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

// winmain.cpp — Thin coordinator.
//
// Pulls in all Win32 GUI sub-translation-units and provides the WinMain()
// entry point that bootstraps the GUI thread and then calls app_main().
//
// Sub-TU responsibilities:
//   winmain/gui_log.cpp      — rich-edit log helpers, fprintf override, guiDEAD
//   winmain/gui_window.cpp   — window class, message loop, painting, sizing,
//                              mouse drag, shared state definitions
//   winmain/gui_tray.cpp     — system-tray icon, shell-notify handler
//   winmain/gui_commands.cpp — menu command dispatcher, user-list refresh
//   winmain/gui_dialogs.cpp  — About / Announce / Kick dialog procedures

#ifdef WIN32_GUI

// Sub-TUs are included directly so that all static functions within
// namespace pvpgn::bnetd remain visible to each other (they reference
// each other via the forward declarations in gui_state.hpp).
#include "winmain/gui_log.cpp"
#include "winmain/gui_window.cpp"
#include "winmain/gui_tray.cpp"
#include "winmain/gui_commands.cpp"
#include "winmain/gui_dialogs.cpp"

#include "common/setup_before.h"
#include "winmain.h"

#include <windows.h>
#include <process.h>

#include "bnetd/cmdline.h"
#include "console_output.h"

#include "common/setup_after.h"

extern int app_main(int argc, char **argv); /* bnetd main function */

using namespace pvpgn;
using namespace pvpgn::bnetd;

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE reserved, LPSTR lpCmdLine, int nCmdShow)
{
    Console console;

    if (cmdline_load(__argc, __argv) != 1)
        return -1;

    if (cmdline_get_console())
    {
        console.RedirectIOToConsole();
        return app_main(__argc, __argv);
    }

    pvpgn::bnetd::gui.main_finished = FALSE;
    pvpgn::bnetd::gui.event_ready   = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    _beginthread(pvpgn::bnetd::guiThread, 0, (void*)hInstance);
    WaitForSingleObject(pvpgn::bnetd::gui.event_ready, INFINITE);

    auto result = app_main(__argc, __argv);

    pvpgn::bnetd::gui.main_finished = TRUE;
    eventlog(pvpgn::eventlog_level_debug, __FUNCTION__,
             "server exited ( return : {} )", result);
    WaitForSingleObject(pvpgn::bnetd::gui.event_ready, INFINITE);

    return 0;
}

#endif // WIN32_GUI
