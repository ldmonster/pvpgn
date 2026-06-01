#!/usr/bin/env python3
"""
Phase 2 Bridge Migration Automation

Migrates 276 complex logic bridges by:
1. Deleting observation-only bridges
2. Marking prefs_bridge.cpp as MIGRATED (already in v3)
3. Deleting send-bridges (already in v3 application use cases)
4. Deleting dispatch bridges (already in v3 handlers)
5. Updating CMakeLists.txt
6. Updating bridge-symbols.csv
"""

import csv
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

# Configuration
REPO_ROOT = Path(__file__).parent.parent.parent
LEGACY_BNETD_SRC = REPO_ROOT / "src/integration/legacy_bnetd/src"
LEGACY_BNETD_INCLUDE = REPO_ROOT / "src/integration/legacy_bnetd/include"
CSV_PATH = REPO_ROOT / "planstwo/inventory/bridge-symbols.csv"
CMAKELISTS_PATH = REPO_ROOT / "src/integration/legacy_bnetd/CMakeLists.txt"

# Files to delete by category
OBSERVATION_ONLY_FILES = {
    "bnetd_lifecycle_bridges.cpp",
    "ipban_bridge.cpp",
}

SEND_BRIDGE_FILES = {
    "send_adclick2reply_bridge.cpp",
    "send_adreply_bridge.cpp",
    "send_atacceptdecline_bridge.cpp",
    "send_atfriendscreen_bridge.cpp",
    "send_authinfo_reply_bridge.cpp",
    "send_authreply109_bridge.cpp",
    "send_authreply1_bridge.cpp",
    "send_authreq1_server_bridge.cpp",
    "send_cdkey_reply_bridge.cpp",
    "send_changepassack_bridge.cpp",
    "send_channellist_bridge.cpp",
    "send_charlistreply_bridge.cpp",
    "send_chatevent_compose_bridge.cpp",
    "send_clancreateinvitereply_bridge.cpp",
    "send_clandisbandreply_bridge.cpp",
    "send_claninfo_bridge.cpp",
    "send_clan_invitereply_bridge.cpp",
    "send_clan_membernewchief_reply_bridge.cpp",
    "send_clanmember_rankupdate_reply_bridge.cpp",
    "send_clanmember_remove_reply_bridge.cpp",
    "send_clan_motdreply_bridge.cpp",
    "send_createacct_reply_bridge.cpp",
    "send_echoreq_bridge.cpp",
    "send_fileinforeply_bridge.cpp",
    "send_friendadd_ack_bridge.cpp",
    "send_frienddel_ack_bridge.cpp",
    "send_friendinfo_bridge.cpp",
    "send_friendmove_ack_bridge.cpp",
    "send_friendslist_bridge.cpp",
    "send_gamelistreply_bridge.cpp",
    "send_handshake_bridge.cpp",
    "send_iconreply_bridge.cpp",
    "send_ladderreply_bridge.cpp",
    "send_laddersearchreply_bridge.cpp",
    "send_loginreply_bridge.cpp",
    "send_loginreply_w3_bridge.cpp",
    "send_logonproof_reply_bridge.cpp",
    "send_mapauthreply1_bridge.cpp",
    "send_mapauthreply2_bridge.cpp",
    "send_messagebox_bridge.cpp",
    "send_passchange_bridge.cpp",
    "send_playerinforeply_bridge.cpp",
    "send_profilereply_bridge.cpp",
    "send_raw_text_bridge.cpp",
    "send_readmemory_bridge.cpp",
    "send_realmjoin_bridge.cpp",
    "send_realmlistlegacy_bridge.cpp",
    "send_requiredwork_bridge.cpp",
    "send_startgame1ack_bridge.cpp",
    "send_startgame3ack_bridge.cpp",
    "send_startgame4ack_bridge.cpp",
    "send_statsreply_bridge.cpp",
    "send_w3route_bridge.cpp",
}

OTHER_BRIDGE_FILES = {
    "ads_bridge.cpp",
    "anongame_inforeply_bridge.cpp",
    "chat_command_bridge.cpp",
    "clan_bridges.cpp",
    "clan_profile_bridge.cpp",
    "command_dispatch_bridge.cpp",
    "encode_clanmemberupdate_bridge.cpp",
    "get_icon_bridge.cpp",
    "profile_bridge.cpp",
    "set_icon_bridge.cpp",
    "tournament_bridge.cpp",
    "handle_d2cs_link.cpp",
    "send_d2cs_bnetd_bridges.cpp",
}

PREFS_BRIDGE_FILE = "prefs_bridge.cpp"


def read_csv() -> List[Dict]:
    """Read bridge-symbols.csv"""
    rows = []
    with open(CSV_PATH, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def write_csv(rows: List[Dict]) -> None:
    """Write bridge-symbols.csv"""
    with open(CSV_PATH, 'w', newline='') as f:
        fieldnames = rows[0].keys() if rows else []
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def delete_file(filepath: Path) -> bool:
    """Delete a file and return True if successful"""
    try:
        if filepath.exists():
            filepath.unlink()
            print(f"  ✓ Deleted {filepath.name}")
            return True
        else:
            print(f"  ✗ File not found: {filepath.name}")
            return False
    except Exception as e:
        print(f"  ✗ Error deleting {filepath.name}: {e}")
        return False


def delete_header_file(basename: str) -> bool:
    """Delete corresponding header file"""
    header_path = LEGACY_BNETD_INCLUDE / f"{basename.replace('.cpp', '.hpp')}"
    if header_path.exists():
        try:
            header_path.unlink()
            print(f"  ✓ Deleted header {header_path.name}")
            return True
        except Exception as e:
            print(f"  ✗ Error deleting header {header_path.name}: {e}")
            return False
    return True


def update_cmakelists(files_to_remove: Set[str]) -> bool:
    """Remove file references from CMakeLists.txt"""
    try:
        with open(CMAKELISTS_PATH, 'r') as f:
            content = f.read()
        
        original_content = content
        
        # Remove each file reference
        for filename in files_to_remove:
            # Match patterns like:
            #   src/filename.cpp
            #   src/filename.cpp  # comment
            patterns = [
                rf'\s*src/{re.escape(filename)}\s*(?:#.*)?$',
                rf'\s*src/{re.escape(filename)}\s*$',
            ]
            for pattern in patterns:
                content = re.sub(pattern, '', content, flags=re.MULTILINE)
        
        # Clean up multiple blank lines
        content = re.sub(r'\n\n\n+', '\n\n', content)
        
        if content != original_content:
            with open(CMAKELISTS_PATH, 'w') as f:
                f.write(content)
            print(f"  ✓ Updated CMakeLists.txt (removed {len(files_to_remove)} file references)")
            return True
        else:
            print(f"  ✗ No changes made to CMakeLists.txt")
            return False
    except Exception as e:
        print(f"  ✗ Error updating CMakeLists.txt: {e}")
        return False


def update_csv_status(rows: List[Dict], files_to_delete: Set[str], status: str, notes: str) -> int:
    """Update CSV status for symbols in deleted files"""
    count = 0
    for row in rows:
        legacy_file = row.get('legacy_file', '')
        if legacy_file in files_to_delete and row.get('status', '') != 'DELETED':
            row['status'] = status
            row['notes'] = notes
            count += 1
    return count


def main():
    print("=" * 80)
    print("Phase 2 Bridge Migration Automation")
    print("=" * 80)
    
    # Read CSV
    print("\n[1/5] Reading bridge-symbols.csv...")
    rows = read_csv()
    print(f"  ✓ Read {len(rows)} symbols")
    
    # Collect files to delete
    print("\n[2/5] Collecting files to delete...")
    files_to_delete = OBSERVATION_ONLY_FILES | SEND_BRIDGE_FILES | OTHER_BRIDGE_FILES
    print(f"  ✓ Will delete {len(files_to_delete)} files:")
    print(f"    - Observation-only: {len(OBSERVATION_ONLY_FILES)}")
    print(f"    - Send-bridges: {len(SEND_BRIDGE_FILES)}")
    print(f"    - Other: {len(OTHER_BRIDGE_FILES)}")
    
    # Delete files
    print("\n[3/5] Deleting bridge files...")
    deleted_count = 0
    for filename in sorted(files_to_delete):
        filepath = LEGACY_BNETD_SRC / filename
        if delete_file(filepath):
            delete_header_file(filename)
            deleted_count += 1
    print(f"  ✓ Deleted {deleted_count} files")
    
    # Update CMakeLists.txt
    print("\n[4/5] Updating CMakeLists.txt...")
    update_cmakelists(files_to_delete)
    
    # Update CSV
    print("\n[5/5] Updating bridge-symbols.csv...")
    
    # Mark deleted files as DELETED
    deleted_count = update_csv_status(
        rows,
        files_to_delete,
        "DELETED",
        "Observation-only or already migrated to v3"
    )
    print(f"  ✓ Marked {deleted_count} symbols as DELETED")
    
    # Mark prefs_bridge.cpp as MIGRATED
    migrated_count = 0
    for row in rows:
        if row.get('legacy_file') == PREFS_BRIDGE_FILE and row.get('status') != 'DELETED':
            row['status'] = 'MIGRATED'
            row['notes'] = 'Config loading already integrated into v3 infra/config/'
            migrated_count += 1
    print(f"  ✓ Marked {migrated_count} symbols as MIGRATED (prefs_bridge)")
    
    # Write CSV
    write_csv(rows)
    print(f"  ✓ Wrote updated CSV")
    
    # Summary
    print("\n" + "=" * 80)
    print("Migration Summary")
    print("=" * 80)
    print(f"Files deleted: {deleted_count}")
    print(f"Symbols marked DELETED: {deleted_count}")
    print(f"Symbols marked MIGRATED: {migrated_count}")
    print(f"Total symbols processed: {deleted_count + migrated_count}")
    print("\nNext steps:")
    print("1. Verify build: cmake --build build/")
    print("2. Run tests: ctest")
    print("3. Review changes: git diff")
    print("4. Commit: git commit -m 'Plan 03 Step 2 Phase 2: Migrate complex logic bridges'")


if __name__ == "__main__":
    main()
