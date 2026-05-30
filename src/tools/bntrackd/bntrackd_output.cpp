// SPDX-License-Identifier: GPL-2.0-or-later
// bntrackd_output.cpp — Server-list file writer (ASCII and XML formats).
//
// Included into bntrackd.cpp's anonymous namespace via #include.
// Not a standalone compilation unit.

void write_outfile(std::FILE *outfile, std::time_t now)
{
    std::erase_if(g_servers, [&](const ServerEntry &e) {
        return e.updated +
               static_cast<std::time_t>(g_prefs.expire) < now;
    });

    for (const auto &server : g_servers) {
        char addrstr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &server.address, addrstr, sizeof(addrstr));

        const TrackPacket &p = server.info;
        if (g_prefs.xml_mode == 1) {
            std::fprintf(outfile, "<server>\n\t<address>%s</address>\n", addrstr);
            std::fprintf(outfile, "\t<port>%" PRIu16 "</port>\n",                p.port);
            std::fprintf(outfile, "\t<location>%s</location>\n",                 p.server_location.data());
            std::fprintf(outfile, "\t<software>%s</software>\n",                 p.software.data());
            std::fprintf(outfile, "\t<version>%s</version>\n",                   p.version.data());
            std::fprintf(outfile, "\t<users>%" PRIu32 "</users>\n",              p.users);
            std::fprintf(outfile, "\t<channels>%" PRIu32 "</channels>\n",        p.channels);
            std::fprintf(outfile, "\t<games>%" PRIu32 "</games>\n",              p.games);
            std::fprintf(outfile, "\t<description>%s</description>\n",           p.server_desc.data());
            std::fprintf(outfile, "\t<platform>%s</platform>\n",                 p.platform.data());
            std::fprintf(outfile, "\t<url>%s</url>\n",                           p.server_url.data());
            std::fprintf(outfile, "\t<contact_name>%s</contact_name>\n",         p.contact_name.data());
            std::fprintf(outfile, "\t<contact_email>%s</contact_email>\n",       p.contact_email.data());
            std::fprintf(outfile, "\t<uptime>%" PRIu32 "</uptime>\n",            p.uptime);
            std::fprintf(outfile, "\t<total_games>%" PRIu32 "</total_games>\n",  p.total_games);
            std::fprintf(outfile, "\t<logins>%" PRIu32 "</logins>\n",            p.total_logins);
            std::fprintf(outfile, "</server>\n");
        }
        else {
            std::fprintf(outfile, "%s\n##\n", addrstr);
            std::fprintf(outfile, "%" PRIu16 "\n##\n", p.port);
            std::fprintf(outfile, "%s\n##\n",          p.server_location.data());
            std::fprintf(outfile, "%s\n##\n",          p.software.data());
            std::fprintf(outfile, "%s\n##\n",          p.version.data());
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.users);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.channels);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.games);
            std::fprintf(outfile, "%s\n##\n",          p.server_desc.data());
            std::fprintf(outfile, "%s\n##\n",          p.platform.data());
            std::fprintf(outfile, "%s\n##\n",          p.server_url.data());
            std::fprintf(outfile, "%s\n##\n",          p.contact_name.data());
            std::fprintf(outfile, "%s\n##\n",          p.contact_email.data());
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.uptime);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.total_games);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.total_logins);
            std::fprintf(outfile, "###\n");
        }
    }
}
