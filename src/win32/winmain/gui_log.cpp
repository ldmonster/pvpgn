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

// gui_log.cpp — Rich-edit log window helpers, fprintf override, and fatal
//               error dialog (guiDEAD).

#ifdef WIN32_GUI

#pragma warning(disable : 4047)

#include "common/setup_before.h"
#include "../winmain.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include <windows.h>
#include <richedit.h>

#include "common/eventlog.h"
#include "common/gui_printf.h"

#include "common/setup_after.h"

#include "gui_state.hpp"

namespace pvpgn
{

    namespace bnetd
    {

        // ----------------------------------------------------------------
        // fprintf override — redirects stdout/stderr to the GUI log window
        //                    via gui_lvprintf; all other streams use the
        //                    standard vfprintf.
        // ----------------------------------------------------------------
        int fprintf(FILE *stream, const char *format, ...)
        {
            va_list args;
            va_start(args, format);
            if (stream == stderr || stream == stdout)
            {
                char buf[1024] = {};
                std::vsnprintf(buf, sizeof buf, format, args);
                gui_lvprintf(eventlog_level_error, "{}", buf);
                return 1;
            }
            else
                return vfprintf(stream, format, args);
        }

        // ----------------------------------------------------------------
        // guiAddText — appends coloured text to the rich-edit console.
        //              Trims the buffer to 30 000 chars when it overflows.
        // ----------------------------------------------------------------
        static void guiAddText(const char *str, COLORREF clr)
        {
            int text_length = SendMessageW(ghwndConsole, WM_GETTEXTLENGTH, 0, 0);

            if (text_length > 30000)
            {
                CHARRANGE ds = {};
                ds.cpMin = 0;
                ds.cpMax = text_length - 30000;
                SendMessageW(ghwndConsole, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&ds));
                SendMessageW(ghwndConsole, EM_REPLACESEL, FALSE, 0);
            }

            CHARRANGE cr = {};
            cr.cpMin = text_length;
            cr.cpMax = text_length;
            SendMessageW(ghwndConsole, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));

            CHARFORMATW fmt = {};
            fmt.cbSize   = sizeof(CHARFORMATW);
            fmt.dwMask   = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT | CFM_UNDERLINE;
            fmt.yHeight  = 160;
            fmt.dwEffects = 0;
            fmt.crTextColor = clr;
            std::swprintf(fmt.szFaceName, sizeof fmt.szFaceName / sizeof *fmt.szFaceName,
                          L"%ls", L"Courier New");

            SendMessageW(ghwndConsole, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&fmt));
            SendMessageA(ghwndConsole, EM_REPLACESEL,    FALSE,          reinterpret_cast<LPARAM>(str));
        }

        // ----------------------------------------------------------------
        // guiDEAD — shows a fatal-error message box (with GetLastError
        //           text) and terminates the process.
        // ----------------------------------------------------------------
        static void guiDEAD(const std::wstring& message)
        {
            wchar_t* error_message = nullptr;

            FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                nullptr,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPWSTR>(&error_message),
                0,
                nullptr);

            wchar_t* nl = std::wcschr(error_message, '\r');
            if (nl)
                *nl = '0';

            MessageBoxW(0,
                std::wstring(message + L"\nGetLastError() = " + error_message).c_str(),
                L"guiDEAD",
                MB_ICONSTOP | MB_OK);

            LocalFree(error_message);
            std::exit(1);
        }

        // ----------------------------------------------------------------
        // guiClearLogWindow — erases all text from the rich-edit console.
        // ----------------------------------------------------------------
        static void guiClearLogWindow()
        {
            SendMessageW(ghwndConsole, WM_SETTEXT, 0, 0);
        }

    } // namespace bnetd

} // namespace pvpgn

#endif // WIN32_GUI
