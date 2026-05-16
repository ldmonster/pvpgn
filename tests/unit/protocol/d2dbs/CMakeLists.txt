# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(protocol_d2dbs_fsm_test
    SOURCES fsm_test.cpp
    DEPS protocol_d2dbs
)

pvpgn_v3_add_test(protocol_d2dbs_codec_test
    SOURCES d2dbs_codec_test.cpp
    DEPS protocol_d2dbs
)
