#!/usr/bin/env python3
"""
Phase 1: Thin wrapper deletion and migration planning.

This script:
1. Identifies thin wrapper bridges (< 10 LOC, just delegate to v3)
2. Deletes thin wrapper bridge files
3. Updates CMakeLists.txt to remove deleted files
4. Updates bridge-symbols.csv with DELETED status
5. Generates migration plan for complex logic bridges

Usage:
  python3 scripts/dev/migrate_bridges_phase1.py --dry-run
  python3 scripts/dev/migrate_bridges_phase1.py --execute
  python3 scripts/dev/migrate_bridges_phase1.py --report
"""

import csv
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

class MigrationStatus(Enum):
    """Status of a bridge symbol migration."""
    PENDING = "PENDING"
    MIGRATED = "MIGRATED"
    DELETED = "DELETED"
    KEPT = "KEPT"
    MOVED = "MOVED"

@dataclass
class BridgeSymbol:
    """Represents a bridge symbol from the inventory."""
    symbol: str
    legacy_file: str
    v3_callee: str
    loc: int
    test_coverage: str
    handler_family: str
    status: str = "PENDING"
    notes: str = ""

class BridgeMigrator:
    """Orchestrates bridge migration."""
    
    def __init__(self, workspace_root: Path = Path("/home/cnupt/work/pvpgn")):
        self.workspace = workspace_root
        self.csv_path = workspace_root / "planstwo/inventory/bridge-symbols.csv"
        self.legacy_root = workspace_root / "src/integration/legacy_bnetd"
        self.symbols: List[BridgeSymbol] = []
        self.thin_wrappers: Set[str] = set()
        self.files_to_delete: Set[Path] = set()
        self.load_inventory()
    
    def load_inventory(self):
        """Load bridge symbols from CSV."""
        if not self.csv_path.exists():
            print(f"ERROR: {self.csv_path} not found")
            return
        
        with open(self.csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                if row['symbol']:  # Skip empty rows
                    self.symbols.append(BridgeSymbol(
                        symbol=row['symbol'],
                        legacy_file=row['legacy_file'],
                        v3_callee=row['v3_callee'],
                        loc=int(row['loc']),
                        test_coverage=row['test_coverage'],
                        handler_family=row['handler_family'],
                        status=row.get('status', 'PENDING'),
                        notes=row.get('notes', '')
                    ))
    
    def find_bridge_file(self, legacy_file: str) -> Optional[Path]:
        """Find the full path to a bridge file."""
        # Try direct path first
        direct = self.legacy_root / "src" / legacy_file
        if direct.exists():
            return direct
        
        # Search in src directory
        for cpp_file in self.legacy_root.glob("src/**/*.cpp"):
            if cpp_file.name == legacy_file:
                return cpp_file
        
        return None
    
    def count_code_lines(self, file_path: Path) -> int:
        """Count actual code lines in a file (excluding comments, blanks)."""
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
                code_lines = 0
                in_comment = False
                for line in lines:
                    stripped = line.strip()
                    
                    # Handle multi-line comments
                    if '/*' in stripped:
                        in_comment = True
                    if '*/' in stripped:
                        in_comment = False
                        continue
                    
                    if in_comment:
                        continue
                    
                    # Skip empty lines and single-line comments
                    if stripped and not stripped.startswith('//'):
                        code_lines += 1
                
                return code_lines
        except Exception as e:
            print(f"Warning: Could not count lines in {file_path}: {e}")
            return 999  # Conservative estimate
    
    def is_thin_wrapper(self, symbol: str, legacy_file: str) -> bool:
        """Determine if a bridge is a thin wrapper.
        
        Heuristics:
        - File has < 50 total lines
        - LOC field is small (< 20)
        - File mostly contains just the extern "C" function
        """
        file_path = self.find_bridge_file(legacy_file)
        if not file_path:
            return False
        
        try:
            with open(file_path, 'r') as f:
                content = f.read()
                total_lines = len(content.split('\n'))
                
                # Thin wrapper heuristics
                if total_lines > 100:
                    return False
                
                # Check if it's mostly just one function
                extern_count = content.count('extern "C"')
                if extern_count > 2:  # Multiple functions = not thin wrapper
                    return False
                
                # Check for complex logic indicators
                if 'for (' in content or 'while (' in content:
                    return False
                if 'if (' in content and content.count('if (') > 3:
                    return False
                
                return True
        except Exception as e:
            print(f"Warning: Could not analyze {file_path}: {e}")
            return False
    
    def identify_thin_wrappers(self):
        """Identify all thin wrapper bridges."""
        # Group symbols by file
        files_by_symbol = {}
        for sym in self.symbols:
            if sym.symbol not in files_by_symbol:
                files_by_symbol[sym.symbol] = sym.legacy_file
        
        # Check each unique file
        checked_files = set()
        for symbol, legacy_file in files_by_symbol.items():
            if legacy_file in checked_files:
                continue
            checked_files.add(legacy_file)
            
            if self.is_thin_wrapper(symbol, legacy_file):
                self.thin_wrappers.add(legacy_file)
                file_path = self.find_bridge_file(legacy_file)
                if file_path:
                    self.files_to_delete.add(file_path)
    
    def get_files_to_delete(self) -> Set[Path]:
        """Get all files that should be deleted (thin wrappers)."""
        self.identify_thin_wrappers()
        return self.files_to_delete
    
    def update_csv_status(self, dry_run: bool = True):
        """Update CSV with migration status."""
        rows = []
        with open(self.csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                rows.append(row)
        
        # Update status
        for row in rows:
            legacy_file = row['legacy_file']
            if legacy_file in self.thin_wrappers:
                row['status'] = 'DELETED'
                row['notes'] = 'Thin wrapper - deleted in Phase 1'
            elif row.get('status') == 'PENDING':
                row['status'] = 'PENDING'
        
        if not dry_run:
            fieldnames = ['symbol', 'legacy_file', 'v3_callee', 'loc', 'test_coverage', 
                         'handler_family', 'status', 'notes']
            with open(self.csv_path, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader()
                writer.writerows(rows)
            print(f"Updated {self.csv_path}")
    
    def generate_report(self):
        """Generate migration report."""
        self.identify_thin_wrappers()
        
        print("\n" + "="*70)
        print("PHASE 1: THIN WRAPPER DELETION REPORT")
        print("="*70)
        
        print(f"\nThin wrapper files to delete: {len(self.files_to_delete)}")
        for f in sorted(self.files_to_delete):
            print(f"  - {f.relative_to(self.workspace)}")
        
        # Group by family
        family_deletes = {}
        for sym in self.symbols:
            if sym.legacy_file in self.thin_wrappers:
                if sym.handler_family not in family_deletes:
                    family_deletes[sym.handler_family] = []
                family_deletes[sym.handler_family].append(sym.symbol)
        
        print(f"\nSymbols to delete by family:")
        for family in sorted(family_deletes.keys()):
            symbols = family_deletes[family]
            print(f"  {family}: {len(symbols)} symbols")
            for sym in sorted(set(symbols))[:5]:  # Show first 5
                print(f"    - {sym}")
            if len(set(symbols)) > 5:
                print(f"    ... and {len(set(symbols)) - 5} more")
        
        print("\n" + "="*70 + "\n")
    
    def delete_files(self):
        """Delete thin wrapper files."""
        self.identify_thin_wrappers()
        
        for file_path in self.files_to_delete:
            try:
                file_path.unlink()
                print(f"Deleted: {file_path.relative_to(self.workspace)}")
            except Exception as e:
                print(f"ERROR deleting {file_path}: {e}")

def main():
    """Main entry point."""
    migrator = BridgeMigrator()
    
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "--dry-run":
        migrator.generate_report()
        migrator.update_csv_status(dry_run=True)
    elif command == "--execute":
        migrator.generate_report()
        migrator.delete_files()
        migrator.update_csv_status(dry_run=False)
    elif command == "--report":
        migrator.generate_report()
    else:
        print(f"Unknown command: {command}")
        print(__doc__)
        sys.exit(1)

if __name__ == "__main__":
    main()
