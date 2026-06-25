# SPDX-License-Identifier: GPL-2.0-or-later
"""Faithful Python port of Battle.net SRP-3 (WarCraft III / W3XP).

A byte-for-byte re-implementation of the parity-verified C++
`pvpgn::v3::infra::crypto::BnetSrp3` (which is itself bit-compatible with the
original server's `BnetSRP3`). Used by the differential harness to drive the
WAR3/W3XP NLS login (SID_AUTH_ACCOUNTCREATE 0x52 / ACCOUNTLOGON 0x53 / PROOF
0x54) against BOTH the original pvpgn-server and v3.

Correctness is pinned by golden vectors captured from the C++ golden test
(tests/unit/infra/crypto/bnet_srp3_golden_test.cpp) — run this module directly
to self-check:  python3 tests/diff/bnet_srp3.py
"""
import hashlib

# --- SRP-3 constants (Warcraft III) -----------------------------------------
_MODULUS_BYTES = bytes([
    0xF8, 0xFF, 0x1A, 0x8B, 0x61, 0x99, 0x18, 0x03,
    0x21, 0x86, 0xB6, 0x8C, 0xA0, 0x92, 0xB5, 0x55,
    0x7E, 0x97, 0x6C, 0x78, 0xC7, 0x32, 0x12, 0xD9,
    0x12, 0x16, 0xF6, 0x65, 0x85, 0x23, 0xC7, 0x87])
_I_BYTES = bytes([
    0xF8, 0x01, 0x8C, 0xF0, 0xA4, 0x25, 0xBA, 0x8B,
    0xEB, 0x89, 0x58, 0xB1, 0xAB, 0x6B, 0xF9, 0x0A,
    0xED, 0x97, 0x0E, 0x6C])

N = int.from_bytes(_MODULUS_BYTES, "big")   # from_bytes_legacy(.,1,true)
G = 0x2F                                     # generator
I = int.from_bytes(_I_BYTES, "big")          # from_bytes_legacy(.,1,true)


# --- BigUInt legacy block conversions (ported from big_uint.cpp) -------------
def from_bytes_legacy(b: bytes, block: int, big_endian: bool) -> int:
    """Mirror BigUInt::from_bytes_legacy."""
    work = bytearray(b)
    if not big_endian and block > 1:
        for i in range(0, len(work) - block + 1, block):
            work[i:i + block] = work[i:i + block][::-1]
    if big_endian:
        return int.from_bytes(work, "big")
    # !big_endian path packs into 4-byte segments LSB-first, segments
    # little-endian-ordered -> the whole buffer read little-endian.
    return int.from_bytes(work, "little")


def to_bytes_legacy(v: int, n: int, block: int, big_endian: bool) -> bytes:
    """Mirror BigUInt::to_bytes_legacy."""
    out = bytearray(n)
    val = v
    i = 0
    while i + 4 <= n:
        seg = val & 0xFFFFFFFF
        val >>= 32
        out[i] = (seg >> 24) & 0xFF
        out[i + 1] = (seg >> 16) & 0xFF
        out[i + 2] = (seg >> 8) & 0xFF
        out[i + 3] = seg & 0xFF
        i += 4
    if i < n:  # tail: remaining low bytes at the end, big-endian
        tail = n - i
        for bb in range(tail):
            out[n - 1 - bb] = val & 0xFF
            val >>= 8
    if not big_endian and block > 1:
        for k in range(0, n - block + 1, block):
            out[k:k + block] = out[k:k + block][::-1]
    return bytes(out)


# --- SHA-1 helpers -----------------------------------------------------------
# digest_to_bytes(sha1_le(x)) == standard SHA-1 digest (20 bytes, big-endian
# words). digest_to_bytes(sha1(x)) would be the byte-swapped words, but every
# place we need raw bytes uses sha1_le, so H() == hashlib.sha1.
def _H(data: bytes) -> bytes:
    return hashlib.sha1(data).digest()


def _ascii_upper(s: str) -> str:
    return "".join(c.upper() if ord(c) < 0x80 else c for c in s)


class BnetSrp3:
    """Client-side SRP-3 (knows the password). Salt may be pinned for
    determinism; the client private key `a` likewise."""

    def __init__(self, username: str, password: str):
        self.username = _ascii_upper(username)
        self.password = _ascii_upper(password)
        self._salt = 0
        self._a = 0

    # ----- determinism hooks -----
    def set_salt(self, s: int):
        self._salt = s

    def set_client_private_key(self, a: int):
        self._a = a

    # ----- raw salt (32 bytes, blockSize=1 big-endian) -----
    def _raw_salt(self) -> bytes:
        return to_bytes_legacy(self._salt, 32, 1, True)

    # ----- x = H(s || H(USER:PASS)) -----
    def _client_private_key(self) -> int:
        userpass = (self.username + ":" + self.password).encode("latin-1")
        userpass_hash = _H(userpass)                 # 20 bytes
        private_value = self._raw_salt() + userpass_hash  # 52 bytes
        private_hash = _H(private_value)             # 20 bytes
        return from_bytes_legacy(private_hash, 1, False)  # little-endian

    def verifier(self) -> int:
        return pow(G, self._client_private_key(), N)

    def client_session_public_key(self) -> int:
        return pow(G, self._a, N)

    # ----- u = first 32-bit word of standard SHA-1(raw_B) -----
    @staticmethod
    def _scrambler(B: int) -> int:
        raw_B = to_bytes_legacy(B, 32, 4, False)
        return int.from_bytes(_H(raw_B)[0:4], "big")

    def _client_secret(self, B: int) -> int:
        x = self._client_private_key()
        u = self._scrambler(B)
        gx = pow(G, x, N)
        base = (N + B - gx) % N
        exp = x * u + self._a
        return pow(base, exp, N)

    @staticmethod
    def _hash_secret(secret: int) -> int:
        raw = to_bytes_legacy(secret, 32, 4, False)
        odd = bytes(raw[i * 2] for i in range(16))
        even = bytes(raw[i * 2 + 1] for i in range(16))
        odd_h = _H(odd)
        even_h = _H(even)
        hashed = bytearray(40)
        for i in range(20):
            hashed[i * 2] = odd_h[i]
            hashed[i * 2 + 1] = even_h[i]
        return from_bytes_legacy(bytes(hashed), 1, False)

    def hashed_client_secret(self, B: int) -> int:
        return self._hash_secret(self._client_secret(B))

    def client_password_proof(self, A: int, B: int, K: int) -> int:
        pd = bytearray(176)
        pd[0:20] = to_bytes_legacy(I, 20, 4, False)
        pd[20:40] = _H(self.username.encode("latin-1"))
        pd[40:72] = to_bytes_legacy(self._salt, 32, 1, True)
        pd[72:104] = to_bytes_legacy(A, 32, 4, False)
        pd[104:136] = to_bytes_legacy(B, 32, 4, False)
        pd[136:176] = to_bytes_legacy(K, 40, 4, False)
        return from_bytes_legacy(_H(bytes(pd)), 1, False)

    @staticmethod
    def server_password_proof(A: int, M1: int, K: int) -> int:
        pd = bytearray(92)
        pd[0:32] = to_bytes_legacy(A, 32, 4, False)
        pd[32:52] = to_bytes_legacy(M1, 20, 4, False)
        pd[52:92] = to_bytes_legacy(K, 40, 4, False)
        return from_bytes_legacy(_H(bytes(pd)), 1, False)


# --- wire (de)serialisation helpers used by the differential client ----------
def salt_to_wire(salt_int: int) -> bytes:
    """32-byte salt as the client sends it at account creation.

    The server decodes the stored salt with ``BigInt(account_salt, 32, 4,
    false)`` (== ``from_bytes_legacy(.,4,False)``) and then derives the raw
    salt it hashes with as ``getData(buf, 32)`` (block 1, big-endian) — the
    same block-1-big-endian raw salt the client uses internally. For both
    sides to agree on the salt *integer* (and therefore the raw salt, x, and
    M1), the wire bytes must be the inverse of the server's block-4 decode,
    which is ``to_bytes_legacy(.,4,True)`` (verified: round-trips exactly).
    Using block-4 *little*-endian here silently desynchronises the salt and
    makes every proof fail with response 2."""
    return to_bytes_legacy(salt_int, 32, 4, True)


def salt_from_wire(b: bytes) -> int:
    return from_bytes_legacy(b, 4, False)


def verifier_to_wire(v: int) -> bytes:
    """Verifier + A are read by the server with from_bytes_legacy(.,1,false)
    i.e. plain little-endian, so the client serialises them little-endian."""
    return v.to_bytes(32, "little")


def pubkey_A_to_wire(a_pub: int) -> bytes:
    return a_pub.to_bytes(32, "little")


def pubkey_B_from_wire(b: bytes) -> int:
    """Server sends B via to_bytes_legacy(.,4,false) == little-endian bytes."""
    return int.from_bytes(b, "little")


def proof_to_wire(m: int) -> bytes:
    return to_bytes_legacy(m, 20, 4, False)


# --- golden self-check -------------------------------------------------------
def _selfcheck() -> int:
    user, pw = "GOLDEN", "correcthorse"
    salt = int("00112233445566778899aabbccddeeff"
               "0123456789abcdef0123456789abcdef", 16)
    a = int("a1a2a3a4a5a6a7a8a9aaabacadaeafb0"
            "b1b2b3b4b5b6b7b8b9babbbcbdbebfc0", 16)
    b = int("0102030405060708090a0b0c0d0e0f10"
            "1112131415161718191a1b1c1d1e1f20", 16)

    c = BnetSrp3(user, pw)
    c.set_salt(salt)
    c.set_client_private_key(a)

    v = c.verifier()
    A = c.client_session_public_key()
    B = (v + pow(G, b, N)) % N
    K = c.hashed_client_secret(B)
    M1 = c.client_password_proof(A, B, K)
    M2 = c.server_password_proof(A, M1, K)

    expect = {
        "v":  "29af1b9c11c85cc6dce4b6e4b649c0b4af9edccb16b2ac6c8a06d94ad38dd806",
        "A":  "ff153b11871a7d3aa77edd3f1575c2b4fbad0d550e6df0222ccb0b7eb93952b",
        "B":  "4643f16f0cdd83558b6881281a67397db8f635c80160d7b4dca1d99592b6d3fc",
        "K":  "82472ed3e115f06ba57a8ac31683a8464aa1a9c3902b3c003eff58608d9c9876"
              "524cf02f0b0e0d4b",
        "M1": "9454d79ec5c50593b27ad845847a0b9b4551b2fb",
        "M2": "ea80de605bc99538ae64f0ec72299acd6e71b53",
    }
    got = {"v": v, "A": A, "B": B, "K": K, "M1": M1, "M2": M2}
    ok = True
    for name, hexv in expect.items():
        if got[name] != int(hexv, 16):
            ok = False
            print(f"FAIL {name}: got {got[name]:x}\n         exp {hexv}")
    if ok:
        print("PASS: Python SRP-3 reproduces all C++ golden vectors")
        return 0
    return 1


if __name__ == "__main__":
    import sys
    sys.exit(_selfcheck())
