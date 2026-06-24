// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/compat/process.hpp"

#include "infra/compat/platform.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <process.h>     // _getpid
#  include <winsock2.h>    // gethostname (declared here on Windows)
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <sys/utsname.h>
#  include <unistd.h>
#endif

namespace pvpgn::v3::infra::compat {

std::uint64_t current_process_id() noexcept {
#if defined(_WIN32) || defined(_WIN64)
    return static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::optional<std::string> host_name() {
#if defined(_WIN32) || defined(_WIN64)
    // gethostname requires WinSock to be initialised. We pull in ws2_32
    // but the runtime requires WSAStartup. Callers that need the host
    // name early (before any networking is up) get an empty result --
    // documented as best-effort.
    WSADATA wsa{};
    const int wsa_status = ::WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsa_status != 0) {
        return std::nullopt;
    }
    std::array<char, 256> buf{};
    const int rc = ::gethostname(buf.data(), static_cast<int>(buf.size() - 1));
    ::WSACleanup();
    if (rc != 0) {
        return std::nullopt;
    }
    buf.back() = '\0';
    return std::string{buf.data()};
#else
    std::array<char, 256> buf{};
    if (::gethostname(buf.data(), buf.size() - 1) != 0) {
        return std::nullopt;
    }
    buf.back() = '\0';
    return std::string{buf.data()};
#endif
}

std::optional<SystemInfo> system_info() {
#if defined(_WIN32) || defined(_WIN64)
    SystemInfo info;
    info.sysname = "Windows";

    // OSVERSIONINFOEX is deprecated on modern Windows but the values are
    // still informational. We deliberately avoid RtlGetVersion gymnastics
    // here -- this is treated as best-effort.
    OSVERSIONINFOEXW vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);
#  pragma warning(push)
#  pragma warning(disable : 4996)
    if (::GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&vi)) != 0) {
        info.release = std::to_string(vi.dwMajorVersion) + "." +
                       std::to_string(vi.dwMinorVersion);
        info.version = "build " + std::to_string(vi.dwBuildNumber);
    }
#  pragma warning(pop)

    SYSTEM_INFO si{};
    ::GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: info.machine = "x86_64"; break;
        case PROCESSOR_ARCHITECTURE_ARM:   info.machine = "arm";    break;
        case PROCESSOR_ARCHITECTURE_ARM64: info.machine = "aarch64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: info.machine = "x86";    break;
        default:                            info.machine = "unknown"; break;
    }

    if (auto h = host_name()) {
        info.nodename = std::move(*h);
    }
    return info;
#else
    ::utsname uts{};
    if (::uname(&uts) != 0) {
        return std::nullopt;
    }
    SystemInfo info;
    info.sysname  = uts.sysname;
    info.nodename = uts.nodename;
    info.release  = uts.release;
    info.version  = uts.version;
    info.machine  = uts.machine;
    return info;
#endif
}

}  // namespace pvpgn::v3::infra::compat
