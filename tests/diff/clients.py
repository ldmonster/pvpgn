# SPDX-License-Identifier: GPL-2.0-or-later
"""Authoritative catalog of every PvPGN-supported client, with a mock-login
dispatcher so each can be driven against the original server (the oracle) and,
where supported, the v3 rewrite.

Three login families cover the whole supported-client matrix:

  * ``ols``  — the classic broken-SHA-1 "old logon system": SID_AUTH_INFO (0x50) →
    SID_AUTH_CHECK (0x51) → SID_CREATE_ACCT1 / SID_LOGONRESPONSE2 (0x3A).
    StarCraft, Brood War, WarCraft II BNE, Diablo, Diablo II (+LoD).
  * ``nls``  — WarCraft III SRP-3 (NLS): 0x50/0x51 then SID_AUTH_ACCOUNTCREATE
    (0x52) / SID_AUTH_ACCOUNTLOGON (0x53) / ...PROOF (0x54). WAR3, W3XP.
  * ``wol``  — Westwood Online: an IRC-dialect handshake on a separate listener
    (wolv1addrs/wolv2addrs). CVERS (SKU→clienttag) → VERCHK → APGAR (password)
    → NICK → USER → welcome/MOTD. C&C, Red Alert 1/2, Tiberian Sun (+Firestorm),
    Yuri's Revenge, Renegade, Nox (+Quest), Dune 2000, Emperor, Westwood Chat.

The ``versions`` lists mirror PvPGN's published supported-clients matrix verbatim.
The version-check is advisory under the test config (allow_unknown_version), so
the differential driver exercises one representative per distinct protocol path
(family + product tag) rather than re-booting the servers for every patch level;
the per-version data is retained so the catalog is the single source of truth.

``arch`` is the platform tag: every supported build here is Windows/x86 ("IX86").
WOL clients carry a ``sku`` (the CVERS SKU number PvPGN maps to a clienttag via
``tag_sku_to_uint``) and ``wolv`` (1 = WOLv1 "Westwood Chat" era, 2 = WOLv2).
"""
from dataclasses import dataclass, field
from typing import Optional


@dataclass(frozen=True)
class Client:
    name: str
    tag: bytes              # 4-byte BNCS/WOL clienttag, e.g. b"STAR"
    family: str             # "ols" | "nls" | "wol"
    versions: tuple         # supported version strings (verbatim from the matrix)
    arch: bytes = b"IX86"   # platform tag
    sku: Optional[int] = None   # WOL CVERS SKU (wol family only)
    wolv: Optional[int] = None  # WOL listener version (1 or 2)
    # Representative version id sent in SID_AUTH_INFO. WC3 builds are >= 0x0D
    # (which gates the "register e-mail" prompt); OLS values are advisory.
    version_id: int = 0xD3


# --- OLS: classic broken-SHA-1 logon -----------------------------------------
_SC_VERSIONS = (
    "1.08", "1.08b", "1.09", "1.09b", "1.10", "1.11", "1.11b", "1.12", "1.12b",
    "1.13", "1.13b", "1.13c", "1.13d", "1.13e", "1.13f", "1.14", "1.15",
    "1.15.1", "1.15.2", "1.15.3", "1.16", "1.16.1", "1.17.0", "1.18.0",
)
_D2_VERSIONS = (
    "1.10", "1.11", "1.11b", "1.12a", "1.13c", "1.14a", "1.14b", "1.14c", "1.14d",
)

OLS_CLIENTS = [
    Client("StarCraft",            b"STAR", "ols", _SC_VERSIONS),
    Client("StarCraft: Brood War", b"SEXP", "ols", _SC_VERSIONS),
    Client("WarCraft II: BNE",     b"W2BN", "ols", ("2.02a", "2.02b")),
    Client("Diablo",               b"DRTL", "ols", ("1.09", "1.09b")),
    Client("Diablo II",            b"D2DV", "ols", _D2_VERSIONS),
    Client("Diablo II: LoD",       b"D2XP", "ols", _D2_VERSIONS),
]

# --- NLS: WarCraft III SRP-3 -------------------------------------------------
_W3_VERSIONS = (
    "1.13a", "1.13b", "1.14a", "1.14b", "1.15a", "1.16a", "1.17a", "1.18a",
    "1.19a", "1.19b", "1.20a", "1.20b", "1.20c", "1.20d", "1.20e", "1.21a",
    "1.21b", "1.22a", "1.23a", "1.24a", "1.24b", "1.24c", "1.24d", "1.24e",
    "1.25b", "1.26a", "1.27a", "1.27b", "1.28", "1.28.1", "1.28.2", "1.28.4",
    "1.28.5",
)

NLS_CLIENTS = [
    Client("WarCraft III: RoC", b"WAR3", "nls", _W3_VERSIONS, version_id=0x1A),
    Client("WarCraft III: TFT", b"W3XP", "nls", _W3_VERSIONS, version_id=0x1A),
]

# --- WOL: Westwood Online (IRC dialect) --------------------------------------
# SKU values are from PvPGN's tag_sku_to_uint(); wolv selects the listener.
WOL_CLIENTS = [
    Client("Westwood Chat Client",        b"WCHT", "wol", ("4.221",),
           sku=1000, wolv=1),
    Client("C&C Win95 (Westwood Chat)",   b"WCHT", "wol", ("1.04a",),
           sku=1003, wolv=1),
    Client("C&C Red Alert Win95 2.00",    b"WCHT", "wol", ("2.00",),
           sku=1005, wolv=1),
    Client("C&C Red Alert Win95 3.03",    b"RALT", "wol", ("3.03",),
           sku=5376, wolv=2),
    Client("C&C Red Alert 2",             b"RAL2", "wol", ("1.006",),
           sku=8448, wolv=2),
    Client("C&C Tiberian Sun",            b"TSUN", "wol", ("2.03 ST-10",),
           sku=4608, wolv=2),
    Client("C&C Tiberian Sun Firestorm",  b"TSXP", "wol", ("2.03 ST-10",),
           sku=7168, wolv=2),
    Client("C&C Yuri's Revenge",          b"YURI", "wol", ("1.001",),
           sku=10496, wolv=2),
    Client("C&C Renegade",                b"RNGD", "wol", ("1.037",),
           sku=3072, wolv=2),
    Client("Nox",                         b"NOXX", "wol", ("1.02b",),
           sku=4096, wolv=2),
    Client("Nox Quest",                   b"NOXQ", "wol", ("1.02b",),
           sku=9472, wolv=2),
    Client("Dune 2000",                   b"DN2K", "wol", ("1.06",),
           sku=3584, wolv=2),
    Client("Emperor: Battle for Dune",    b"EBFD", "wol", ("1.09",),
           sku=7936, wolv=2),
]

CATALOG = OLS_CLIENTS + NLS_CLIENTS + WOL_CLIENTS


def representatives():
    """One Client per distinct (family, tag) — the distinct protocol paths."""
    seen, out = set(), []
    for c in CATALOG:
        key = (c.family, c.tag)
        if key not in seen:
            seen.add(key)
            out.append(c)
    return out


def total_versions():
    return sum(len(c.versions) for c in CATALOG)


# --- mock-login dispatch -----------------------------------------------------
def login(spec: Client, host: str, port: int, username: str, password: str,
          wol_port: Optional[int] = None) -> dict:
    """Drive ``spec``'s real login handshake. Returns a normalized result dict:
    {family, tag, ok: bool, detail: ...}. Raises only on a harness/transport
    error; a protocol-level rejection is reported via ok=False + detail."""
    import bncs_client as bc

    if spec.family == "ols":
        c = bc.BncsClient(host, port)
        try:
            ctok = 0xDEADBEEF
            stok, authres, logon_type = bc.auth_handshake(
                c, product=spec.tag, client_token=ctok)
            create_rc = bc.create_account_ols(c, username, password)
            login_rc = bc.login_ols(c, username, password, ctok, stok)
            return {"family": "ols", "tag": spec.tag, "logon_type": logon_type,
                    "auth_check": authres, "create_rc": create_rc,
                    "login_rc": login_rc, "ok": login_rc == 0}
        finally:
            c.close()

    if spec.family == "nls":
        c = bc.BncsClient(host, port)
        try:
            stok, authres, logon_type = bc.auth_handshake(
                c, product=spec.tag, client_token=0xDEADBEEF)
            create_rc = bc.create_account_w3(c, username, password)
            res = bc.login_w3(c, username, password)
            # 0x00 OK and 0x0E EMAIL are both successful-login codes.
            ok = res["m2_matches"] and res["proof_response"] in (0x00, 0x0E)
            return {"family": "nls", "tag": spec.tag, "logon_type": logon_type,
                    "auth_check": authres, "create_rc": create_rc,
                    "login_msg": res["login_msg"],
                    "proof_response": res["proof_response"],
                    "m2_matches": res["m2_matches"], "ok": ok}
        finally:
            c.close()

    if spec.family == "wol":
        import wol_client as wc
        res = wc.wol_login(host, wol_port if wol_port else port,
                           username, password, sku=spec.sku)
        res.update({"family": "wol", "tag": spec.tag})
        return res

    raise ValueError(f"unknown family {spec.family!r}")


if __name__ == "__main__":
    # Print the catalog as a quick sanity check.
    print(f"{len(CATALOG)} products, {total_versions()} versions, "
          f"{len(representatives())} distinct protocol paths\n")
    for fam in ("ols", "nls", "wol"):
        print(f"== {fam.upper()} ==")
        for c in CATALOG:
            if c.family == fam:
                extra = f" sku={c.sku} wolv={c.wolv}" if c.family == "wol" else ""
                print(f"  {c.tag.decode():4} {c.name:32} "
                      f"{len(c.versions):2d} versions{extra}")
        print()
