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


def send(payload, port=DEFAULT_PORT, timeout=20.0):
    """Send one request line and return the decoded response text."""
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
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
