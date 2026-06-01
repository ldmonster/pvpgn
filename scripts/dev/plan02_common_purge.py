#!/usr/bin/env python3
"""
Plan 02: src/common/ Purge - Automated migration script
Systematically deletes or relocates files from src/common/
"""

import os
import shutil
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple

# Classification mapping
CLASSIFICATIONS = {
    # DELETE: Container replacements
    "delete": [
        "hashtable.cpp", "hashtable.h",
        "list.cpp", "list.h",
        "queue.cpp", "queue.h",
        "elist.h",
        "eventlog.cpp", "eventlog.h",
        "gui_printf.cpp", "gui_printf.h",
    ],
    
    # PLAN-08: Crypto files (defer)
    "plan-08": [
        "bnethash.cpp", "bnethash.h",
        "bnethashconv.cpp", "bnethashconv.h",
        "bnetsrp3.cpp", "bnetsrp3.h",
        "bigint.cpp", "bigint.h",
        "wolhash.cpp", "wolhash.h",
    ],
    
    # MOVE-CORE: String/util utilities
    "move-core-strings": {
        "dest": "src/core/strings",
        "files": ["xstring.cpp", "xstring.h", "util_string.cpp"],
    },
    
    "move-core-encoding": {
        "dest": "src/core/encoding",
        "files": ["util_hex.cpp"],
    },
    
    "move-core-types": {
        "dest": "src/core/types",
        "files": ["bn_type.cpp", "bn_type.h", "tag.cpp", "tag.h", 
                  "tag_core.cpp", "tag_clienttag.cpp", "tag_wol.cpp"],
    },
    
    "move-core-time": {
        "dest": "src/core/time",
        "files": ["bnettime.cpp", "bnettime.h", "util_time.cpp"],
    },
    
    "move-core-util": {
        "dest": "src/core/util",
        "files": ["util.cpp", "util.h", "util_file.cpp", "token.cpp", "token.h",
                  "trans.cpp", "trans.h", "proginfo.cpp", "proginfo.h",
                  "peerchat.cpp", "peerchat.h", "rcm.cpp", "rcm.h",
                  "hash_tuple.hpp", "flags.h", "introtate.h"],
    },
    
    "move-core-debug": {
        "dest": "src/core/debug",
        "files": ["hexdump.cpp", "hexdump.h"],
    },
    
    "move-core-error": {
        "dest": "src/core/error",
        "files": ["systemerror.cpp", "systemerror.h"],
    },
    
    "move-core-config": {
        "dest": "src/core/config",
        "files": ["conf.cpp", "conf.h"],
    },
    
    "move-core-version": {
        "dest": "src/core/version",
        "files": ["version.h"],
    },
    
    "move-core-net": {
        "dest": "src/core/net",
        "files": ["addr.cpp", "addr.h", "addr_core.cpp", "addr_list.cpp", "addr_netaddr.cpp"],
    },
    
    # MOVE-INFRA-NET: Network utilities
    "move-infra-net": {
        "dest": "src/infra/net",
        "files": ["fdwatch.cpp", "fdwatch.h", "fdwatch_epoll.cpp", "fdwatch_epoll.h",
                  "fdwatch_kqueue.cpp", "fdwatch_kqueue.h", "fdwatch_poll.cpp", "fdwatch_poll.h",
                  "fdwatch_select.cpp", "fdwatch_select.h", "fdwbackend.cpp", "fdwbackend.h",
                  "network.cpp", "network.h"],
    },
    
    # MOVE-INFRA-PROCESS: Process utilities
    "move-infra-process": {
        "dest": "src/infra/process",
        "files": ["give_up_root_privileges.cpp", "give_up_root_privileges.h",
                  "rlimit.cpp", "rlimit.h"],
    },
    
    # KEEP-WIRE: Protocol headers (move to src/protocol/)
    "keep-wire": [
        "packet.cpp", "packet.h", "packet_buffer.cpp", "packet_reader.cpp", "packet_writer.cpp",
        "field_sizes.h", "lstr.h", "tracker.h",
        "anongame_protocol.h", "bnet_protocol.h", "bot_protocol.h", "init_protocol.h",
        "irc_protocol.h", "udp_protocol.h", "file_protocol.h", "wol_gameres_protocol.h",
        "d2char_checksum.cpp", "d2char_checksum.h", "d2char_file.h",
        "d2cs_bnetd_protocol.h", "d2cs_d2dbs_ladder.h", "d2cs_d2gs_character.h",
        "d2cs_d2gs_protocol.h", "d2cs_protocol.h", "d2game_protocol.h",
    ],
    
    # SETUP headers (keep in src/common/ for now)
    "keep-setup": [
        "setup_before.h", "setup_after.h",
    ],
}

def run_command(cmd: str, cwd: str = "/home/cnupt/work/pvpgn") -> Tuple[int, str]:
    """Run a shell command and return exit code and output."""
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    return result.returncode, result.stdout + result.stderr

def delete_files(files: List[str]) -> int:
    """Delete files from src/common/."""
    deleted = 0
    for f in files:
        path = Path("/home/cnupt/work/pvpgn/src/common") / f
        if path.exists():
            path.unlink()
            print(f"  ✓ Deleted {f}")
            deleted += 1
    return deleted

def move_files(src_dir: str, dest_dir: str, files: List[str]) -> int:
    """Move files from src/common/ to destination."""
    dest_path = Path(dest_dir)
    dest_path.mkdir(parents=True, exist_ok=True)
    
    moved = 0
    for f in files:
        src_file = Path("/home/cnupt/work/pvpgn/src/common") / f
        dest_file = dest_path / f
        if src_file.exists():
            shutil.move(str(src_file), str(dest_file))
            print(f"  ✓ Moved {f} to {dest_dir}")
            moved += 1
    return moved

def main():
    print("=" * 70)
    print("Plan 02: src/common/ Purge - Automated Migration")
    print("=" * 70)
    
    total_deleted = 0
    total_moved = 0
    
    # Phase 1: Delete container replacements and eventlog shim
    print("\n[Phase 1] Deleting container replacements and eventlog shim...")
    deleted = delete_files(CLASSIFICATIONS["delete"])
    total_deleted += deleted
    print(f"  Total deleted: {deleted}")
    
    # Phase 2: Skip crypto files (Plan 08)
    print("\n[Phase 2] Skipping crypto files (Plan 08)...")
    print(f"  Files to defer: {', '.join(CLASSIFICATIONS['plan-08'])}")
    
    # Phase 3: Move core utilities
    print("\n[Phase 3] Moving core utilities...")
    for key, config in CLASSIFICATIONS.items():
        if key.startswith("move-core-"):
            dest = config["dest"]
            files = config["files"]
            moved = move_files("/home/cnupt/work/pvpgn/src/common", dest, files)
            total_moved += moved
            print(f"  Moved {moved} files to {dest}")
    
    # Phase 4: Move infra utilities
    print("\n[Phase 4] Moving infra utilities...")
    for key, config in CLASSIFICATIONS.items():
        if key.startswith("move-infra-"):
            dest = config["dest"]
            files = config["files"]
            moved = move_files("/home/cnupt/work/pvpgn/src/common", dest, files)
            total_moved += moved
            print(f"  Moved {moved} files to {dest}")
    
    # Phase 5: Keep wire protocol headers (for now)
    print("\n[Phase 5] Wire protocol headers (keeping in src/common/ for now)...")
    print(f"  Files to keep: {len(CLASSIFICATIONS['keep-wire'])} files")
    
    # Phase 6: Keep setup headers
    print("\n[Phase 6] Setup headers (keeping in src/common/)...")
    print(f"  Files to keep: {', '.join(CLASSIFICATIONS['keep-setup'])}")
    
    print("\n" + "=" * 70)
    print(f"Summary:")
    print(f"  Total deleted: {total_deleted}")
    print(f"  Total moved: {total_moved}")
    print(f"  Total deferred (Plan 08): {len(CLASSIFICATIONS['plan-08'])}")
    print(f"  Total kept (wire protocol): {len(CLASSIFICATIONS['keep-wire'])}")
    print(f"  Total kept (setup): {len(CLASSIFICATIONS['keep-setup'])}")
    print("=" * 70)

if __name__ == "__main__":
    main()
