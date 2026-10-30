#!/usr/bin/env python3
"""dbg.py - send one debug command to the runtime's TCP server (JSON over newline).

Needs a build configured with -DPSX_DEBUG_TOOLS=ON (on by default for
Debug/RelWithDebInfo; off for Release). See psxrecomp/docs/TCP_COMMANDS.md.

Usage: python3 tools/dbg.py [--port 4797] '<json>'  OR  python3 tools/dbg.py cmd k=v k=v
Examples:
  python3 tools/dbg.py ping
  python3 tools/dbg.py screenshot path=/tmp/r4.png
  python3 tools/dbg.py read_ram addr=0x80010000 len=64
  python3 tools/dbg.py dirty_ram_stats
"""
import json
import socket
import os
import struct
import sys

DEFAULT_PORT = 4797  # game.toml [runtime] debug_port (SLUS-00797)

# addr/hex-like fields are parsed server-side from strings; everything else
# numeric is sent as an int.
STRING_FIELDS = {"addr", "hex", "lo", "hi", "target", "path"}


def build_payload(args):
    if args[0].lstrip().startswith("{"):
        return args[0]
    obj = {"cmd": args[0]}
    for kv in args[1:]:
        if "=" not in kv:
            continue
        k, v = kv.split("=", 1)
        if k in STRING_FIELDS:
            obj[k] = v
            continue
        try:
            obj[k] = int(v, 0)
        except ValueError:
            obj[k] = v
    return json.dumps(obj)


_source_port = 20000 + os.getpid() % 20000


def connect(port, timeout):
    try:
        return socket.create_connection(("127.0.0.1", port), timeout=timeout)
    except OSError as error:
        if getattr(error, "winerror", None) != 10048:
            raise
    # Only retry connect, before any request bytes: never replay a mutation.
    # Other programs can exhaust Windows' dynamic range with TIME_WAIT sockets.
    # Bind an unused diagnostic source port without changing global TCP policy.
    global _source_port
    for _ in range(128):
        _source_port = 20000 + (_source_port - 20000 + 1) % 20000
        candidate = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            candidate.settimeout(timeout)
            candidate.bind(("127.0.0.1", _source_port))
            candidate.connect(("127.0.0.1", port))
            # Response is read completely before close; avoid adding another
            # TIME_WAIT entry to an already exhausted host.
            candidate.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("HH", 1, 0))
            return candidate
        except OSError as error:
            candidate.close()
            if getattr(error, "winerror", None) != 10048:
                raise
    raise OSError("No free diagnostic TCP source port after 128 attempts")


def send(payload, port=DEFAULT_PORT, timeout=20.0):
    """Send one request line and return the decoded response text."""
    with connect(port, timeout) as s:
        s.sendall(payload.encode() + b"\n")
        buf = b""
        s.settimeout(timeout)
        while not buf.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            buf += chunk
    return buf.decode(errors="replace").strip()


def cmd(obj, port=DEFAULT_PORT, timeout=20.0):
    """Send a dict command and return the parsed JSON response ({} on garbage)."""
    out = send(json.dumps(obj), port=port, timeout=timeout)
    for line in reversed(out.splitlines()):
        try:
            return json.loads(line)
        except ValueError:
            continue
    return {}


def main():
    args = sys.argv[1:]
    port = DEFAULT_PORT
    if args and args[0] == "--port":
        port = int(args[1])
        args = args[2:]
    if not args:
        print(__doc__)
        return 2
    try:
        out = send(build_payload(args), port=port)
    except OSError as exc:
        print(f"cannot reach 127.0.0.1:{port}: {exc}", file=sys.stderr)
        return 1
    try:
        print(json.dumps(json.loads(out), indent=2))
    except ValueError:
        print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
