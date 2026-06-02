// SPDX-License-Identifier: GPL-2.0-or-later
// bntrackd_server.cpp — Main UDP receive loop and server-registry management.
//
// Included into bntrackd.cpp's anonymous namespace via #include.
// Not a standalone compilation unit.

int server_process(socket_t sockfd)
{
    std::time_t last = std::time(nullptr) - g_prefs.update;

    for (;;) {
        const std::time_t now = std::time(nullptr);

        if (last + static_cast<std::time_t>(g_prefs.update) < now) {
            last = now;
            std::FILE *outfile = std::fopen(g_prefs.outfile, "w");
            if (!outfile) {
                LOG_ERROR("bntrackd",
                    "unable to open file \"{}\" for writing (fopen: {})",
                    g_prefs.outfile, std::strerror(errno));
            }
            else {
                write_outfile(outfile, last);
                if (std::fclose(outfile) < 0) {
                    LOG_ERROR("bntrackd",
                        "could not close output file \"{}\" after writing"
                        " (fclose: {})",
                        g_prefs.outfile, std::strerror(errno));
                }
                if (g_prefs.process && g_prefs.process[0] != '\0') {
                    [[maybe_unused]] int ret = std::system(g_prefs.process);
                }
            }
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        timeval tv{};
        tv.tv_sec  = kGranularitySeconds;
        tv.tv_usec = 0;

        const int s = select(static_cast<int>(sockfd) + 1, &rfds, nullptr,
            nullptr, &tv);
        if (s < 0) {
#ifdef EINTR
            if (errno == EINTR) continue;
#endif
            LOG_ERROR("bntrackd", "select failed (select: {})",
                std::strerror(errno));
            continue;
        }
        if (s == 0) continue;
        if (!FD_ISSET(static_cast<std::size_t>(sockfd), &rfds)) continue;

        unsigned char buf[1024]{};
        sockaddr_in cliaddr{};
        socklen_portable clilen = sizeof(cliaddr);
        const auto n = recvfrom(sockfd,
            reinterpret_cast<char*>(buf), sizeof(buf), 0,
            reinterpret_cast<sockaddr*>(&cliaddr), &clilen);
        if (n < 0) continue;

        TrackPacket packet{};
        if (!decode_packet(buf, static_cast<std::size_t>(n), packet)) continue;
        if (packet.packet_version < kTrackVersion) continue;

        fixup_str(packet.software.data(),        packet.software.size());
        fixup_str(packet.version.data(),         packet.version.size());
        fixup_str(packet.platform.data(),        packet.platform.size());
        fixup_str(packet.server_desc.data(),     packet.server_desc.size());
        fixup_str(packet.server_location.data(), packet.server_location.size());
        fixup_str(packet.server_url.data(),      packet.server_url.size());
        fixup_str(packet.contact_name.data(),    packet.contact_name.size());
        fixup_str(packet.contact_email.data(),   packet.contact_email.size());

        auto it = std::find_if(g_servers.begin(), g_servers.end(),
            [&](const ServerEntry &e) {
                return std::memcmp(&e.address, &cliaddr.sin_addr,
                    sizeof(in_addr)) == 0;
            });

        if (it != g_servers.end()) {
            if (packet.flags & kFlagShutdown) {
                g_servers.erase(it);
            }
            else {
                it->info    = packet;
                it->updated = std::time(nullptr);
            }
        }
        else if (!(packet.flags & kFlagShutdown)) {
            ServerEntry entry{};
            entry.address = cliaddr.sin_addr;
            entry.info    = packet;
            entry.updated = std::time(nullptr);
            g_servers.push_back(entry);
        }

        char addrstr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &cliaddr.sin_addr, addrstr, sizeof(addrstr));

        LOG_DEBUG("bntrackd",
            "Packet received from {}:"
            " packet_version={}"
            " flags=0x{:08x}"
            " port={}"
            " software=\"{}\""
            " version=\"{}\""
            " platform=\"{}\""
            " server_desc=\"{}\""
            " server_location=\"{}\""
            " server_url=\"{}\""
            " contact_name=\"{}\""
            " contact_email=\"{}\""
            " uptime={}"
            " total_games={}"
            " total_logins={}",
            addrstr,
            packet.packet_version,
            packet.flags,
            packet.port,
            packet.software.data(),
            packet.version.data(),
            packet.platform.data(),
            packet.server_desc.data(),
            packet.server_location.data(),
            packet.server_url.data(),
            packet.contact_name.data(),
            packet.contact_email.data(),
            packet.uptime,
            packet.total_games,
            packet.total_logins);
    }
}
