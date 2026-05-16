# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_infra_plugin_capability
    SOURCES capability_test.cpp
    DEPS    core infra_scripting_plugin)

pvpgn_v3_add_test(test_infra_plugin_manifest
    SOURCES plugin_manifest_test.cpp
    DEPS    core infra_scripting_plugin)

pvpgn_v3_add_test(test_infra_plugin_loader
    SOURCES plugin_loader_test.cpp
    DEPS    core infra_scripting_plugin)
