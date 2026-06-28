# SPDX-License-Identifier: GPL-2.0-or-later
"""A protocol-faithful Battle.net (BNCS) mock client for differential testing.

The same client drives BOTH the original pvpgn-server (the oracle) and the v3
rewrite, so their responses can be diffed. Implements the OLS login flow
(broken-SHA-1 double-hash) and the NLS/SRP-3 flow (WAR3/W3XP), plus enough chat
to exercise post-login behaviour.
"""
import socket
import struct


class ProtocolError(Exception):
    """Raised when the server deviates from the expected BNCS wire sequence."""


# ---- SID constants ----------------------------------------------------------
SID_NULL = 0x00
SID_STARTADVEX3 = 0x1C
SID_CLIENTID = 0x05
SID_LOGONRESPONSE = 0x29
SID_GETADVLISTEX = 0x09
SID_ENTERCHAT = 0x0A
SID_JOINCHANNEL = 0x0C
SID_CHATCOMMAND = 0x0E
SID_CHATEVENT = 0x0F
SID_PING = 0x25
SID_LOGONRESPONSE2 = 0x3A
SID_CREATEACCOUNT2 = 0x3D
SID_CREATE_ACCT1 = 0x2A   # CLIENT_CREATEACCTREQ1 (reply result: 1 = OK, 0 = NO)
SID_AUTH_INFO = 0x50
SID_AUTH_CHECK = 0x51
SID_AUTH_ACCOUNTCREATE = 0x52
SID_AUTH_ACCOUNTLOGON = 0x53
SID_AUTH_ACCOUNTLOGONPROOF = 0x54
SID_AUTH_ACCOUNTCHANGE = 0x55
SID_AUTH_ACCOUNTCHANGEPROOF = 0x56
SID_FRIENDSLIST = 0x65
SID_FRIENDINFO = 0x66
SID_READUSERDATA = 0x26
SID_WRITEUSERDATA = 0x27
SID_PROFILE = 0x35
SID_CHANGEPASSWORD = 0x31

# Chat event ids (canonical BNCS).
EID_SHOWUSER = 0x01
EID_JOIN = 0x02
EID_CHANNEL = 0x07
EID_INFO = 0x12
EID_ERROR = 0x13


# ---- broken-SHA-1 (Blizzard "xsha1") ----------------------------------------
def _rotl32(v, n):
    v &= 0xFFFFFFFF
    n &= 31
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def blizzard_hash(data: bytes):
    """The legacy broken-SHA-1: ROTL32(1, mix) message schedule, no padding.
    Returns 5 host-order digest words."""
    digest = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0]
    pos, size = 0, len(data)
    while size > 0:
        inc = 64 if size > 64 else size
        block = data[pos:pos + inc]
        tmp = [0] * 80
        for i in range(16):
            word = 0
            for b in range(4):
                idx = i * 4 + b
                if idx < inc:
                    word |= block[idx] << (8 * b)
            tmp[i] = word & 0xFFFFFFFF
        for i in range(64):
            mix = (tmp[i] ^ tmp[i + 8] ^ tmp[i + 2] ^ tmp[i + 13]) & 0xFFFFFFFF
            tmp[i + 16] = _rotl32(1, mix)
        a, bb, c, d, e = digest
        g = 0
        for i in range(20):
            g = (tmp[i] + _rotl32(a, 5) + e + ((bb & c) | (~bb & 0xFFFFFFFF & d)) + 0x5A827999) & 0xFFFFFFFF
            e, d, c, bb, a = d, c, _rotl32(bb, 30), a, g
        for i in range(20, 40):
            g = ((d ^ c ^ bb) + e + _rotl32(g, 5) + tmp[i] + 0x6ED9EBA1) & 0xFFFFFFFF
            e, d, c, bb, a = d, c, _rotl32(bb, 30), a, g
        for i in range(40, 60):
            g = (tmp[i] + _rotl32(g, 5) + e + ((c & bb) | (d & c) | (d & bb)) - 0x70E44324) & 0xFFFFFFFF
            e, d, c, bb, a = d, c, _rotl32(bb, 30), a, g
        for i in range(60, 80):
            g = ((d ^ c ^ bb) + e + _rotl32(g, 5) + tmp[i] - 0x359D3E2A) & 0xFFFFFFFF
            e, d, c, bb, a = d, c, _rotl32(bb, 30), a, g
        digest[0] = (digest[0] + g) & 0xFFFFFFFF
        digest[1] = (digest[1] + bb) & 0xFFFFFFFF
        digest[2] = (digest[2] + c) & 0xFFFFFFFF
        digest[3] = (digest[3] + d) & 0xFFFFFFFF
        digest[4] = (digest[4] + e) & 0xFFFFFFFF
        pos += inc
        size -= inc
    return digest


def hash_password(password: str):
    """hash1 = xsha1(lowercase password bytes) -> 5 words."""
    return blizzard_hash(password.lower().encode("latin-1"))


def double_hash(hash1_words, client_token, server_token):
    """hash2 = xsha1(client_token ‖ server_token ‖ hash1)."""
    buf = struct.pack("<7I", client_token, server_token, *hash1_words)
    return blizzard_hash(buf)


def cstring(s: str) -> bytes:
    return s.encode("latin-1") + b"\x00"


# ---- the client -------------------------------------------------------------
class BncsClient:
    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b""
        # Battle.net protocol-select byte.
        self.sock.sendall(b"\x01")

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def send(self, sid: int, body: bytes = b""):
        pkt = struct.pack("<BBH", 0xFF, sid, len(body) + 4) + body
        self.sock.sendall(pkt)

    def _fill(self, n: int):
        while len(self.buf) < n:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("server closed the connection")
            self.buf += chunk

    def recv(self):
        """Return (sid, body) of the next BNCS packet, or None on close/timeout."""
        try:
            self._fill(4)
        except (socket.timeout, ConnectionError):
            return None
        marker, sid, length = struct.unpack_from("<BBH", self.buf, 0)
        if marker != 0xFF or length < 4:
            raise ValueError(f"bad BNCS header marker={marker:#x} len={length}")
        try:
            self._fill(length)
        except (socket.timeout, ConnectionError):
            return None
        body = self.buf[4:length]
        self.buf = self.buf[length:]
        return sid, body

    def recv_sid(self, want_sid: int, max_packets: int = 20):
        """Drain packets until one with want_sid arrives; return its body."""
        for _ in range(max_packets):
            r = self.recv()
            if r is None:
                return None
            sid, body = r
            if sid == want_sid:
                return body
        return None

    # ---- handshake builders -------------------------------------------------
    def send_auth_info(self, product=b"SEXP"):
        # SID_AUTH_INFO: protocol id, platform 'IX86', product, verbyte, lang,
        # local ip, tz bias, locale id, lang id, country abbrev, country.
        body = struct.pack("<I", 0)            # protocol id
        body += b"68XI"                        # platform 'IX86' (LE)
        body += product[::-1]                  # product tag (LE)
        body += struct.pack("<I", 0xD3)        # version byte
        body += struct.pack("<I", 0)           # product language
        body += struct.pack("<I", 0)           # local ip
        body += struct.pack("<i", 0)           # tz bias
        body += struct.pack("<I", 0)           # locale id
        body += struct.pack("<I", 0)           # language id
        body += cstring("USA")                 # country abbreviation
        body += cstring("United States")       # country
        self.send(SID_AUTH_INFO, body)


def first_result_u32(body):
    if body is None or len(body) < 4:
        return None
    return struct.unpack_from("<I", body, 0)[0]


# ---- higher-level flows (shared by the differential scenarios) ---------------
def _drain_until(client, want_sid, max_packets=30):
    """Drain packets, auto-echoing server PINGs, until want_sid arrives."""
    for _ in range(max_packets):
        r = client.recv()
        if r is None:
            return None
        sid, body = r
        if sid == SID_PING:
            client.send(SID_PING, body[:4])   # echo the latency cookie
            continue
        if sid == want_sid:
            return body
    return None


def auth_handshake(client, product=b"SEXP", client_token=0xDEADBEEF):
    """Faithful OLS AUTH_INFO handshake — the exact sequence a real Blizzard
    client follows, used to drive BOTH servers identically.

        client -> SID_AUTH_INFO (0x50)
        server -> SID_AUTH_INFO (0x50)  SEED: logon_type, server_token, ...
        client -> SID_AUTH_CHECK (0x51) version + CD-key proof
        server -> SID_AUTH_CHECK (0x51) result

    A real client BLOCKS for the server seed and uses its server_token in the
    password double-hash; there is no "skip the seed" shortcut. If the server
    does not send the 0x50 seed first this raises, so the mock doubles as a
    regression guard for the seed.

    Returns (server_token, auth_check_result, logon_type).
    """
    client.send_auth_info(product=product)
    seed = _drain_until(client, SID_AUTH_INFO)
    if seed is None or len(seed) < 8:
        raise ProtocolError(
            "server did not send the SID_AUTH_INFO (0x50) seed before AUTH_CHECK")
    logon_type   = struct.unpack_from("<I", seed, 0)[0]
    server_token = struct.unpack_from("<I", seed, 4)[0]
    # Respond with SID_AUTH_CHECK (version byte, checksum, CD-key proof). The
    # values are dummies — version-check is not enforced under the test config.
    chk = struct.pack("<IIIII", client_token, 0xD3, 0, 1, 0)
    chk += struct.pack("<IIII", 0, 0, 0, 0) + struct.pack("<5I", 0, 0, 0, 0, 0)
    chk += cstring("Game.exe 01/01/01 00:00:00 1") + cstring("owner")
    client.send(SID_AUTH_CHECK, chk)
    res = _drain_until(client, SID_AUTH_CHECK)
    return server_token, (first_result_u32(res) if res else None), logon_type


def create_account_ols(client, username, password):
    """Create an account via SID_CREATE_ACCT1 (hash1 = xsha1(password))."""
    h1 = hash_password(password)
    body = struct.pack("<5I", *h1) + cstring(username)
    client.send(SID_CREATE_ACCT1, body)
    res = _drain_until(client, SID_CREATE_ACCT1)
    return first_result_u32(res)


SID_LOGONREALMEX = 0x3E  # CLIENT_REALMJOINREQ_109 / SERVER_REALMJOINREPLY_109


def realm_join(client, realmname, seqno=1):
    """SID_LOGONREALMEX (0x3e): join a D2 realm after a BNCS login. bnetd issues
    the session credentials the client then presents to d2cs. Request body:
    seqno(4) + seqnohash[5](20) + realmname. Reply (BNCS header already stripped
    by recv) per t_server_realmjoinreply_109. Returns a dict or None."""
    body = struct.pack("<I", seqno) + b"\x00" * 20 + cstring(realmname)
    client.send(SID_LOGONREALMEX, body)
    rep = _drain_until(client, SID_LOGONREALMEX)
    if rep is None or len(rep) < 72:
        return None
    # offsets within the reply body:
    #  seqno 0 | u1 4 | bncs_addr1 8 | sessionnum 12 | addr 16(BE) | port 20(BE)
    #  u3 22 | sessionkey 24 | u5 28 | u6 32 | clienttag 36 | versionid 40
    #  bncs_addr2 44 | u7 48 | secret_hash[5] 52..72 | account name 72+
    import socket as _s
    sessionnum  = struct.unpack_from("<I", rep, 12)[0]
    addr_be     = struct.unpack_from(">I", rep, 16)[0]
    port_be     = struct.unpack_from(">H", rep, 20)[0]
    sessionkey  = struct.unpack_from("<I", rep, 24)[0]
    secret_hash = rep[52:72]
    return {
        "sessionnum": sessionnum,
        "sessionkey": sessionkey,
        "d2cs_addr": _s.inet_ntoa(struct.pack(">I", addr_be)),
        "d2cs_port": port_be,
        "secret_hash": secret_hash,
    }


def login_ols(client, username, password, client_token, server_token):
    """SID_LOGONRESPONSE2 with hash2 = xsha1(client_token ‖ server_token ‖ hash1)."""
    h1 = hash_password(password)
    h2 = double_hash(h1, client_token, server_token)
    body = struct.pack("<2I", client_token, server_token)
    body += struct.pack("<5I", *h2) + cstring(username)
    client.send(SID_LOGONRESPONSE2, body)
    res = _drain_until(client, SID_LOGONRESPONSE2)
    return first_result_u32(res)


# ---- WarCraft III SRP-3 (NLS) flow ------------------------------------------
# A faithful WAR3/W3XP client: SID_AUTH_ACCOUNTCREATE (0x52) registers a
# salt + verifier; SID_AUTH_ACCOUNTLOGON (0x53) / ...PROOF (0x54) do the SRP-3
# challenge/response. The crypto is bnet_srp3.py (golden-verified bit-for-bit
# against the C++ implementation the original server uses).
import bnet_srp3 as _srp  # noqa: E402

# A fixed salt keeps the handshake deterministic across both servers.
_W3_SALT = int(
    "0011223344556677" "8899aabbccddeeff" "0011223344556677" "8899aabbccddeeff", 16)


def create_account_w3(client, username, password, salt=_W3_SALT):
    """SID_AUTH_ACCOUNTCREATE (0x52): register salt + SRP-3 verifier.
    Returns the u32 result (0 == OK)."""
    c = _srp.BnetSrp3(username, password)
    c.set_salt(salt)
    body = _srp.salt_to_wire(salt)
    body += _srp.verifier_to_wire(c.verifier())
    body += cstring(username)
    client.send(SID_AUTH_ACCOUNTLOGON - 1, body)  # 0x52 = ACCOUNTCREATE
    res = _drain_until(client, SID_AUTH_ACCOUNTLOGON - 1)
    return first_result_u32(res)


def login_w3(client, username, password, salt=_W3_SALT, client_priv=None):
    """SID_AUTH_ACCOUNTLOGON (0x53) + PROOF (0x54). Returns a dict:
       {login_msg, proof_response, m2_matches}.

    m2_matches is True iff the server's M2 equals the value the client derives
    independently — i.e. mutual authentication succeeded."""
    c = _srp.BnetSrp3(username, password)
    c.set_salt(salt)
    # Deterministic client private key (so two servers see identical A).
    c.set_client_private_key(
        client_priv if client_priv is not None else
        int("a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0", 16))
    A = c.client_session_public_key()

    # 0x53: send A + username, receive {message, salt, B}.
    client.send(SID_AUTH_ACCOUNTLOGON, _srp.pubkey_A_to_wire(A) + cstring(username))
    reply = _drain_until(client, SID_AUTH_ACCOUNTLOGON)
    if reply is None or len(reply) < 4:
        return {"login_msg": None, "proof_response": None, "m2_matches": False}
    login_msg = struct.unpack_from("<I", reply, 0)[0]
    if login_msg != 0 or len(reply) < 4 + 32 + 32:
        return {"login_msg": login_msg, "proof_response": None, "m2_matches": False}
    B = _srp.pubkey_B_from_wire(reply[4 + 32:4 + 32 + 32])

    K = c.hashed_client_secret(B)
    M1 = c.client_password_proof(A, B, K)
    M2_expected = c.server_password_proof(A, M1, K)

    # 0x54: send M1, receive {response, M2}.
    client.send(SID_AUTH_ACCOUNTLOGONPROOF, _srp.proof_to_wire(M1))
    preply = _drain_until(client, SID_AUTH_ACCOUNTLOGONPROOF)
    if preply is None or len(preply) < 4:
        return {"login_msg": login_msg, "proof_response": None, "m2_matches": False}
    response = struct.unpack_from("<I", preply, 0)[0]
    m2_matches = False
    if len(preply) >= 4 + 20:
        m2_wire = preply[4:4 + 20]
        m2_matches = (m2_wire == _srp.proof_to_wire(M2_expected))
    return {"login_msg": login_msg, "proof_response": response,
            "m2_matches": m2_matches}


def passchange_w3(client, username, old_password, new_password,
                  salt=_W3_SALT, new_salt=None, client_priv=None):
    """SID_AUTH_ACCOUNTCHANGE (0x55) + ...PROOF (0x56): change a WAR3 account's
    password. Step A is the same SRP-3 challenge as login (prove the OLD
    password); step B sends M1 + the NEW salt + NEW verifier. Returns:
       {change_msg, proof_response, m2_matches}.

    m2_matches confirms mutual auth on the change proof (the server returned the
    M2 the client independently derives from the OLD credentials)."""
    if new_salt is None:
        new_salt = salt
    # Challenge against the OLD password.
    c = _srp.BnetSrp3(username, old_password)
    c.set_salt(salt)
    c.set_client_private_key(
        client_priv if client_priv is not None else
        int("b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0", 16))
    A = c.client_session_public_key()

    # 0x55: send A + username, receive {message, salt, B}.
    client.send(SID_AUTH_ACCOUNTCHANGE, _srp.pubkey_A_to_wire(A) + cstring(username))
    reply = _drain_until(client, SID_AUTH_ACCOUNTCHANGE)
    if reply is None or len(reply) < 4:
        return {"change_msg": None, "proof_response": None, "m2_matches": False}
    change_msg = struct.unpack_from("<I", reply, 0)[0]
    if change_msg != 0 or len(reply) < 4 + 32 + 32:
        return {"change_msg": change_msg, "proof_response": None,
                "m2_matches": False}
    B = _srp.pubkey_B_from_wire(reply[4 + 32:4 + 32 + 32])

    K = c.hashed_client_secret(B)
    M1 = c.client_password_proof(A, B, K)
    M2_expected = c.server_password_proof(A, M1, K)

    # Derive the NEW salt + verifier from the new password.
    nc = _srp.BnetSrp3(username, new_password)
    nc.set_salt(new_salt)
    new_verifier = nc.verifier()

    # 0x56: send M1 + new salt + new verifier, receive {response, M2}.
    body = (_srp.proof_to_wire(M1)
            + _srp.salt_to_wire(new_salt)
            + _srp.verifier_to_wire(new_verifier))
    client.send(SID_AUTH_ACCOUNTCHANGEPROOF, body)
    preply = _drain_until(client, SID_AUTH_ACCOUNTCHANGEPROOF)
    if preply is None or len(preply) < 4:
        return {"change_msg": change_msg, "proof_response": None,
                "m2_matches": False}
    response = struct.unpack_from("<I", preply, 0)[0]
    m2_matches = False
    if len(preply) >= 4 + 20:
        m2_matches = (preply[4:4 + 20] == _srp.proof_to_wire(M2_expected))
    return {"change_msg": change_msg, "proof_response": response,
            "m2_matches": m2_matches}


# ---- post-login chat / channel flows ----------------------------------------
def parse_chat_event(body):
    """SID_CHATEVENT: event_id, flags, ping, ip, acct, reg, username\0, text\0."""
    if body is None or len(body) < 24:
        return None
    event_id = struct.unpack_from("<I", body, 0)[0]
    rest = body[24:]
    parts = rest.split(b"\x00")
    username = parts[0].decode("latin-1", "replace") if len(parts) > 0 else ""
    text = parts[1].decode("latin-1", "replace") if len(parts) > 1 else ""
    return event_id, username, text


def enter_chat(client, username):
    """SID_ENTERCHAT -> returns the unique name the server assigns."""
    client.send(SID_ENTERCHAT, cstring(username) + cstring(""))
    body = _drain_until(client, SID_ENTERCHAT)
    if body is None:
        return None
    return body.split(b"\x00")[0].decode("latin-1", "replace")


def join_channel(client, channel, flags=0x00, collect=8, settle=0.4):
    """SID_JOINCHANNEL -> collect the resulting CHATEVENT stream.
    Returns a list of (event_id, username, text) the server emitted."""
    import time
    client.send(SID_JOINCHANNEL, struct.pack("<I", flags) + cstring(channel))
    time.sleep(settle)
    events = []
    for _ in range(collect):
        r = client.recv()
        if r is None:
            break
        sid, body = r
        if sid == SID_PING:
            client.send(SID_PING, body[:4])
            continue
        if sid == SID_CHATEVENT:
            ev = parse_chat_event(body)
            if ev:
                events.append(ev)
    return events


def chat_command(client, text, collect=8, settle=0.4):
    """SID_CHATCOMMAND -> collect the resulting CHATEVENT stream."""
    import time
    client.send(SID_CHATCOMMAND, cstring(text))
    time.sleep(settle)
    events = []
    for _ in range(collect):
        r = client.recv()
        if r is None:
            break
        sid, body = r
        if sid == SID_PING:
            client.send(SID_PING, body[:4])
            continue
        if sid == SID_CHATEVENT:
            ev = parse_chat_event(body)
            if ev:
                events.append(ev)
    return events


def request_friends_list(client, settle=0.4):
    """SID_FRIENDSLIST (0x65): request the friends list and parse the reply.
    Wire: count(u8) then per friend: name\\0, status(u8), location(u8),
    client_tag(u32 LE), location_name\\0. Returns a list of dicts sorted by name:
    [{name, status, location, client_tag}]."""
    import struct
    import time
    client.send(SID_FRIENDSLIST, b"")
    time.sleep(settle)
    body = _drain_until(client, SID_FRIENDSLIST)
    out = []
    if body is None or len(body) < 1:
        return out
    count = body[0]
    pos = 1
    for _ in range(count):
        nul = body.find(b"\x00", pos)
        if nul < 0:
            break
        name = body[pos:nul].decode("latin-1", "replace")
        pos = nul + 1
        if pos + 1 + 1 + 4 > len(body):
            break
        status = body[pos]
        location = body[pos + 1]
        client_tag = struct.unpack_from("<I", body, pos + 2)[0]
        pos += 1 + 1 + 4  # status, location, client_tag
        nul2 = body.find(b"\x00", pos)
        if nul2 < 0:
            break
        pos = nul2 + 1
        out.append({"name": name, "status": status, "location": location,
                    "client_tag": client_tag})
    return sorted(out, key=lambda f: f["name"].lower())


def drain_chat(client, settle=0.5):
    """Collect any pending SID_CHATEVENTs on a client's socket (answers PINGs).
    Returns a list of (event_id, username, text). Used to observe what another
    client's actions delivered to this client."""
    import time
    time.sleep(settle)
    old = client.sock.gettimeout()
    client.sock.settimeout(0.4)
    events = []
    try:
        while True:
            r = client.recv()
            if r is None:
                break
            sid, body = r
            if sid == SID_PING:
                client.send(SID_PING, body[:4])
                continue
            if sid == SID_CHATEVENT:
                ev = parse_chat_event(body)
                if ev:
                    events.append(ev)
    finally:
        client.sock.settimeout(old)
    return events


SID_CLANCREATEREQ = 0x70  # CLIENT_CLAN_CREATEREQ / SERVER_CLAN_CREATEREPLY


def clan_create_req(client, clan_tag=0x54414721, cookie=0x1234, settle=0.4):
    """SID_CLANCREATEREQ (0x70): request body count(u32)+clantag(u32). Reply:
    count(u32) + check_result(u8) + friend_count(u8) + friend names. Returns
    {cookie, check_result, friends:[...]} or None."""
    import struct, time
    client.send(SID_CLANCREATEREQ, struct.pack("<II", cookie, clan_tag))
    time.sleep(settle)
    body = _drain_until(client, SID_CLANCREATEREQ)
    if body is None or len(body) < 6:
        return None
    rc = struct.unpack_from("<I", body, 0)[0]
    check_result = body[4]
    friend_count = body[5]
    friends = []
    pos = 6
    for _ in range(friend_count):
        nul = body.find(b"\x00", pos)
        if nul < 0:
            break
        friends.append(body[pos:nul].decode("latin-1", "replace"))
        pos = nul + 1
    return {"cookie": rc, "check_result": check_result, "friends": friends}


def friends_add(client, name):
    """/friends add <name> via SID_CHATCOMMAND (drains the resulting ack)."""
    chat_command(client, f"/friends add {name}")


def friends_remove(client, name):
    """/friends remove <name> via SID_CHATCOMMAND (drains the resulting ack)."""
    chat_command(client, f"/friends remove {name}")


def advertise_game(client, name, info="map\r\nx", status=0x10, gametype=0x02,
                   option=0x01, password=""):
    """SID_STARTADVEX3 (0x1C): host/advertise a game so GETADVLISTEX can find it.
    status 0x10 satisfies the original's INIT_VALID mask (public, open, not full).
    Returns the u32 ack (0 == OK)."""
    body = struct.pack("<HHIHHII", status, 0, 0, gametype, option, 0, 0)
    body += cstring(name) + cstring(password) + cstring(info)
    client.send(SID_STARTADVEX3, body)
    res = _drain_until(client, SID_STARTADVEX3)
    return first_result_u32(res)


def game_list(client, gametype=0x0000, name="", settle=0.4):
    """SID_GETADVLISTEX (0x09): list advertised games. gametype 0 == ALL.
    Returns a sorted list of advertised game names."""
    import time
    # The original reads TWO trailing cstrings: game name + password (it aborts
    # without replying if the password field is missing). v3 reads only the name
    # and ignores the trailing password, so sending both satisfies both servers.
    body = struct.pack("<HHIII", gametype, 0, 0, 0, 0) + cstring(name) + cstring("")
    client.send(SID_GETADVLISTEX, body)
    time.sleep(settle)
    reply = _drain_until(client, SID_GETADVLISTEX)
    out = []
    if reply is None or len(reply) < 8:
        return out
    count = struct.unpack_from("<I", reply, 0)[0]
    # sstatus at [4:8]; entries follow.
    pos = 8
    for i in range(count):
        if i > 0:
            pos += 4  # 4-byte spacer before entries 2..n
        pos += 28     # fixed entry header (gametype..unknown6)
        if pos > len(reply):
            break
        nul = reply.find(b"\x00", pos)            # game_name
        if nul < 0:
            break
        gname = reply[pos:nul].decode("latin-1", "replace")
        pos = nul + 1
        nul = reply.find(b"\x00", pos)            # password
        if nul < 0:
            break
        pos = nul + 1
        nul = reply.find(b"\x00", pos)            # info
        if nul < 0:
            break
        pos = nul + 1
        out.append(gname)
    return sorted(out, key=str.lower)


def full_login(host, port, username, password, product=b"SEXP"):
    """Connect + OLS create + login + enter chat. Returns (client, unique_name)."""
    c = BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = auth_handshake(c, product=product, client_token=ctok)
    create_account_ols(c, username, password)
    rc = login_ols(c, username, password, ctok, stok)
    if rc != 0:
        c.close()
        raise RuntimeError(f"login failed rc={rc}")
    uniq = enter_chat(c, username)
    return c, uniq


def write_userdata(client, account, kv):
    """SID_WRITEUSERDATA (0x27): name_count, key_count, name, keys[], values[].
    kv is an ordered dict of profile keys -> values."""
    keys = list(kv.keys())
    body = struct.pack("<II", 1, len(keys))
    body += cstring(account)
    for k in keys:
        body += cstring(k)
    for k in keys:
        body += cstring(kv[k])
    client.send(SID_WRITEUSERDATA, body)


def change_password_ols(client, account, old_password, new_password,
                        ticks=0, sessionkey=0, max_packets=30):
    """SID_CHANGEPASSWORD (0x31): old password as a session-hash
    (xsha1(ticks||sessionkey||hash1(old))), new password as hash1. Returns the
    ACK message (1 = success, 0 = fail) or None."""
    old_h1 = hash_password(old_password)
    old_h2 = double_hash(old_h1, ticks, sessionkey)
    new_h1 = hash_password(new_password)
    body = struct.pack("<II", ticks, sessionkey)
    body += struct.pack("<5I", *old_h2)
    body += struct.pack("<5I", *new_h1)
    body += cstring(account)
    client.send(SID_CHANGEPASSWORD, body)
    rbody = client.recv_sid(SID_CHANGEPASSWORD, max_packets)
    if rbody is None or len(rbody) < 4:
        return None
    return struct.unpack_from("<I", rbody, 0)[0]


def request_profile(client, player_name, cookie=1, max_packets=30):
    """SID_PROFILE (0x35): request a user's profile; return
    {fail, description, location, clan_tag} or None (no reply)."""
    body = struct.pack("<I", cookie) + cstring(player_name)
    client.send(SID_PROFILE, body)
    rbody = client.recv_sid(SID_PROFILE, max_packets)
    if rbody is None or len(rbody) < 5:
        return None
    _cookie, fail = struct.unpack_from("<IB", rbody, 0)
    if fail != 0:
        return {"fail": fail, "description": "", "location": "", "clan_tag": 0}
    off = 5
    end = rbody.find(b"\x00", off)
    desc = rbody[off:end].decode("latin-1", "replace")
    off = end + 1
    end = rbody.find(b"\x00", off)
    loc = rbody[off:end].decode("latin-1", "replace")
    off = end + 1
    clan_tag = struct.unpack_from("<I", rbody, off)[0] if off + 4 <= len(rbody) else 0
    return {"fail": fail, "description": desc, "location": loc,
            "clan_tag": clan_tag}


def read_userdata(client, account, keys, request_id=1, max_packets=30):
    """SID_READUSERDATA (0x26): request name x keys for one account; return the
    list of values (parallel to keys), or None."""
    body = struct.pack("<III", 1, len(keys), request_id)
    body += cstring(account)
    for k in keys:
        body += cstring(k)
    client.send(SID_READUSERDATA, body)
    rbody = client.recv_sid(SID_READUSERDATA, max_packets)
    if rbody is None or len(rbody) < 12:
        return None
    name_count, key_count, _rid = struct.unpack_from("<III", rbody, 0)
    off = 12
    values = []
    for _ in range(name_count * key_count):
        end = rbody.find(b"\x00", off)
        if end < 0:
            break
        values.append(rbody[off:end].decode("latin-1", "replace"))
        off = end + 1
    return values
