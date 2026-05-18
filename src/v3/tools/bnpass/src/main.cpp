// SPDX-License-Identifier: GPL-2.0-or-later
//
// pvpgn v3 port of the legacy `src/bnpass/bnpass.cpp` and
// `src/bnpass/sha1hash.cpp` CLI tools (see
// plans/refactoring-plan-legacy-tools.md "Step 2").
//
// `pvpgn-bnpass` reads a clear-text password (from argv or stdin),
// lower-cases it the same way the legacy tool did, and prints the
// matching `passhash1` attribute line. With `--sha1` it instead
// emits a true-SHA-1 digest of the (un-lowercased) bytes -- the
// legacy `sha1hash` CLI is folded into the same binary so we keep
// just one entrypoint.
//
// All hashing routes through `pvpgn::v3::infra::crypto`, the C++20
// reimplementation of legacy `common/bnethash.{h,cpp}`. No legacy
// `common`/`compat` linkage required.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "infra/crypto/bnet_hash.hpp"

#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif

namespace {

enum class Mode {
    BnetPassHash1,  // legacy `bnpass`
    Sha1,           // legacy `sha1hash`
};

[[noreturn]] void usage(const char* progname, int code) {
    std::fprintf(
        stderr,
        "usage: %s [<options>] [--] [<cleartextpassword>]\n"
        "    -h, --help, --usage  show this information and exit\n"
        "    -v, --version        print version number and exit\n"
        "        --sha1           emit a true SHA-1 digest of the input bytes\n"
        "                         (replaces the legacy `sha1hash` tool)\n",
        progname);
    std::exit(code);
}

void to_lower_ascii(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 1 || !argv || !argv[0]) {
        std::fprintf(stderr, "bad arguments\n");
        return EXIT_FAILURE;
    }

    const char* pass = nullptr;
    bool force_pass = false;
    Mode mode = Mode::BnetPassHash1;

    for (int a = 1; a < argc; ++a) {
        std::string_view arg{argv[a]};
        if (force_pass && !pass) {
            pass = argv[a];
        } else if (arg == "-" && !pass) {
            pass = argv[a];
        } else if (!arg.empty() && arg.front() != '-' && !pass) {
            pass = argv[a];
        } else if (force_pass || arg.empty() || (arg.front() != '-')
                   || arg == "-") {
            std::fprintf(stderr, "%s: extra password argument \"%s\"\n",
                         argv[0], argv[a]);
            usage(argv[0], EXIT_FAILURE);
        } else if (arg == "--") {
            force_pass = true;
        } else if (arg == "-v" || arg == "--version") {
            std::printf("version %s\n", PVPGN_VERSION);
            return EXIT_SUCCESS;
        } else if (arg == "-h" || arg == "--help" || arg == "--usage") {
            usage(argv[0], EXIT_SUCCESS);
        } else if (arg == "--sha1") {
            mode = Mode::Sha1;
        } else {
            std::fprintf(stderr, "%s: unknown option \"%s\"\n",
                         argv[0], argv[a]);
            usage(argv[0], EXIT_FAILURE);
        }
    }

    std::string buff;
    if (!pass) {
        std::printf("Enter password to hash: ");
        std::fflush(stdout);
        if (!std::getline(std::cin, buff)) {
            buff.clear();
        }
    } else {
        buff = pass;
    }

    namespace pc = pvpgn::v3::infra::crypto;

    if (mode == Mode::BnetPassHash1) {
        // Legacy contract: lower-case the password before hashing.
        to_lower_ascii(buff);
        const pc::BnetDigest h = pc::blizzard_hash(std::string_view{buff});
        std::printf("\"BNET\\\\acct\\\\passhash1\"=\"%s\"\n",
                    pc::to_hex(h).c_str());
    } else {
        const pc::BnetDigest h = pc::sha1(std::string_view{buff});
        std::printf("sha1 hash = %s\n", pc::to_hex(h).c_str());
    }

    return EXIT_SUCCESS;
}
