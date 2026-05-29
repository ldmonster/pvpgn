# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_infra_webui_channel_json
    SOURCES channel_json_test.cpp
    DEPS
        infra_webui_json
        infra_inmemory)
