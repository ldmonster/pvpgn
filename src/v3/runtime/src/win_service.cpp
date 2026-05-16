// SPDX-License-Identifier: GPL-2.0-or-later
/// Windows Service Control Manager integration
/// Handles service installation, start, stop, and status

#ifdef _WIN32

#include "runtime/service_host.hpp"
#include <windows.h>
#include <winsvc.h>
#include <iostream>
#include <sstream>

namespace pvpgn::runtime {

/// Windows Service wrapper
class WindowsService {
public:
    /// Install service in SCM
    static Result<void, std::string> install(const std::string& service_name, 
                                             const std::string& display_name,
                                             const std::string& exe_path)
    {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
        if (!scm) {
            return Result<void, std::string>(
                core::fail(std::string("Failed to open SCM: ") + get_error_message())
            );
        }
        
        SC_HANDLE service = CreateServiceA(
            scm,
            service_name.c_str(),
            display_name.c_str(),
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            exe_path.c_str(),
            nullptr,  // load order group
            nullptr,  // tag id
            nullptr,  // dependencies
            nullptr,  // service start name (LocalSystem)
            nullptr   // password
        );
        
        if (!service) {
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to create service: ") + get_error_message())
            );
        }
        
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return Result<void, std::string>();  // Success
    }
    
    /// Uninstall service from SCM
    static Result<void, std::string> uninstall(const std::string& service_name)
    {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) {
            return Result<void, std::string>(
                core::fail(std::string("Failed to open SCM: ") + get_error_message())
            );
        }
        
        SC_HANDLE service = OpenServiceA(scm, service_name.c_str(), DELETE);
        if (!service) {
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to open service: ") + get_error_message())
            );
        }
        
        if (!DeleteService(service)) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to delete service: ") + get_error_message())
            );
        }
        
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return Result<void, std::string>();  // Success
    }
    
    /// Start service
    static Result<void, std::string> start(const std::string& service_name)
    {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) {
            return Result<void, std::string>(
                core::fail(std::string("Failed to open SCM: ") + get_error_message())
            );
        }
        
        SC_HANDLE service = OpenServiceA(scm, service_name.c_str(), SERVICE_START);
        if (!service) {
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to open service: ") + get_error_message())
            );
        }
        
        if (!StartServiceA(service, 0, nullptr)) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to start service: ") + get_error_message())
            );
        }
        
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return Result<void, std::string>();  // Success
    }
    
    /// Stop service
    static Result<void, std::string> stop(const std::string& service_name)
    {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) {
            return Result<void, std::string>(
                core::fail(std::string("Failed to open SCM: ") + get_error_message())
            );
        }
        
        SC_HANDLE service = OpenServiceA(scm, service_name.c_str(), SERVICE_STOP);
        if (!service) {
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to open service: ") + get_error_message())
            );
        }
        
        SERVICE_STATUS status;
        if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return Result<void, std::string>(
                core::fail(std::string("Failed to stop service: ") + get_error_message())
            );
        }
        
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return Result<void, std::string>();  // Success
    }
    
    /// Get service status
    static Result<std::string, std::string> status(const std::string& service_name)
    {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) {
            return Result<std::string, std::string>(
                core::fail(std::string("Failed to open SCM: ") + get_error_message())
            );
        }
        
        SC_HANDLE service = OpenServiceA(scm, service_name.c_str(), SERVICE_QUERY_STATUS);
        if (!service) {
            CloseServiceHandle(scm);
            return Result<std::string, std::string>(
                core::fail(std::string("Failed to open service: ") + get_error_message())
            );
        }
        
        SERVICE_STATUS status;
        if (!QueryServiceStatus(service, &status)) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return Result<std::string, std::string>(
                core::fail(std::string("Failed to query service status: ") + get_error_message())
            );
        }
        
        std::string status_str;
        switch (status.dwCurrentState) {
            case SERVICE_STOPPED:
                status_str = "STOPPED";
                break;
            case SERVICE_START_PENDING:
                status_str = "START_PENDING";
                break;
            case SERVICE_RUNNING:
                status_str = "RUNNING";
                break;
            case SERVICE_PAUSE_PENDING:
                status_str = "PAUSE_PENDING";
                break;
            case SERVICE_PAUSED:
                status_str = "PAUSED";
                break;
            case SERVICE_CONTINUE_PENDING:
                status_str = "CONTINUE_PENDING";
                break;
            case SERVICE_STOP_PENDING:
                status_str = "STOP_PENDING";
                break;
            default:
                status_str = "UNKNOWN";
                break;
        }
        
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return Result<std::string, std::string>(status_str);
    }
    
private:
    static std::string get_error_message()
    {
        DWORD error = GetLastError();
        LPSTR message = nullptr;
        
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&message,
            0,
            nullptr
        );
        
        std::string result = message ? std::string(message) : "Unknown error";
        if (message) {
            LocalFree(message);
        }
        
        return result;
    }
};

} // namespace pvpgn::runtime

#endif // _WIN32
