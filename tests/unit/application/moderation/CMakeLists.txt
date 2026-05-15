# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_application_moderation_ip_ban
    SOURCES check_ip_ban_test.cpp
    DEPS    application_moderation)
