# contrib/

This directory contains community-contributed utilities and helper scripts that are
not part of the core PvPGN server build but may be useful to operators and integrators.

## Contents

| File | Description |
|------|-------------|
| `pvpgn_hash.inc.php` | PHP implementation of the PvPGN password hash algorithm, useful for web portals that need to authenticate against a PvPGN user database. |
| `pvpgn_wol_hash.inc.php` | PHP implementation of the Westwood Online (WOL) variant of the PvPGN hash algorithm. |

## Usage

These files are provided **as-is** with no guarantee of maintenance or compatibility
with future PvPGN versions. They are not compiled or tested as part of the CI pipeline.

If you maintain one of these utilities and would like to keep it up to date, pull
requests are welcome.
