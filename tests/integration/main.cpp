// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Catch2 main entry point for the integration test binary.
/// Passes --reporter console --use-colour yes by default so CI logs are
/// readable without a terminal that supports ANSI codes.

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    Catch::Session session;

    // Default to coloured console output; individual runs can override via CLI.
    session.configData().defaultColourMode = Catch::ColourMode::ANSI;

    return session.run(argc, argv);
}
