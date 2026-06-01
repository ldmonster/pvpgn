#!/usr/bin/env python3
"""
Update CMakeLists.txt to remove deleted bridge files.

This script removes references to deleted thin wrapper bridge files
from the integration_legacy_bnetd_linked target in CMakeLists.txt.
"""

import re
from pathlib import Path

def update_cmakelists():
    """Remove deleted files from CMakeLists.txt."""
    
    # Files that were deleted
    deleted_files = {
        'src/account_dispatch_bridge.cpp',
        'src/ad_dispatch_bridge.cpp',
        'src/anongame_dispatch_bridge.cpp',
        'src/anongame_entry_bridge.cpp',
        'src/anongame_infos_bridge.cpp',
        'src/anongame_infos_get_bridge.cpp',
        'src/anongame_lobby_bridge.cpp',
        'src/auth_dispatch_bridge.cpp',
        'src/bot_dispatch_bridge.cpp',
        'src/cdkey_dispatch_bridge.cpp',
        'src/change_password_bridge.cpp',
        'src/channel_state_bridge.cpp',
        'src/clan_dispatch_bridge.cpp',
        'src/clan_send_bridge.cpp',
        'src/connection_dispatch_bridge.cpp',
        'src/d2_character_dispatch_bridge.cpp',
        'src/d2cs_link_dispatch_bridge.cpp',
        'src/file_dispatch_bridge.cpp',
        'src/friends_dispatch_bridge.cpp',
        'src/game_report_bridge.cpp',
        'src/gamelist_join_bridge.cpp',
        'src/gameport_dispatch_bridge.cpp',
        'src/handshake_dispatch_bridge.cpp',
        'src/irc_dispatch_bridge.cpp',
        'src/keepalive_dispatch_bridge.cpp',
        'src/ladder_dispatch_bridge.cpp',
        'src/login_user_bridge.cpp',
        'src/message_dispatch_bridge.cpp',
        'src/news_bridge.cpp',
        'src/passemail_dispatch_bridge.cpp',
        'src/profile_dispatch_bridge.cpp',
        'src/progident_dispatch_bridge.cpp',
        'src/realm_dispatch_bridge.cpp',
        'src/realm_list_bridge.cpp',
        'src/runprog_bridge.cpp',
        'src/send_anongame_cancel_bridge.cpp',
        'src/send_anongame_found_bridge.cpp',
        'src/send_anongame_search_reply_bridge.cpp',
        'src/send_file_bridge.cpp',
        'src/send_packet_bridge.cpp',
        'src/send_udptest_bridge.cpp',
        'src/server_dispatch_bridge.cpp',
        'src/startgame_bridge.cpp',
        'src/stub_dispatch_bridge.cpp',
        'src/telemetry_dispatch_bridge.cpp',
        'src/telnet_dispatch_bridge.cpp',
        'src/userlog_bridge.cpp',
        'src/versioncheck_bridge.cpp',
        'src/wol_dispatch_bridge.cpp',
    }
    
    cmake_path = Path('/home/cnupt/work/pvpgn/src/integration/legacy_bnetd/CMakeLists.txt')
    
    with open(cmake_path, 'r') as f:
        content = f.read()
    
    original_content = content
    
    # Remove each deleted file line
    for deleted_file in deleted_files:
        # Match the line with the file, including any trailing comment
        pattern = rf'\s*{re.escape(deleted_file)}.*\n'
        content = re.sub(pattern, '', content)
    
    # Clean up any double blank lines that might have been created
    content = re.sub(r'\n\n\n+', '\n\n', content)
    
    if content != original_content:
        with open(cmake_path, 'w') as f:
            f.write(content)
        print(f"Updated {cmake_path}")
        print(f"Removed {len(deleted_files)} file references")
    else:
        print("No changes needed")

if __name__ == '__main__':
    update_cmakelists()
