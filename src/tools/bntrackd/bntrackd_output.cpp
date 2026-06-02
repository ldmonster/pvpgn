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
            std::println(outfile, "<server>\n\t<address>{}</address>", addrstr);
            std::println(outfile, "\t<port>{{}}</port>",                p.port);
            std::println(outfile, "\t<location>{}</location>",                 p.server_location.data());
            std::println(outfile, "\t<software>{}</software>",                 p.software.data());
            std::println(outfile, "\t<version>{}</version>",                   p.version.data());
            std::println(outfile, "\t<users>{{}}</users>",              p.users);
            std::println(outfile, "\t<channels>{{}}</channels>",        p.channels);
            std::println(outfile, "\t<games>{{}}</games>",              p.games);
            std::println(outfile, "\t<description>{}</description>",           p.server_desc.data());
            std::println(outfile, "\t<platform>{}</platform>",                 p.platform.data());
            std::println(outfile, "\t<url>{}</url>",                           p.server_url.data());
            std::println(outfile, "\t<contact_name>{}</contact_name>",         p.contact_name.data());
            std::println(outfile, "\t<contact_email>{}</contact_email>",       p.contact_email.data());
            std::println(outfile, "\t<uptime>{{}}</uptime>",            p.uptime);
            std::println(outfile, "\t<total_games>{{}}</total_games>",  p.total_games);
            std::println(outfile, "\t<logins>{{}}</logins>",            p.total_logins);
            std::println(outfile, "</server>");
        }
        else {
            std::println(outfile, "{}\n##", addrstr);
            std::println(outfile, "{{}}\n##", p.port);
            std::println(outfile, "{}\n##",          p.server_location.data());
            std::println(outfile, "{}\n##",          p.software.data());
            std::println(outfile, "{}\n##",          p.version.data());
            std::println(outfile, "{{}}\n##", p.users);
            std::println(outfile, "{{}}\n##", p.channels);
            std::println(outfile, "{{}}\n##", p.games);
            std::println(outfile, "{}\n##",          p.server_desc.data());
            std::println(outfile, "{}\n##",          p.platform.data());
            std::println(outfile, "{}\n##",          p.server_url.data());
            std::println(outfile, "{}\n##",          p.contact_name.data());
            std::println(outfile, "{}\n##",          p.contact_email.data());
            std::println(outfile, "{{}}\n##", p.uptime);
            std::println(outfile, "{{}}\n##", p.total_games);
            std::println(outfile, "{{}}\n##", p.total_logins);
            std::println(outfile, "###");
        }
    }
}
