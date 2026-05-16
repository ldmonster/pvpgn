// SPDX-License-Identifier: GPL-2.0-or-later
/// Crash handler and stack trace utilities
/// Provides backtrace generation for debugging crashes

#include "runtime/service_host.hpp"

#include <iostream>
#include <sstream>
#include <csignal>
#include <cstring>

#ifndef _WIN32
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#else
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace pvpgn::runtime {

/// Crash handler for generating stack traces
class CrashHandler {
public:
    /// Install signal handlers for crash reporting
    static void install()
    {
#ifndef _WIN32
        std::signal(SIGSEGV, signal_handler);
        std::signal(SIGABRT, signal_handler);
        std::signal(SIGBUS, signal_handler);
        std::signal(SIGFPE, signal_handler);
        std::signal(SIGILL, signal_handler);
#else
        // Windows exception handling would go here
        // For now, we rely on Windows error reporting
#endif
    }
    
    /// Generate and print stack trace
    static void print_stack_trace()
    {
#ifndef _WIN32
        print_stack_trace_unix();
#else
        print_stack_trace_windows();
#endif
    }
    
private:
    static void signal_handler(int sig)
    {
        std::cerr << "\n=== CRASH DETECTED ===\n";
        std::cerr << "Signal: " << sig << " (" << strsignal(sig) << ")\n";
        std::cerr << "\nStack trace:\n";
        print_stack_trace();
        std::cerr << "=== END CRASH REPORT ===\n";
        
        // Re-raise the signal to generate core dump
        std::signal(sig, SIG_DFL);
        raise(sig);
    }
    
#ifndef _WIN32
    static void print_stack_trace_unix()
    {
        const int max_frames = 64;
        void* addrlist[max_frames];
        
        int addrlen = backtrace(addrlist, max_frames);
        if (addrlen == 0) {
            std::cerr << "  <empty stack trace>\n";
            return;
        }
        
        char** symbollist = backtrace_symbols(addrlist, addrlen);
        
        for (int i = 0; i < addrlen; ++i) {
            char* begin_name = nullptr;
            char* begin_offset = nullptr;
            char* end_offset = nullptr;
            
            // Find the parentheses and offset address
            for (char* p = symbollist[i]; *p; ++p) {
                if (*p == '(')
                    begin_name = p;
                else if (*p == '+')
                    begin_offset = p;
                else if (*p == ')' && begin_offset) {
                    end_offset = p;
                    break;
                }
            }
            
            if (begin_name && begin_offset && end_offset &&
                begin_name < begin_offset) {
                *begin_name++ = '\0';
                *begin_offset++ = '\0';
                *end_offset = '\0';
                
                int status;
                char* demangled_name = abi::__cxa_demangle(begin_name, nullptr, nullptr, &status);
                
                if (status == 0) {
                    std::cerr << "  [" << i << "] " << symbollist[i] << " : "
                              << demangled_name << "+" << begin_offset << "\n";
                    free(demangled_name);
                } else {
                    std::cerr << "  [" << i << "] " << symbollist[i] << " : "
                              << begin_name << "+" << begin_offset << "\n";
                }
            } else {
                // Couldn't parse the line? Print the whole thing.
                std::cerr << "  [" << i << "] " << symbollist[i] << "\n";
            }
        }
        
        free(symbollist);
    }
#else
    static void print_stack_trace_windows()
    {
        const int max_frames = 64;
        PVOID addrlist[max_frames];
        HANDLE process = GetCurrentProcess();
        
        SymInitialize(process, nullptr, TRUE);
        
        USHORT frames = CaptureStackBackTrace(0, max_frames, addrlist, nullptr);
        
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        
        for (USHORT i = 0; i < frames; ++i) {
            SymFromAddr(process, (DWORD64)addrlist[i], nullptr, symbol);
            std::cerr << "  [" << i << "] " << symbol->Name << " - 0x" 
                      << std::hex << symbol->Address << std::dec << "\n";
        }
        
        free(symbol);
        SymCleanup(process);
    }
#endif
};

} // namespace pvpgn::runtime
