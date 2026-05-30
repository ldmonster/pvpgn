// SPDX-License-Identifier: GPL-2.0-or-later
// bntrackd_net.cpp — Platform networking initialisation and PID helper.
//
// Included into bntrackd.cpp's anonymous namespace via #include.
// Not a standalone compilation unit.

#ifdef _WIN32
class WsaInit {
public:
    WsaInit() {
        WSADATA wsa{};
        ok_ = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    }
    ~WsaInit() { if (ok_) WSACleanup(); }
    [[nodiscard]] bool ok() const noexcept { return ok_; }
private:
    bool ok_ = false;
};
#endif

unsigned long current_pid()
{
#ifdef _WIN32
    return static_cast<unsigned long>(_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}
