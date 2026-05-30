# TOML migration guide

Starting with the v3 build (`PVPGN_BUILD_V3=ON`) the three pvpgn services
(`bnetd`, `d2cs`, `d2dbs`) are configured **exclusively** from TOML files.
The legacy `bnetd.conf` / `d2cs.conf` / `d2dbs.conf` parsers are still
present in the legacy build path but are no longer installed, parsed or
honoured under v3. This page is the operator-facing migration cheatsheet.

## TL;DR

| Legacy file       | v3 file        | Loader                                |
| ----------------- | -------------- | ------------------------------------- |
| `bnetd.conf`      | `bnetd.toml`   | `pvpgn_v3_prefs_load_toml`            |
| `d2cs.conf`       | `d2cs.toml`    | `pvpgn_v3_d2cs_prefs_load_toml`       |
| `d2dbs.conf`      | `d2dbs.toml`   | `pvpgn_v3_d2dbs_prefs_load_toml`      |

The v3 binaries derive the `.toml` path from the `-c <preffile>` cmdline
argument by swapping the extension. A start-up parse failure is fatal.
A `SIGHUP` reload that fails to parse is logged at `error` level and the
previous snapshot stays in effect.

## Mechanical translation rules

1. Group every legacy `key = value` line under one of the v3 sections
   (see per-service tables below).
2. Quote every string value: `servername = MyServer` -> `servername = "MyServer"`.
3. Booleans become real TOML booleans: `track = 1` -> `track = true`, `0` -> `false`.
4. Comment syntax is `#` (TOML); legacy `#` comments translate verbatim.
5. Comma-separated lists stay as quoted strings: `loglevels = info,error`
   -> `loglevels = "info,error"` (the v3 parser keeps the CSV shape).

## bnetd.conf -> bnetd.toml

The v3 schema groups keys into four sections: `[server]`, `[log]`,
`[network]` and `[files]`. Keys that did not survive the migration are
listed at the bottom of `conf/bnetd.toml.in` as TODOs.

```toml
[server]
servername = "PvPGN Realm"
hostname   = "myhost.example.com"
track      = true

[log]
logfile   = "/var/log/bnetd.log"
loglevels = "fatal,error,warn,info"

[network]
servaddrs = "0.0.0.0:6112"
udpaddr   = "0.0.0.0:6112"

[files]
filedir   = "/usr/local/share/pvpgn/files"
i18ndir   = "/usr/local/share/pvpgn/i18n"
```

corresponds to the legacy:

```ini
servername = PvPGN Realm
hostname   = myhost.example.com
track      = 1
logfile    = /var/log/bnetd.log
loglevels  = fatal,error,warn,info
servaddrs  = 0.0.0.0:6112
udpaddr    = 0.0.0.0:6112
filedir    = /usr/local/share/pvpgn/files
i18ndir    = /usr/local/share/pvpgn/i18n
```

## d2cs.conf -> d2cs.toml

Sections: `[server]`, `[log]`, `[network]`, `[files]`.

```toml
[server]
servername = "D2CS"

[log]
logfile   = "/var/log/d2cs.log"
loglevels = "fatal,error,warn,info"

[network]
listen    = "0.0.0.0:6200"
bnetaddr  = "127.0.0.1:6112"

[files]
charsavedir = "/var/lib/pvpgn/d2cs/charsave"
gameservlist = "/etc/pvpgn/d2cs.gameservlist"
```

## d2dbs.conf -> d2dbs.toml

Sections: `[log]`, `[network]`, `[files]`.

```toml
[log]
logfile   = "/var/log/d2dbs.log"
loglevels = "fatal,error,warn,info"

[network]
listen    = "0.0.0.0:6113"

[files]
charsave_dir = "/var/lib/pvpgn/d2dbs/charsave"
charinfo_dir = "/var/lib/pvpgn/d2dbs/charinfo"
```

## Verifying a migrated file

- Start the daemon with `-c /etc/pvpgn/bnetd.toml`. A clean start logs
  `v3 TOML config loaded from '/etc/pvpgn/bnetd.toml'`.
- Issue `/config` (bnetd) or send `SIGHUP` to d2cs/d2dbs and watch the
  eventlog: a full TOML-shaped snapshot of the active configuration is
  written, section by section, identical to what the in-tree
  `format_dump()` helper produces.
- Diff your input `.toml` against the snapshot to spot keys that the
  parser dropped silently (typos, wrong section, ...).
