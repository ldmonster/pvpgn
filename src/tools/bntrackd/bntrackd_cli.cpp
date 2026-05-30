// SPDX-License-Identifier: GPL-2.0-or-later
// bntrackd_cli.cpp — CLI option parsing for bntrackd.
//
// Included into bntrackd.cpp's anonymous namespace via #include.
// Not a standalone compilation unit.

bool parse_uint(std::string_view sv, unsigned int &out)
{
    unsigned int v = 0;
    const char *first = sv.data();
    const char *last  = sv.data() + sv.size();
    auto [p, ec] = std::from_chars(first, last, v);
    if (ec != std::errc{} || p != last) return false;
    out = v;
    return true;
}

bool parse_ushort(std::string_view sv, std::uint16_t &out)
{
    unsigned int v = 0;
    if (!parse_uint(sv, v)) return false;
    if (v > 0xFFFFu) return false;
    out = static_cast<std::uint16_t>(v);
    return true;
}

[[noreturn]] void usage(const char *progname)
{
    std::fprintf(stderr, "usage: %s [<options>]\n", progname);
    std::fprintf(stderr,
        "  -c COMMAND, --command=COMMAND  execute COMMAND update\n"
        "  -d, --debug                    turn on debug mode\n"
        "  -e SECS, --expire SECS         forget a list entry after SEC seconds\n"
#ifdef DO_DAEMONIZE
        "  -f, --foreground               don't daemonize\n"
#else
        "  -f, --foreground               don't daemonize (default)\n"
#endif
        "  -l FILE, --logfile=FILE        write event messages to FILE\n"
        "  -o FILE, --outfile=FILE        write server list to FILE\n");
    std::fprintf(stderr,
        "  -p PORT, --port=PORT           listen for announcments on UDP port PORT\n"
        "  -P FILE, --pidfile=FILE        write pid to FILE\n"
        "  -u SECS, --update SECS         write output file every SEC seconds\n"
        "  -x, --XML                      write output file in XML format\n"
        "  -h, --help, --usage            show this information and exit\n"
        "  -v, --version                  print version number and exit\n");
    std::exit(EXIT_FAILURE);
}

void getprefs(int argc, char *argv[])
{
    g_prefs = Prefs{};

    auto need_arg = [&](int &a) {
        if (a + 1 >= argc) {
            std::fprintf(stderr,
                "%s: option \"%s\" requires an argument\n",
                argv[0], argv[a]);
            usage(argv[0]);
        }
        ++a;
        return argv[a];
    };

    for (int a = 1; a < argc; ++a) {
        std::string_view arg = argv[a];

        if (arg.starts_with("--command=")) {
            if (g_prefs.process) {
                std::fprintf(stderr,
                    "%s: processing command was already specified as \"%s\"\n",
                    argv[0], g_prefs.process);
                usage(argv[0]);
            }
            g_prefs.process = argv[a] + 10;
        }
        else if (arg == "-c") {
            const char *val = need_arg(a);
            if (g_prefs.process) {
                std::fprintf(stderr,
                    "%s: processing command was already specified as \"%s\"\n",
                    argv[0], g_prefs.process);
                usage(argv[0]);
            }
            g_prefs.process = val;
        }
        else if (arg == "-d" || arg == "--debug") {
            g_prefs.debug = 1;
        }
        else if (arg.starts_with("--expire=")) {
            if (g_prefs.expire) {
                std::fprintf(stderr,
                    "%s: expiration period was already specified as \"%u\"\n",
                    argv[0], g_prefs.expire);
                usage(argv[0]);
            }
            if (!parse_uint(arg.substr(9), g_prefs.expire)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], argv[a] + 9);
                usage(argv[0]);
            }
        }
        else if (arg == "-e") {
            const char *val = need_arg(a);
            if (g_prefs.expire) {
                std::fprintf(stderr,
                    "%s: expiration period was already specified as \"%u\"\n",
                    argv[0], g_prefs.expire);
                usage(argv[0]);
            }
            if (!parse_uint(val, g_prefs.expire)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], val);
                usage(argv[0]);
            }
        }
        else if (arg == "-f" || arg == "--foreground") {
            g_prefs.foreground = 1;
        }
        else if (arg == "-x" || arg == "--XML") {
            g_prefs.xml_mode = 1;
        }
        else if (arg.starts_with("--logfile=")) {
            if (g_prefs.logfile) {
                std::fprintf(stderr,
                    "%s: eventlog file was already specified as \"%s\"\n",
                    argv[0], g_prefs.logfile);
                usage(argv[0]);
            }
            g_prefs.logfile = argv[a] + 10;
        }
        else if (arg == "-l") {
            const char *val = need_arg(a);
            if (g_prefs.logfile) {
                std::fprintf(stderr,
                    "%s: eventlog file was already specified as \"%s\"\n",
                    argv[0], g_prefs.logfile);
                usage(argv[0]);
            }
            g_prefs.logfile = val;
        }
        else if (arg.starts_with("--outfile=")) {
            if (g_prefs.outfile) {
                std::fprintf(stderr,
                    "%s: output file was already specified as \"%s\"\n",
                    argv[0], g_prefs.outfile);
                usage(argv[0]);
            }
            g_prefs.outfile = argv[a] + 10;
        }
        else if (arg == "-o") {
            const char *val = need_arg(a);
            if (g_prefs.outfile) {
                std::fprintf(stderr,
                    "%s: output file was already specified as \"%s\"\n",
                    argv[0], g_prefs.outfile);
                usage(argv[0]);
            }
            g_prefs.outfile = val;
        }
        else if (arg.starts_with("--pidfile=")) {
            if (g_prefs.pidfile) {
                std::fprintf(stderr,
                    "%s: pid file was already specified as \"%s\"\n",
                    argv[0], g_prefs.pidfile);
                usage(argv[0]);
            }
            g_prefs.pidfile = argv[a] + 10;
        }
        else if (arg == "-P") {
            const char *val = need_arg(a);
            if (g_prefs.pidfile) {
                std::fprintf(stderr,
                    "%s: pid file was already specified as \"%s\"\n",
                    argv[0], g_prefs.pidfile);
                usage(argv[0]);
            }
            g_prefs.pidfile = val;
        }
        else if (arg.starts_with("--port=")) {
            if (g_prefs.port) {
                std::fprintf(stderr,
                    "%s: port number was already specified as \"%hu\"\n",
                    argv[0], g_prefs.port);
                usage(argv[0]);
            }
            if (!parse_ushort(arg.substr(7), g_prefs.port)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], argv[a] + 7);
                usage(argv[0]);
            }
        }
        else if (arg == "-p") {
            const char *val = need_arg(a);
            if (g_prefs.port) {
                std::fprintf(stderr,
                    "%s: port number was already specified as \"%hu\"\n",
                    argv[0], g_prefs.port);
                usage(argv[0]);
            }
            if (!parse_ushort(val, g_prefs.port)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], val);
                usage(argv[0]);
            }
        }
        else if (arg.starts_with("--update=")) {
            if (g_prefs.update) {
                std::fprintf(stderr,
                    "%s: update period was already specified as \"%u\"\n",
                    argv[0], g_prefs.update);
                usage(argv[0]);
            }
            if (!parse_uint(arg.substr(9), g_prefs.update)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], argv[a] + 9);
                usage(argv[0]);
            }
        }
        else if (arg == "-u") {
            const char *val = need_arg(a);
            if (g_prefs.update) {
                std::fprintf(stderr,
                    "%s: update period was already specified as \"%u\"\n",
                    argv[0], g_prefs.update);
                usage(argv[0]);
            }
            if (!parse_uint(val, g_prefs.update)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], val);
                usage(argv[0]);
            }
        }
        else if (arg == "-h" || arg == "--help" || arg == "--usage") {
            usage(argv[0]);
        }
        else if (arg == "-v" || arg == "--version") {
            std::printf("bntrackd version " PVPGN_VERSION "\n");
            std::exit(EXIT_SUCCESS);
        }
        else {
            std::fprintf(stderr, "%s: unrecognized option \"%s\"\n",
                argv[0], argv[a]);
            usage(argv[0]);
        }
    }

    if (!g_prefs.process) g_prefs.process = "";
    if (g_prefs.update == 0) g_prefs.update = kDefaultUpdate;
    if (g_prefs.expire == 0) g_prefs.expire = kDefaultExpire;
    if (!g_prefs.logfile)    g_prefs.logfile = kDefaultLogfile;
    if (!g_prefs.outfile)    g_prefs.outfile = kDefaultOutfile;
    if (g_prefs.port == 0)
        g_prefs.port = static_cast<std::uint16_t>(kDefaultPort);
    if (!g_prefs.pidfile)    g_prefs.pidfile = kDefaultPidfile;

    if (g_prefs.logfile && g_prefs.logfile[0] == '\0')
        g_prefs.logfile = nullptr;
    if (g_prefs.pidfile && g_prefs.pidfile[0] == '\0')
        g_prefs.pidfile = nullptr;
}
