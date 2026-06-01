#!/usr/bin/env python3
"""
Bridge migration orchestrator for Plan 03, Step 2.

This script analyzes bridge symbols from planstwo/inventory/bridge-symbols.csv
and determines migration strategy for each symbol:
  - MIGRATED: v3 callee exists, bridge is thin wrapper
  - DELETED: bridge removed, v3 callee handles it
  - KEPT: bridge contains logic not yet in v3
  - MOVED: logic moved from bridge to v3 use case

Usage:
  python3 scripts/dev/migrate_bridges.py --analyze
  python3 scripts/dev/migrate_bridges.py --migrate [family]
  python3 scripts/dev/migrate_bridges.py --report
"""

import csv
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from enum import Enum

class MigrationStatus(Enum):
    """Status of a bridge symbol migration."""
    PENDING = "PENDING"
    MIGRATED = "MIGRATED"
    DELETED = "DELETED"
    KEPT = "KEPT"
    MOVED = "MOVED"
    ERROR = "ERROR"

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
    
    def find_bridge_definition(self, symbol: str) -> Optional[Tuple[Path, int]]:
        """Find the definition of a bridge symbol in source files.
        
        Returns: (file_path, line_number) or None
        """
        # Search for extern "C" definition
        pattern = rf'extern\s+"C"\s+.*{re.escape(symbol)}\s*\('
        
        for cpp_file in self.legacy_root.glob("src/**/*.cpp"):
            try:
                with open(cpp_file, 'r') as f:
                    for i, line in enumerate(f, 1):
                        if re.search(pattern, line):
                            return (cpp_file, i)
            except Exception as e:
                print(f"Warning: Could not read {cpp_file}: {e}")
        
        return None
    
    def is_thin_wrapper(self, symbol: str) -> bool:
        """Determine if a bridge is a thin wrapper (just delegates to v3).
        
        A thin wrapper typically:
        - Has < 10 lines of code
        - Just calls a v3 function and returns
        - No complex logic or state management
        """
        result = self.find_bridge_definition(symbol)
        if not result:
            return False
        
        file_path, line_num = result
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
                # Get function body (rough heuristic)
                start = line_num - 1
                end = min(start + 15, len(lines))
                body = ''.join(lines[start:end])
                
                # Count actual code lines (excluding braces, comments)
                code_lines = [l.strip() for l in body.split('\n') 
                             if l.strip() and not l.strip().startswith('//')]
                
                # Thin wrapper: mostly just a call and return
                return len(code_lines) < 10
        except Exception as e:
            print(f"Warning: Could not analyze {file_path}: {e}")
            return False
    
    def analyze_family(self, family: str) -> Dict[str, int]:
        """Analyze migration status for a handler family."""
        family_symbols = [s for s in self.symbols if s.handler_family == family]
        
        stats = {
            'total': len(family_symbols),
            'thin_wrappers': 0,
            'complex_logic': 0,
            'no_v3_callee': 0,
        }
        
        for sym in family_symbols:
            if not sym.v3_callee or sym.v3_callee == 'N/A':
                stats['no_v3_callee'] += 1
            elif self.is_thin_wrapper(sym.symbol):
                stats['thin_wrappers'] += 1
            else:
                stats['complex_logic'] += 1
        
        return stats
    
    def generate_report(self):
        """Generate migration analysis report."""
        families = set(s.handler_family for s in self.symbols)
        
        print("\n" + "="*70)
        print("BRIDGE MIGRATION ANALYSIS REPORT")
        print("="*70)
        
        total_stats = {
            'total': 0,
            'thin_wrappers': 0,
            'complex_logic': 0,
            'no_v3_callee': 0,
        }
        
        for family in sorted(families):
            stats = self.analyze_family(family)
            print(f"\n{family.upper()} ({stats['total']} symbols)")
            print(f"  Thin wrappers (can delete):     {stats['thin_wrappers']}")
            print(f"  Complex logic (need migration): {stats['complex_logic']}")
            print(f"  No v3 callee (need creation):   {stats['no_v3_callee']}")
            
            for key in total_stats:
                total_stats[key] += stats[key]
        
        print(f"\n{'TOTAL':.<50} {total_stats['total']}")
        print(f"{'Thin wrappers (deletable)':.<50} {total_stats['thin_wrappers']}")
        print(f"{'Complex logic (migrate)':.<50} {total_stats['complex_logic']}")
        print(f"{'No v3 callee (create)':.<50} {total_stats['no_v3_callee']}")
        print("="*70 + "\n")
    
    def update_csv_with_status(self):
        """Update the CSV file with migration status."""
        # Read current data
        rows = []
        with open(self.csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                rows.append(row)
        
        # Update status for each symbol
        for row in rows:
            symbol = row['symbol']
            sym_obj = next((s for s in self.symbols if s.symbol == symbol), None)
            if sym_obj:
                row['status'] = sym_obj.status
                row['notes'] = sym_obj.notes
        
        # Write back with status column
        fieldnames = ['symbol', 'legacy_file', 'v3_callee', 'loc', 'test_coverage', 
                     'handler_family', 'status', 'notes']
        
        with open(self.csv_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        
        print(f"Updated {self.csv_path}")

def main():
    """Main entry point."""
    migrator = BridgeMigrator()
    
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "--analyze":
        migrator.generate_report()
    elif command == "--report":
        migrator.generate_report()
    elif command == "--update-csv":
        migrator.update_csv_with_status()
    else:
        print(f"Unknown command: {command}")
        print(__doc__)
        sys.exit(1)

if __name__ == "__main__":
    main()
