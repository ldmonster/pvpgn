#!/usr/bin/env python3
"""Concurrency hardening check for the D2CS game-lobby routing (waves 223-226).

The create/join-game routing introduced shared mutable state across sessions
(the thread-safe D2gsRegistry) plus cross-thread socket writes (a D2GS-link
session answering a client session on another strand) and weak_ptr lifetime
correlation. The d2cs server runs rt.run(hardware_concurrency()), so those paths
execute on multiple threads.

This drives many concurrent clients (each: login -> create char -> select ->
CREATEGAMEREQ) through a single game-hosting mock D2GS, against a ThreadSanitizer
build of pvpgn_v3_d2cs, and fails if TSan reports any data race.

Run against a TSan build (cmake -B build/v3-tsan -DPVPGN_V3_SANITIZERS=thread):
    python3 tests/diff/tsan_d2cs_creategame_stress.py
TSan needs ASLR disabled, so the server is launched under `setarch <arch> -R`
(otherwise TSan aborts with "unexpected memory mapping").
"""
import argparse
import os
import signal
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import d2cs_client as dc  # noqa: E402
import d2gs_client as dg  # noqa: E402


def creategamereq_body(seqno, name):
    return (struct.pack("<HIBBB", seqno, 0x100002, 0, 0, 4)
            + name.encode() + b"\x00" + b"\x00" + b"\x00")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-tsan/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--port", type=int, default=9210)
    ap.add_argument("--clients", type=int, default=12)
    a = ap.parse_args()

    if not os.path.isfile(a.bin):
        print(f"SKIP: TSan binary not found at {a.bin} "
              f"(build with -DPVPGN_V3_SANITIZERS=thread)")
        return 0

    log_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "..", "..", "build", "v3-tsan", "tsan_d2cs_stress.log")
    arch = subprocess.check_output(["uname", "-m"]).decode().strip()
    env = dict(os.environ, TSAN_OPTIONS="halt_on_error=0 exitcode=0")
    logf = open(log_path, "w")
    proc = subprocess.Popen(
        ["setarch", arch, "-R", a.bin, "--port", str(a.port), "--listen", "127.0.0.1"],
        stdout=logf, stderr=subprocess.STDOUT, env=env)
    time.sleep(2.0)

    results = []
    try:
        gs = dg.D2gsClient("127.0.0.1", a.port)
        hs = gs.handshake()
        assert hs and hs["reply"] == 0, f"d2gs handshake failed: {hs}"
        gs.send_setgsinfo(maxgame=100)
        time.sleep(0.3)
        stop = threading.Event()

        def serve_loop():
            while not stop.is_set():
                try:
                    gs.serve(max_packets=4, timeout=0.5)
                except Exception:  # noqa: BLE001
                    pass
        st = threading.Thread(target=serve_loop, daemon=True)
        st.start()

        def client(i):
            try:
                c = dc.D2csClient("127.0.0.1", a.port)
                acct = f"u{i}"
                c.login(acct, sessionnum=1,
                        secret_hash_raw=dc.d2cs_token(acct, 1, 7), seqno=7)
                c.create_char("Hero", char_class=4, status=0x20)
                c.char_login("Hero")
                c.send(0x03, creategamereq_body(i, f"Game{i}"))
                results.append(c.recv_type(0x03) is not None)
                c.close()
            except Exception as e:  # noqa: BLE001
                results.append(f"err:{e}")

        threads = [threading.Thread(target=client, args=(i,))
                   for i in range(a.clients)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=15)
        stop.set()
        time.sleep(0.5)
        gs.close()
    finally:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            proc.kill()
        logf.close()

    served = sum(1 for r in results if r is True)
    data = open(log_path).read()
    warns = data.count("WARNING: ThreadSanitizer")
    print(f"clients served CREATEGAMEREPLY: {served}/{a.clients}")
    print(f"ThreadSanitizer warnings: {warns}")
    if warns:
        idx = data.find("WARNING: ThreadSanitizer")
        print(data[idx:idx + 2000])
    ok = (served == a.clients and warns == 0)
    print("OK: concurrent create-game routing is race-free under TSan"
          if ok else "FAIL")
    return 0 if ok else 1


sys.exit(main())
