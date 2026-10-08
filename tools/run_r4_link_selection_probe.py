#!/usr/bin/env python3
"""Run the offline R4 link selector against a locally generated EXE shard.

The generated shard and game binaries remain outside this source branch. This
executes the real recompiled function with synthetic RAM, without starting the
game, emulating SIO1, or committing generated game code.
"""

import argparse
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

from r4_link_manifest import EXE_BASE, EXE_HEADER_SIZE, EXE_SHA256, sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generated", type=Path, required=True,
                        help="directory containing SLUS_007.97_full_08.c and decls")
    parser.add_argument("--exe", type=Path, required=True,
                        help="matching local SLUS_007.97 for source preflight")
    parser.add_argument("--runtime-include", type=Path, required=True,
                        help="psxrecomp/runtime/include from the matching framework pin")
    parser.add_argument("--cc", default=os.environ.get("CC", "clang"))
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "tests/test_r4_link_selection.c"
    shards = [args.generated / "SLUS_007.97_full_08.c",
              args.generated / "SLUS_007.97_full_13.c"]
    decls = args.generated / "SLUS_007.97_decls.h"
    cpu_header = args.runtime_include / "cpu_state.h"
    for path in (source, *shards, decls, cpu_header):
        if not path.is_file():
            parser.error(f"required local input is missing: {path}")
    try:
        exe_bytes = args.exe.read_bytes()
        if sha256(exe_bytes) != EXE_SHA256:
            parser.error("EXE SHA-256 differs from authenticated SLUS_007.97")
        text = "\n".join(path.read_text() for path in shards)
    except OSError as exc:
        parser.error(str(exc))
    # A stale generated shard could still link and pass a synthetic test. Check
    # the exact non-branch instructions that form the four-input seam first.
    encoded = {int(a, 16): int(w, 16) for a, w in re.findall(
        r"/\* 0x([0-9A-Fa-f]{8}): 0x([0-9A-Fa-f]{8}) \*/", text)}
    for address in (0x80029C3C, 0x80029C5C, 0x80029C70, 0x80029C80,
                    0x80029C88, 0x80029C90, 0x80029C98, 0x80029CA0,
                    0x80029CA8, 0x800383E8, 0x800383EC, 0x8003840C,
                    0x8003841C, 0x80038450, 0x80038458, 0x80038470):
        offset = EXE_HEADER_SIZE + address - EXE_BASE
        retail_word = struct.unpack_from("<I", exe_bytes, offset)[0]
        if encoded.get(address) != retail_word:
            parser.error(f"generated shard differs from EXE at 0x{address:08X}")
    with tempfile.TemporaryDirectory(prefix="r4-link-select-") as directory:
        objects = [Path(directory) / (path.stem + ".o") for path in shards]
        exe = Path(directory) / "probe"
        base = [args.cc, "-DPSX_NO_DEBUG_TOOLS", "-ffunction-sections",
                "-fdata-sections", "-Wno-c23-extensions",
                f"-I{args.generated}", f"-I{args.runtime_include}"]
        for shard, obj in zip(shards, objects):
            subprocess.run(base + ["-c", str(shard), "-o", str(obj)], check=True)
        dead_strip = "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections"
        subprocess.run(base + [str(source), *(str(obj) for obj in objects),
                               dead_strip, "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
