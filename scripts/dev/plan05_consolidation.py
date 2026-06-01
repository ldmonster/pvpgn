#!/usr/bin/env python3
"""
Plan 05 — application/ports/ Consolidation
Automates the migration of remaining cross-cutting headers and deletion of shims.
"""

import os
import re
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple

# Mapping of headers to their new locations
HEADER_MIGRATIONS = {
    # Cross-cutting headers that need to move
    "command_registry.hpp": "src/domain/chat/include/domain/chat/ports/command_registry.hpp",
    "event_loop.hpp": "src/core/runtime/include/core/runtime/event_loop.hpp",
    "event_bus.hpp": "src/domain/shared/include/domain/shared/event_bus.hpp",
    "metrics_registry.hpp": None,  # Delete - re-export shim
    "config_subscriber.hpp": "src/core/config/include/core/config/config_subscriber.hpp",
    "random_source.hpp": "src/core/crypto/include/core/crypto/random_source.hpp",
    "script_host.hpp": "src/infra/scripting/include/infra/scripting/script_host.hpp",
    "news_store.hpp": "src/domain/social/include/domain/social/ports/news_store.hpp",
    "icon_provider.hpp": "src/domain/content/include/domain/content/icon_provider.hpp",
    "trace_sink.hpp": "src/core/observability/include/core/observability/trace_sink.hpp",
    "unit_of_work.hpp": "src/application/persistence/include/application/persistence/unit_of_work.hpp",
    "unit_of_work_factory.hpp": "src/application/persistence/include/application/persistence/unit_of_work_factory.hpp",
}

# Shims to delete (already migrated to domain/<ctx>/ports.hpp)
SHIMS_TO_DELETE = [
    "account_ban_repository.hpp",
    "account_repository.hpp",
    "anongame_compressor.hpp",
    "audit_log.hpp",
    "channel_repository.hpp",
    "channel_store.hpp",
    "clan_repository.hpp",
    "connection_handler.hpp",
    "friend_list_repository.hpp",
    "game_repository.hpp",
    "helpfile_source.hpp",
    "ip_ban_repository.hpp",
    "ladder_repository.hpp",
    "mail_store.hpp",
    "message_broadcaster.hpp",
    "message_router.hpp",
    "password_hasher.hpp",
    "permission_checker.hpp",
    "realm_repository.hpp",
    "session_registry.hpp",
    "session_token_issuer.hpp",
    "team_repository.hpp",
]

def find_includes(header_name: str) -> List[Tuple[str, int, str]]:
    """Find all files that include a header."""
    results = []
    pattern = f'#include [<"]application/ports/{header_name}[>"]'
    
    try:
        output = subprocess.check_output(
            ["grep", "-r", "-n", pattern, "src/", "tests/"],
            stderr=subprocess.DEVNULL,
            text=True
        )
        for line in output.strip().split("\n"):
            if line:
                parts = line.split(":")
                filepath = parts[0]
                line_num = int(parts[1])
                content = ":".join(parts[2:])
                results.append((filepath, line_num, content))
    except subprocess.CalledProcessError:
        pass
    
    return results

def update_includes(old_path: str, new_path: str) -> int:
    """Update all includes from old_path to new_path."""
    count = 0
    includes = find_includes(os.path.basename(old_path))
    
    for filepath, line_num, content in includes:
        with open(filepath, "r") as f:
            lines = f.readlines()
        
        # Replace the include
        old_include = f'#include "application/ports/{os.path.basename(old_path)}"'
        new_include = f'#include "{new_path.replace("src/", "").replace("/include/", "")}"'
        
        for i, line in enumerate(lines):
            if old_include in line:
                lines[i] = line.replace(old_include, new_include)
                count += 1
        
        with open(filepath, "w") as f:
            f.writelines(lines)
    
    return count

def main():
    print("Plan 05 — application/ports/ Consolidation")
    print("=" * 60)
    
    # Step 1: Delete shims
    print("\n[Step 1] Deleting shims (already migrated to domain/<ctx>/ports.hpp)...")
    shims_dir = "src/application/ports/include/application/ports"
    for shim in SHIMS_TO_DELETE:
        shim_path = os.path.join(shims_dir, shim)
        if os.path.exists(shim_path):
            os.remove(shim_path)
            print(f"  ✓ Deleted {shim}")
    
    # Step 2: Move cross-cutting headers
    print("\n[Step 2] Moving cross-cutting headers...")
    for old_name, new_path in HEADER_MIGRATIONS.items():
        if new_path is None:
            # Delete metrics_registry.hpp
            old_path = os.path.join(shims_dir, old_name)
            if os.path.exists(old_path):
                os.remove(old_path)
                print(f"  ✓ Deleted {old_name} (metrics re-export shim)")
            continue
        
        old_path = os.path.join(shims_dir, old_name)
        if not os.path.exists(old_path):
            print(f"  ⚠ {old_name} not found")
            continue
        
        # Create destination directory
        dest_dir = os.path.dirname(new_path)
        os.makedirs(dest_dir, exist_ok=True)
        
        # Copy file
        with open(old_path, "r") as f:
            content = f.read()
        
        with open(new_path, "w") as f:
            f.write(content)
        
        # Update includes
        count = update_includes(old_path, new_path)
        print(f"  ✓ Moved {old_name} → {new_path} ({count} includes updated)")
        
        # Delete old file
        os.remove(old_path)
    
    # Step 3: Delete src/application/ports/
    print("\n[Step 3] Deleting src/application/ports/...")
    ports_dir = "src/application/ports"
    if os.path.exists(ports_dir):
        import shutil
        shutil.rmtree(ports_dir)
        print(f"  ✓ Deleted {ports_dir}")
    
    # Step 4: Update CMakeLists.txt
    print("\n[Step 4] Updating src/application/CMakeLists.txt...")
    cmake_path = "src/application/CMakeLists.txt"
    with open(cmake_path, "r") as f:
        content = f.read()
    
    # Remove add_subdirectory(ports)
    content = re.sub(r'add_subdirectory\s*\(\s*ports\s*\)\s*\n?', '', content)
    
    with open(cmake_path, "w") as f:
        f.write(content)
    print(f"  ✓ Removed add_subdirectory(ports) from {cmake_path}")
    
    print("\n" + "=" * 60)
    print("Plan 05 consolidation complete!")

if __name__ == "__main__":
    main()
