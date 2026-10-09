#!/usr/bin/env python3
"""Headless loopback regression for R4's online Link Battle (2-4 seats).

Boots N headless peers of one debug build (-DPSX_NETPLAY=ON) on loopback
LAN netplay, host = seat 0, then drives every seat through R4's native menus:
Link Battle entry, Car Select, Course Select, a race with one driver per seat
(each peer's own pad), Results, Car & Course Change into a second race,
Retire, Results and Exit to the title. It checks seat order, per-seat
control, synchronized guest-state digests, the presented local view of every
peer, and dispatch / segment misses. Every runtime it starts is stopped on
exit. The disc and all outputs stay local (--work, default under /tmp).

  python3 tools/r4_online_battle_regression.py --build BUILD --peers 3 \\
      --work /tmp/r4-online-3 --report /tmp/r4-online-3/report.json
"""

import argparse
import json
import math
import os
import re
import shutil
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dbg import cmd  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DISC = ROOT / "disc" / "R4 - Ridge Racer Type 4 (USA).cue"

MODE = 0x800F4EF4        # u16, 4 = link race mode
LINK_FSM = 0x800FF838    # u16, 2 = entrants accepted
ACCEPTED = 0x800AC074    # 4 x u8 accepted entrant flags
CAR_COUNT = 0x800AC754   # u16 roster count
ROSTER = 0x800FFDD0      # 4 x car pointer
PHASE = 0x800FF860       # u32 race phase, 2 = racing
PAUSED = 0x800F4E18      # u8
ENTRANTS = 0x80107328    # u16

NEUTRAL = 0xFFFF
START = 0xFFF7
CIRCLE = 0xDFFF
CROSS = 0xBFFF            # accelerate
CROSS_BIT = 0x4000
LEFT, RIGHT, UP, DOWN, SQUARE = 0x0080, 0x0020, 0x0010, 0x0040, 0x8000


class Peers:
    def __init__(self, args):
        self.args = args
        self.n = args.peers
        self.procs, self.handles = [], []
        self.logs = [args.work / f"peer{i}.log" for i in range(self.n)]
        # --remote-seat: that seat runs on another machine (started there by
        # the operator, debug port forwarded here); its files come back by scp.
        self.remote = args.remote_seat
        self.remote_files = {}
        self.remote_log_at = 0.0
        self.modern_race = False   # --modern: triggers/stick in a race

    def view(self, i):
        """This seat's own widescreen View ('' = off)."""
        return self.args.host_widescreen if i == 0 else self.args.guest_widescreen

    def local(self, i):
        return i != self.remote

    def port(self, i):
        return self.args.remote_port if not self.local(i) \
            else self.args.debug_port_base + i

    def ask(self, i, name, timeout=5, **fields):
        if name == "set_input" and self.args.modern and "buttons" in fields \
                and self.modern_race:
            # Modern controls, as a player's gamepad drives them: gas = right
            # trigger, brake = left trigger, steering = left stick (the
            # transform makes it NeGcon twist). Cross / Square / D-pad
            # left-right become those; menus keep the plain buttons.
            b = int(fields["buttons"], 16)
            fields["rt"] = 255 if not b & CROSS_BIT else 0
            fields["lt"] = 255 if not b & SQUARE else 0
            fields.setdefault("lx", 0 if not b & LEFT else
                              255 if not b & RIGHT else 0x80)
            fields["buttons"] = hex(b | CROSS_BIT | SQUARE | LEFT | RIGHT)
            fields["pad_type"] = 1   # a DualShock-shaped gamepad: the stick passes
        if not self.local(i) and "path" in fields:
            local = Path(fields["path"])
            remote = f"{self.args.remote_dir}/shots/{local.name}"
            self.remote_files[str(local)] = remote
            fields["path"] = remote
        return cmd(dict(cmd=name, **fields), port=self.port(i),
                   timeout=max(timeout, 15) if not self.local(i) else timeout)

    def fetch(self, path):
        """Bring a file the remote seat wrote back to its local path."""
        remote = self.remote_files.pop(str(path), None)
        if remote:
            subprocess.check_call(["scp", "-q", f"{self.args.remote_ssh}:{remote}",
                                   str(path)])

    def read(self, i, address, length):
        return bytes.fromhex(self.ask(i, "read_ram", addr=hex(address),
                                      len=length)["hex"])

    def u8(self, i, a):
        return self.read(i, a, 1)[0]

    def u16(self, i, a):
        return struct.unpack("<H", self.read(i, a, 2))[0]

    def u32(self, i, a):
        return struct.unpack("<I", self.read(i, a, 4))[0]

    def frame(self, i=0):
        return self.ask(i, "frame").get("frame", 0)

    def log_text(self, i):
        if not self.local(i) and time.monotonic() - self.remote_log_at > 2:
            subprocess.call(["scp", "-q",
                             f"{self.args.remote_ssh}:{self.args.remote_dir}/peer{i}.log",
                             str(self.logs[i])])
            self.remote_log_at = time.monotonic()
        return self.logs[i].read_text(errors="replace") \
            if self.logs[i].exists() else ""

    def digests(self, i):
        """Live core digests by tick, without ticks this peer later rolled
        back over: an episode reloads its load tick and every peer in it
        recomputes load..target, so a digest logged there before the episode
        (on a predicted input, or a forward frame the resimulation replaces)
        is superseded by the replay."""
        text = self.log_text(i)
        replayed = [(int(m), int(t)) for m, t in re.findall(
            r"rb (?:begin|follow) epoch=\d+ mismatch=\d+ load=(\d+) "
            r"target=(\d+)", text)]
        return {int(t): c for t, c in re.findall(
            r"rb live dig local sim=(\d+) core=([0-9a-f]+)", text)
                if not any(m <= int(t) <= e for m, e in replayed)}

    def alive(self):
        for i, p in enumerate(self.procs):
            if p.poll() is not None:
                raise RuntimeError(f"peer {i} exited ({p.returncode}); see {self.logs[i]}")

    def wait(self, predicate, seconds, label):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.alive()
            if predicate():
                return
            time.sleep(0.1)
        raise TimeoutError(label)

    def wait_frames(self, frames, seconds=60):
        target = self.frame() + frames
        self.wait(lambda: self.frame() >= target, seconds, f"{frames} frames")

    def set_all(self, buttons):
        for i in range(self.n):
            self.ask(i, "set_input", buttons=hex(buttons))

    def pulse(self, buttons, held=8, released=72, seats=None):
        seats = range(self.n) if seats is None else seats
        start = self.frame()
        for i in seats:
            self.ask(i, "set_input", buttons=hex(buttons))
        self.wait(lambda: self.frame() >= start + held, 30, "pad hold")
        for i in seats:
            self.ask(i, "set_input", buttons=hex(NEUTRAL))
        self.wait(lambda: self.frame() >= start + held + released, 30,
                  "pad release")

    def provision(self):
        build = self.args.build.resolve()
        for i in range(self.n):
            if not self.local(i):
                continue
            d = self.args.work / f"peer{i}"
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True)
            for name in ("game.toml", "game_options.toml"):
                shutil.copy2(build / name, d / name)
            for name in ("bios", "mods", "assets"):
                if (build / name).exists():
                    shutil.copytree(build / name, d / name, symlinks=True)
            # A hard link under its own name: a concurrent `pkill r4-runtime`
            # elsewhere on a shared machine must not stop these peers.
            try:
                os.link(build / "r4-runtime", d / self.args.peer_name)
            except OSError:
                shutil.copy2(build / "r4-runtime", d / self.args.peer_name)
            (d / "memcards").mkdir()
            # Widescreen is each player's own choice (own view only).
            view = self.view(i)
            state = ""
            if view:
                state += ("\n[[feature]]\n"
                          "package_id = \"r4.enhancement.widescreen\"\n"
                          "id = \"widescreen\"\nenabled = true\n"
                          f"[feature.values]\naspect = \"{view}\"\n")
            if self.args.modern:
                state += ("\n[[feature]]\npackage_id = \"r4.modern-controls\"\n"
                          "id = \"modern-controls\"\nenabled = true\n"
                          "[feature.values]\nscheme = \"modern\"\n")
            if state:
                (d / "mods").mkdir(exist_ok=True)
                (d / "mods" / "state.toml").write_text("format_version = 2\n" + state)

    def launch(self):
        a = self.args
        for i in range(self.n):
            if not self.local(i):
                continue
            env = os.environ.copy()
            env.update(PSX_NETPLAY="1", PSX_NET_MODE=a.mode,
                       PSX_NET_TRANSPORT="lan", PSX_NET_SLOTS=str(self.n),
                       PSX_NET_SLOT=str(i),
                       PSX_NET_BIND=f"{a.bind_host}:{a.udp_port_base + i}",
                       PSX_NET_SESSION_ID=str(a.session_id),
                       PSX_NETPLAY_HEADLESS_INPUT_PROBE="1",
                       PSX_DEBUG_FMV_QUIET="0", SDL_AUDIODRIVER="dummy")
            if a.experimental_env:
                env["PSX_R4_LINK_EXPERIMENTAL"] = "1"
            else:
                env.pop("PSX_R4_LINK_EXPERIMENTAL", None)
            env.pop("PSX_R4_VIEW_COUNT_PROBE", None)
            env.pop("PSX_OVERLAY_AUTOCOMPILE_CMD", None)
            for k in ("PSX_R4_LINK_TEST_VIEW_COST_MS",
                      "PSX_R4_LINK_TEST_FRAME_COST_MS"):
                env.pop(k, None)
            if i in a.load_seat:
                if a.view_cost_ms:
                    env["PSX_R4_LINK_TEST_VIEW_COST_MS"] = str(a.view_cost_ms)
                if a.frame_cost_ms:
                    env["PSX_R4_LINK_TEST_FRAME_COST_MS"] = str(a.frame_cost_ms)
            if i:
                env["PSX_NET_PEER"] = f"127.0.0.1:{a.udp_port_base}"
            else:
                env.pop("PSX_NET_PEER", None)
            d = a.work / f"peer{i}"
            self.handles.append(self.logs[i].open("w"))
            self.procs.append(subprocess.Popen(
                ["./" + a.peer_name, "--game", "./game.toml", "--bios",
                 "./bios/openbios.bin", "--disc", str(a.disc),
                 "--memcard-dir", "./memcards", "--debug-port",
                 str(self.port(i)), "--no-launcher",
                 "--hidden-window" if a.frontend == "hidden" else "--headless"],
                cwd=d, env=env, stdout=self.handles[-1],
                stderr=subprocess.STDOUT))
            if a.join_gap:
                time.sleep(a.join_gap)

    def stop(self):
        for p in self.procs:
            if p.poll() is None:
                try:
                    os.kill(p.pid, signal.SIGCONT)
                except OSError:
                    pass
                p.terminate()
        for p in self.procs:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
        for h in self.handles:
            h.close()
        if self.remote >= 0:
            if self.args.remote_stop:
                subprocess.call(["ssh", self.args.remote_ssh, self.args.remote_stop])
            self.remote_log_at = 0.0
            self.log_text(self.remote)


def car_state(peers, i, seat, ptr=None):
    if ptr is None:
        ptr = struct.unpack("<4I", peers.read(i, ROSTER, 16))[seat]
    car = peers.read(i, ptr, 0x280)
    return dict(ptr=ptr,
                x=struct.unpack_from("<i", car, 0)[0],
                z=struct.unpack_from("<i", car, 8)[0],
                yaw=struct.unpack_from("<H", car, 0x54)[0],
                speed=struct.unpack_from("<h", car, 0x1D8)[0],
                rank=struct.unpack_from("<h", car, 0x1EE)[0],
                active=struct.unpack_from("<h", car, 0x1E8)[0] != -1,
                raw=car)


SEG_STRIDE = 0x3C       # course segment record: x>>8 @0, z>>8 @4 (i32)
CAR_SEGMENT = 0xF0      # i16 current course segment (racing = decreasing)
CAR_HEADING = 0x1EC     # u16 heading, 4096 per turn, atan2(dx, dz)


def find_course(peers, i=0):
    """Locate the closed course centreline from car 0's segment and position.

    Returns (base, count). The table lives in the course data loaded for the
    race; it is found, not hard-coded, so another course works the same way.
    """
    car = car_state(peers, i, 0)
    seg = struct.unpack_from("<h", car["raw"], CAR_SEGMENT)[0]
    cx, cz = car["x"] >> 8, car["z"] >> 8
    ram = b"".join(peers.read(i, 0x80100000 + off, 0x8000)
                   for off in range(0, 0x100000, 0x8000))

    def at(a):
        o = a - 0x80100000
        if o < 0 or o + 8 > len(ram):
            return None
        return struct.unpack_from("<ii", ram, o)
    for o in range(0, len(ram) - 8, 4):
        x, z = struct.unpack_from("<ii", ram, o)
        if abs(x - cx) > 600 or abs(z - cz) > 600:
            continue
        base = 0x80100000 + o - seg * SEG_STRIDE
        pts = []
        a = base
        while True:
            pt = at(a)
            if pt is None or (pts and math.dist(pt, pts[-1]) > 1200):
                break
            pts.append(pt)
            a += SEG_STRIDE
            if len(pts) > 4000:
                break
        if len(pts) > 100 and seg < len(pts) and \
                math.dist(pts[0], pts[-1]) < 1200 and \
                all(200 < math.dist(pts[k], pts[k + 1]) < 1200
                    for k in range(len(pts) - 1)):
            return base, pts
    raise RuntimeError("course centreline not found")


def angle(dx, dz):
    return (math.atan2(dx, dz) * 2048 / math.pi) % 4096


def wrap(a):
    return (a + 2048) % 4096 - 2048


class Driver:
    """Test-only autopilot for one seat: hold accelerate, steer toward the
    course centreline a speed-dependent distance ahead, brake into sharp
    corners. Inputs go through that seat's own peer (set_input), exactly like
    a player's pad; it reads only that peer's guest RAM."""

    def __init__(self, peers, seat, pts):
        self.peers, self.seat, self.pts = peers, seat, pts
        self.prev = None
        self.ptr = None
        self.stuck = 0
        self.reverse = 0
        self.trace = []

    def step(self):
        if self.ptr is None:
            self.ptr = struct.unpack("<4I", self.peers.read(
                self.seat, ROSTER, 16))[self.seat]
        c = car_state(self.peers, self.seat, self.seat, self.ptr)
        seg = struct.unpack_from("<h", c["raw"], CAR_SEGMENT)[0] % len(self.pts)
        pos = (c["x"] / 256.0, c["z"] / 256.0)
        n = len(self.pts)
        track = angle(*(a - b for a, b in zip(self.pts[(seg - 1) % n],
                                              self.pts[seg])))
        moved = math.dist(pos, self.prev) if self.prev else 0.0
        heading = angle(pos[0] - self.prev[0], pos[1] - self.prev[1]) \
            if self.prev and moved > 8 else track
        self.prev = pos
        speed = c["speed"]
        look = 2 + int(max(speed, 0) / 160)
        target = self.pts[(seg - look) % n]
        desired = angle(target[0] - pos[0], target[1] - pos[1])
        err = wrap(desired - heading)
        far = angle(*(a - b for a, b in zip(
            self.pts[(seg - look - 6) % n], self.pts[(seg - look) % n])))
        bend = abs(wrap(far - track))
        limit = 900 if bend < 200 else 700 if bend < 400 else 520 if bend < 700 else 380
        buttons = NEUTRAL & ~0x4000          # X: accelerate
        if speed > limit:
            buttons = NEUTRAL & ~0x8000      # Square: brake
        if err < -60:
            buttons &= ~LEFT
        elif err > 60:
            buttons &= ~RIGHT
        self.stuck = self.stuck + 1 if speed < 40 else 0
        if self.stuck > 25 and not self.reverse:
            self.reverse = 12
        if self.reverse:
            self.reverse -= 1
            # Back off the wall: brake/reverse while steering away.
            buttons = NEUTRAL & ~0x8000 & ~(RIGHT if err < 0 else LEFT)
            if not self.reverse:
                self.stuck = 0
        self.peers.ask(self.seat, "set_input", buttons=hex(buttons))
        self.trace.append(dict(seg=seg, speed=speed, err=round(err),
                               bend=round(bend), buttons=hex(buttons)))
        return seg, speed


def png_rgb(path):
    """Decode a PNG to (w, h, rgb bytes) with ffmpeg (no Python imaging)."""
    probe = subprocess.check_output(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
         "stream=width,height", "-of", "csv=p=0", str(path)], text=True)
    w, h = (int(v) for v in probe.strip().split(","))
    rgb = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(path),
                                   "-f", "rawvideo", "-pix_fmt", "rgb24", "-"])
    return w, h, rgb


def quadrant_distance(full, thumb, qx, qy):
    """Mean absolute RGB difference between a 160x120 thumbnail and one
    quadrant of a 320x240 frame."""
    fw, _, frgb = full
    tw, th, trgb = thumb
    total = 0
    for y in range(0, th, 2):
        for x in range(0, tw, 2):
            a = 3 * ((qy + y) * fw + qx + x)
            b = 3 * (y * tw + x)
            total += sum(abs(frgb[a + k] - trgb[b + k]) for k in range(3))
    return total / ((th // 2) * (tw // 2) * 3)


def view_distance(full, thumb, qx, qy):
    """Own full-screen view (hidden-window peers) against one quadrant of the
    canonical frame: the same camera at half size, but a different HUD and a
    rear-view mirror, so only the scene's middle band is compared."""
    fw, _, frgb = full
    tw, _, trgb = thumb
    total = n = 0
    for y in range(40, 100, 2):
        for x in range(30, 130, 2):
            a = 3 * ((qy + y) * fw + qx + x)
            b = 3 * (y * tw + x)
            total += sum(abs(frgb[a + k] - trgb[b + k]) for k in range(3))
            n += 3
    return total / n


def scaled_distance(full, thumb):
    """Mean absolute RGB difference between a 160x120 thumbnail and the
    whole 320x240 frame sampled at half resolution."""
    fw, _, frgb = full
    tw, th, trgb = thumb
    total = 0
    for y in range(0, th, 2):
        for x in range(0, tw, 2):
            a = 3 * ((2 * y) * fw + 2 * x)
            b = 3 * (y * tw + x)
            total += sum(abs(frgb[a + k] - trgb[b + k]) for k in range(3))
    return total / ((th // 2) * (tw // 2) * 3)


def presented_window(peers, i, path, full_path=None):
    """Hidden-window peers: the window's presented image (present_shot),
    scaled to 160x120 at `path`, and the canonical display frame (the
    authoritative VRAM, `screenshot`) at `full_path`."""
    raw = path.with_suffix(".window.png")
    wide = bool(peers.view(i))
    seq = peers.ask(i, "present_shot_seq")["seq"]
    peers.ask(i, "present_shot", path=str(raw))
    peers.wait(lambda: peers.ask(i, "present_shot_seq")["seq"] != seq, 10,
               f"present_shot on peer {i}")
    if full_path is not None:
        # Widescreen: "screenshot" would return the presented wide surface;
        # the canonical 4:3 frame (authoritative VRAM) is screenshot_file.
        peers.ask(i, "screenshot_file" if wide else "screenshot",
                  path=str(full_path))
    # A wide own view is compared on its middle 4:3 (same camera, the extra
    # columns are beside it).
    peers.fetch(raw)
    if full_path is not None:
        peers.fetch(full_path)
    vf = "scale=160:120:flags=area"
    if wide:
        # The picture's height: a fixed View narrower than the window is
        # letterboxed in it; then its middle 4:3.
        w0, h0, _ = png_rgb(raw)
        num, den = (int(v) for v in peers.view(i).split(":")) \
            if ":" in peers.view(i) else (w0, h0)
        ch = min(h0, w0 * den // num)
        vf = f"crop={ch * 4 // 3}:{ch}," + vf
    subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-i", str(raw),
                           "-vf", vf, str(path)])
    w, h, rgb = png_rgb(raw)
    side = max(1, (w - h * 4 // 3) // 2)
    lit = n = 0
    for y in range(h // 8, h * 7 // 8, 8):
        for x in list(range(0, side, 4)) + list(range(w - side, w, 4)):
            a = 3 * (y * w + x)
            lit += sum(rgb[a:a + 3]) > 48
            n += 1
    return dict(ok=True, width=w, height=h, window=str(raw),
                side_lit=round(lit / max(1, n), 3))


def presented(peers, i, path, full_path=None):
    """This peer's newest presented image (headless present ring) as PNG,
    plus, when asked, the full display frame of the same guest frame."""
    if peers.args.frontend == "hidden":
        return presented_window(peers, i, path, full_path)
    stats = peers.ask(i, "present_image_ring_stats")
    for frame in range(stats["newest"], stats["newest"] - 8, -1):
        response = peers.ask(i, "present_image_ring_get", frame=frame,
                             path=str(path))
        if not response.get("ok"):
            continue
        peers.fetch(path)
        if full_path is None:
            return response
        full = peers.ask(i, "display_ring_get", frame=frame,
                         path=str(full_path))
        if full.get("ok"):
            peers.fetch(full_path)
            return response
    raise RuntimeError(f"present/display ring read failed on peer {i}")


def local_views(peers, shots, label):
    """Every peer presents exactly its own seat's quadrant of the shared
    frame. Freeze each peer's present ring, compare its newest presented
    image with the four quadrants of a full-frame screenshot."""
    rows = []
    for i in range(peers.n):
        full_path = shots / f"{label}-full.peer{i}.png"
        thumb_path = shots / f"{label}-presented.peer{i}.png"
        # The window image and the canonical screenshot are taken a moment
        # apart; a moving car can cross into a tunnel in between, so a
        # hidden-window peer gets a few tries.
        for attempt in range(4 if peers.args.frontend == "hidden" else 1):
            info = presented(peers, i, thumb_path, full_path)
            full, thumb = png_rgb(full_path), png_rgb(thumb_path)
            metric = view_distance if peers.args.frontend == "hidden" \
                else quadrant_distance
            dist = [round(metric(full, thumb, (q & 1) * 160, (q >> 1) * 120), 1)
                    for q in range(peers.n)]
            if dist.index(min(dist)) == i:
                break
        row = dict(peer=i, presented=[info["width"], info["height"]],
                   side_lit=info.get("side_lit"),
                   quadrant_distance=dist,
                   best=dist.index(min(dist)),
                   thumb=str(thumb_path))
        if peers.args.frontend == "hidden":
            st = peers.ask(i, "render_pass_stats")
            row.update(window=info["window"], local_views=st["local_views"],
                       local_attempts=st["local_attempts"],
                       local_status=st["local_status"])
        rows.append(row)
    # A peer behind the match sheds its own view (status 5, FAST_FORWARD)
    # and presents its quadrant of the shared frame instead: still its seat.
    # Under a load that only the own view adds, it alternates between the
    # two, so a sample may catch the quadrant just after the view returned.
    def shed(r):
        return r.get("local_status") == 5 or \
            "netplay own view SHED" in peers.log_text(r["peer"])
    ok = all(r["best"] == r["peer"] for r in rows) and \
        all(r.get("local_views", 1) > 0 or shed(r) for r in rows)
    if peers.args.frontend == "hidden":
        # A widescreen player's own view has scenery in the side columns.
        ok = ok and all(r["side_lit"] > 0.5 for r in rows
                        if peers.view(r["peer"]) and not shed(r))
    if not ok:
        raise AssertionError(f"local views: {rows}")
    return dict(ok=ok, peers=rows)


def seat_control(peers):
    """From the grid, each seat in turn accelerates alone: that seat's car
    must move while every seat not yet pressed stays put."""
    rows = []
    start = [car_state(peers, 0, s) for s in range(peers.n)]
    for seat in range(peers.n):
        before = [car_state(peers, 0, s) for s in range(peers.n)]
        peers.ask(seat, "set_input", buttons=hex(CROSS))
        peers.wait_frames(90)
        peers.ask(seat, "set_input", buttons=hex(NEUTRAL & ~0x8000))
        after = [car_state(peers, 0, s) for s in range(peers.n)]
        moved = [math.dist((a["x"], a["z"]), (b["x"], b["z"])) / 256
                 for a, b in zip(before, after)]
        rows.append(dict(seat=seat, moved=[round(m) for m in moved]))
        if moved[seat] < 20 or any(moved[j] > 1 for j in range(seat + 1, peers.n)):
            raise AssertionError(f"seat {seat} control: {rows}")
        if peers.args.modern:
            # Brake (left trigger) and steering (left stick) of this seat.
            peers.ask(seat, "set_input", buttons=hex(CROSS))
            peers.wait_frames(60)
            fast = car_state(peers, 0, seat)
            peers.ask(seat, "set_input", buttons=hex(NEUTRAL & ~SQUARE))
            peers.wait_frames(20)
            fast = max(fast, car_state(peers, 0, seat), key=lambda c: c["speed"])
            peers.wait_frames(60)   # input delay grows with seats
            slow = car_state(peers, 0, seat)
            peers.ask(seat, "set_input", buttons=hex(CROSS), lx=0)
            peers.wait_frames(40)
            turned = car_state(peers, 0, seat)
            peers.ask(seat, "set_input", buttons=hex(NEUTRAL & ~SQUARE))
            peers.wait_frames(60)
            hd = lambda c: c["yaw"]
            turn = (hd(turned) - hd(slow) + 2048) % 4096 - 2048
            rows[-1].update(speed_gas=fast["speed"], speed_braked=slow["speed"],
                            heading_turn=turn)
            if slow["speed"] >= fast["speed"] or abs(turn) < 16:
                raise AssertionError(f"seat {seat} brake/steer: {rows}")
    for seat in range(peers.n):
        peers.ask(seat, "set_input", buttons=hex(NEUTRAL))
    return dict(ok=True, steps=rows,
                grid=[(c["x"] >> 8, c["z"] >> 8) for c in start])


def drive(peers, pts, args, shot, label, until_finish=False, seconds=None):
    drivers = [Driver(peers, seat, pts) for seat in range(peers.n)]
    t0 = time.monotonic()
    f0 = peers.frame()
    telemetry = []
    last = 0
    laps = {}
    while True:
        peers.alive()
        elapsed = time.monotonic() - t0
        if seconds is not None and elapsed > seconds:
            break
        if until_finish and peers.u32(0, PHASE) != 2:
            break
        if elapsed > args.race_timeout:
            shot(f"{label}-timeout")
            raise TimeoutError(f"{label} finish")
        row = [d.step() for d in drivers]
        if time.monotonic() - last > 2:
            last = time.monotonic()
            telemetry.append(dict(frame=peers.frame(), cars=row))
            if args.verbose:
                print(label, telemetry[-1], flush=True)
        time.sleep(0.05)
    peers.set_all(NEUTRAL)
    (args.work / f"{label}.telemetry.json").write_text(json.dumps(
        dict(samples=telemetry, traces=[d.trace for d in drivers])))
    out = dict(seconds=round(time.monotonic() - t0, 1),
               frames=peers.frame() - f0,
               phase=[peers.u32(i, PHASE) for i in range(peers.n)],
               shot=shot(f"{label}-end"))
    return out


def wait_results(peers, shot, label):
    try:
        # Race phase 4 after a finish, 6 after Retire; both show Results.
        peers.wait(lambda: all(peers.u32(i, PHASE) in (4, 6)
                               for i in range(peers.n)), 180, f"{label} screen")
    except TimeoutError:
        shot(f"{label}-timeout")
        raise
    peers.wait_frames(420, 120)
    return dict(phase=[peers.u32(i, PHASE) for i in range(peers.n)],
                ranks=[car_state(peers, 0, s)["rank"]
                                for s in range(peers.n)],
                shot=shot(label))


def restart(peers, shot, race_ready):
    """Results -> Car & Course Change (default item) -> both preset car
    selects -> Course Select Start -> a new race on every peer."""
    taps = []
    # A finished race shows its winner page first; Course Select may leave
    # its cursor off Start (Up returns it).
    plan = [CIRCLE] * 7 + [NEUTRAL & ~UP, CIRCLE, CIRCLE] * 3
    for k, buttons in enumerate(plan):
        if race_ready():
            break
        peers.pulse(buttons, released=100)
        taps.append(hex(buttons))
        if k in (0, 1, 5):
            shot(f"restart-{k}")
    try:
        peers.wait(race_ready, 120, "restart race")
    except TimeoutError:
        shot("restart-timeout")
        raise
    return dict(taps=taps, cars=[peers.u16(i, CAR_COUNT) for i in range(peers.n)],
                shot=shot("race2"))


def pause_retire(peers, shots, shot):
    """Start from the last seat pauses every peer (each then presents the
    whole frame with the shared menu); Down twice from seat 0 selects
    RETIRE, Start confirms it."""
    last = peers.n - 1
    peers.pulse(START, released=40, seats=[last])
    peers.wait(lambda: all(peers.u8(i, PAUSED) == 1 for i in range(peers.n)),
               30, "pause on every peer")
    peers.wait_frames(20)
    paused_views = []
    for i in range(peers.n):
        thumb_path = shots / f"pause-presented.peer{i}.png"
        full_path = shots / f"pause-full.peer{i}.png"
        presented(peers, i, thumb_path, full_path)
        full, thumb = png_rgb(full_path), png_rgb(thumb_path)
        whole = round(scaled_distance(full, thumb), 1)
        own = round(quadrant_distance(full, thumb, (i & 1) * 160,
                                      (i >> 1) * 120), 1)
        paused_views.append(dict(peer=i, whole_frame=whole, own_quadrant=own))
        if not whole < own:
            raise AssertionError(f"paused peer {i} does not show the whole "
                                 f"frame: {paused_views}")
    pause_shot = shot("pause")
    menu = []
    for _ in range(2):
        peers.pulse(NEUTRAL & ~DOWN, released=40, seats=[0])
        menu.append([peers.u8(i, 0x800F318C) for i in range(peers.n)])
    shot("pause-retire")
    confirms = []
    for buttons in (START, CIRCLE, START, CIRCLE):
        if all(peers.u32(i, PHASE) != 2 for i in range(peers.n)):
            break
        peers.pulse(buttons, released=120, seats=[0])
        confirms.append(dict(buttons=hex(buttons),
                             phase=[peers.u32(i, PHASE) for i in range(peers.n)],
                             paused=[peers.u8(i, PAUSED) for i in range(peers.n)],
                             shot=shot(f"retire-confirm{len(confirms)}")))
    return dict(paused=[1] * peers.n, presented_while_paused=paused_views,
                menu_index=menu, confirms=confirms, shot=pause_shot)


def exit_to_title(peers, shot):
    peers.pulse(NEUTRAL & ~DOWN, released=60)
    peers.pulse(CIRCLE, released=60)
    peers.wait(lambda: all(peers.u16(i, MODE) != 4 for i in range(peers.n)),
               120, "exit to title")
    peers.wait_frames(240, 120)
    return dict(mode=[peers.u16(i, MODE) for i in range(peers.n)],
                shot=shot("title"))


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--peers", type=int, choices=(2, 3, 4), required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    p.add_argument("--disc", type=Path, default=DISC)
    p.add_argument("--mode", choices=("delay", "rollback"), default="delay")
    p.add_argument("--frontend", choices=("headless", "hidden"),
                   default="headless",
                   help="hidden: OpenGL peers in never-shown windows; each "
                        "draws its own full-screen view (present_shot)")
    p.add_argument("--host-widescreen", default="",
                   help="seat 0 enables r4.enhancement.widescreen at this "
                        "View (Fit, 16:9, 21:9, 32:9) for its own view")
    p.add_argument("--guest-widescreen", default="",
                   help="the other seats' own widescreen View (default off)")
    p.add_argument("--modern", action="store_true",
                   help="every seat runs R4 Modern controls (an input "
                        "transform kept online): accelerate is the right "
                        "trigger (rt=255), not Cross")
    p.add_argument("--remote-seat", type=int, default=-1,
                   help="this seat (the last) runs on another machine, started "
                        "there with the same session env; its debug port is "
                        "forwarded to --remote-port")
    p.add_argument("--remote-port", type=int, default=5996)
    p.add_argument("--remote-ssh", default="pc")
    p.add_argument("--remote-dir", default="",
                   help="the remote peer's directory as scp names it "
                        "(peer<seat>.log and shots/ live there)")
    p.add_argument("--remote-stop", default="",
                   help="ssh command that stops the remote peer")
    p.add_argument("--bind-host", default="127.0.0.1",
                   help="address local peers bind (the LAN address for a "
                        "remote seat)")
    p.add_argument("--debug-port-base", type=int, default=5895)
    p.add_argument("--udp-port-base", type=int, default=48841)
    p.add_argument("--session-id", type=int, default=98435)
    p.add_argument("--join-gap", type=float, default=0.5,
                   help="seconds between guest launches (join order)")
    p.add_argument("--load-seat", type=int, action="append", default=[],
                   help="test load on this seat (repeatable): the costs below "
                        "are spent in its host time each frame / own view")
    p.add_argument("--view-cost-ms", type=int, default=0,
                   help="extra host ms per own-view draw on --load-seat")
    p.add_argument("--frame-cost-ms", type=int, default=0,
                   help="extra host ms per race frame on --load-seat")
    p.add_argument("--peer-name", default="r4peer",
                   help="process name of every peer (distinct per run so "
                        "stray peers can be found and stopped by name)")
    p.add_argument("--experimental-env", action="store_true",
                   help="set PSX_R4_LINK_EXPERIMENTAL=1 on every peer "
                        "(older builds only; online battle no longer needs it)")
    p.add_argument("--stop-after", default="exit",
                   choices=("entry", "race", "explore", "finish", "restart", "exit"))
    p.add_argument("--explore-seconds", type=float, default=20)
    p.add_argument("--dump-ram", action="store_true")
    p.add_argument("--steer-probe", action="store_true")
    p.add_argument("--pause-dump", action="store_true")
    p.add_argument("--race-timeout", type=float, default=900)
    p.add_argument("--verbose", action="store_true")
    p.add_argument("--menu-probe", default="",
                   help="debugging: all-seat taps from Car Select, a shot after each")
    p.add_argument("--course-probe", default="",
                   help="debugging: seat-0 taps at Course Select, a shot after each")
    p.add_argument("--course-taps", default="",
                   help="seat-0 taps at Course Select before Start (e.g. laps)")
    p.add_argument("--retire-first-race", action="store_true",
                   help="debugging: Retire instead of finishing race 1")
    args = p.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    peers = Peers(args)
    result = dict(peers=args.peers, mode=args.mode, steps={})
    shots = args.work / "shots"
    shots.mkdir(exist_ok=True)

    def shot(label, i=0):
        path = shots / f"{label}.peer{i}.png"
        peers.ask(i, "screenshot", path=str(path))
        return str(path)

    try:
        peers.provision()
        peers.launch()
        peers.wait(lambda: all("netplay lockstep armed" in peers.log_text(i)
                               for i in range(peers.n)), 60, "netplay arm")
        # Seat = session slot each runtime reports (host 0, guests in the
        # order they were started and joined).
        result["slots"] = [int(m.group(1)) if (m := re.search(
            r"psx_netplay: started transport=\S+ slot=(\d+)",
            peers.log_text(i))) else None for i in range(peers.n)]
        # Widescreen is per player: each peer's own-view mods, from its log.
        result["own_view_mods"] = [re.findall(
            r"netplay keeps (\S+) for this player's own view only",
            peers.log_text(i)) for i in range(peers.n)]
        if args.frontend == "hidden":
            # A widescreen player's 16:9 window (presentation only).
            for i in range(peers.n):
                if peers.view(i):
                    peers.ask(i, "window_size", w=1280, h=720)
        peers.wait(lambda: peers.frame() >= 1200, 120, "boot")

        def accepted():
            want = bytes([1] * peers.n + [0] * (4 - peers.n))
            return all(peers.u16(i, MODE) == 4 and peers.u16(i, LINK_FSM) == 2
                       and peers.read(i, ACCEPTED, 4) == want
                       for i in range(peers.n))
        pulses = 0
        while not accepted():
            if pulses >= 75:
                shot("entry-timeout")
                raise TimeoutError("native Link entry")
            peers.pulse(START)
            pulses += 1
        result["steps"]["entry"] = dict(start_pulses=pulses,
                                        accepted=list(peers.read(0, ACCEPTED, 4)))
        if args.stop_after == "entry":
            return
        peers.wait_frames(600, 60)
        result["steps"]["car_select_shot"] = shot("car-select")
        if args.menu_probe:
            for k, word in enumerate(args.menu_probe.split(",")):
                peers.pulse(int(word, 0), released=80)
                shot(f"menu-probe{k}")
            return
        for _ in range(4):
            peers.pulse(CIRCLE, released=80)
        result["steps"]["course_select_shot"] = shot("course-select")
        if args.course_probe:
            for k, word in enumerate(args.course_probe.split(",")):
                peers.pulse(int(word, 0), released=60, seats=[0])
                shot(f"course-probe{k}")
            return
        for k, word in enumerate(filter(None, args.course_taps.split(","))):
            peers.pulse(int(word, 0), released=60, seats=[0])
        if args.course_taps:
            result["steps"]["course_setup_shot"] = shot("course-setup")
        peers.pulse(CIRCLE, released=80)

        def race_ready():
            return all(peers.u16(i, MODE) == 4 and
                       peers.u16(i, CAR_COUNT) == peers.n and
                       peers.u32(i, PHASE) == 2 for i in range(peers.n))
        peers.wait(race_ready, 120, "race start")
        result["steps"]["race"] = dict(
            entrants=[peers.u16(i, ENTRANTS) for i in range(peers.n)],
            cars=[peers.u16(i, CAR_COUNT) for i in range(peers.n)])
        result["steps"]["race_shot"] = shot("race")
        peers.modern_race = bool(args.modern)
        if args.stop_after == "race":
            peers.wait_frames(300, 60)
            result["steps"]["local_views"] = local_views(peers, shots, "race")
            _, pts = find_course(peers)
            drive(peers, pts, args, shot, "race1", seconds=args.explore_seconds)
            result["steps"]["local_views_driving"] = local_views(
                peers, shots, "driving")
            return
        if args.stop_after in ("finish", "restart", "exit"):
            base, pts = find_course(peers)
            result["course"] = dict(base=hex(base), segments=len(pts))
            result["steps"]["seat_control"] = seat_control(peers)
            result["steps"]["local_views"] = local_views(peers, shots, "race")
            if args.retire_first_race:
                drive(peers, pts, args, shot, "race1", seconds=8)
                result["steps"]["pause1"] = pause_retire(peers, shots, shot)
            else:
                result["steps"]["finish"] = drive(peers, pts, args, shot,
                                                  "race1", until_finish=True)
            result["steps"]["results"] = wait_results(peers, shot, "results1")
            if args.stop_after == "finish":
                return
            result["steps"]["restart"] = restart(peers, shot, race_ready)
            if args.stop_after == "restart":
                return
            drive(peers, pts, args, shot, "race2", seconds=8)
            result["steps"]["pause"] = pause_retire(peers, shots, shot)
            result["steps"]["results2"] = wait_results(peers, shot, "results2")
            result["steps"]["exit"] = exit_to_title(peers, shot)
            return
        if args.stop_after == "explore" and args.pause_dump:
            peers.wait_frames(120)
            peers.pulse(START, released=40, seats=[0])
            peers.wait_frames(30)
            shot("pause-dump")
            ring = peers.ask(0, "gpu_ring_stats")
            newest = ring.get("newest_frame", 0)
            frames = [peers.ask(0, "gpu_frame_dump", frame=f, count=8192, timeout=20)
                      for f in range(newest - 3, newest + 1)]
            (args.work / "pause_gpu.json").write_text(json.dumps(frames))
            return
        if args.stop_after == "explore" and args.steer_probe:
            base, pts = find_course(peers)
            result["course"] = dict(base=hex(base), segments=len(pts))
            rows = []
            for buttons in (CROSS, CROSS & ~LEFT, CROSS & ~RIGHT):
                peers.set_all(buttons)
                t0 = time.monotonic()
                while time.monotonic() < t0 + 1.5:
                    c = car_state(peers, 0, 0)
                    rows.append(dict(buttons=hex(buttons), frame=peers.frame(),
                                     heading=struct.unpack_from("<H", c["raw"], CAR_HEADING)[0],
                                     seg=struct.unpack_from("<h", c["raw"], CAR_SEGMENT)[0],
                                     x=c["x"] >> 8, z=c["z"] >> 8, speed=c["speed"]))
                    time.sleep(0.1)
            result["steer_probe"] = rows
            return
        if args.stop_after == "explore":
            peers.set_all(CROSS)
            end = time.monotonic() + args.explore_seconds
            samples = []
            while time.monotonic() < end:
                peers.alive()
                row = dict(frame=peers.frame(), cars=[])
                for seat in range(peers.n):
                    c = car_state(peers, 0, seat)
                    row["cars"].append(dict(x=c["x"], z=c["z"], yaw=c["yaw"],
                                            speed=c["speed"],
                                            raw=c["raw"].hex()))
                samples.append(row)
                time.sleep(0.5)
            (args.work / "explore.json").write_text(json.dumps(samples))
            if args.dump_ram:
                peers.ask(0, "pause") if False else None
                ram = b"".join(peers.read(0, 0x80000000 + off, 0x8000)
                               for off in range(0, 0x200000, 0x8000))
                (args.work / "ram.bin").write_bytes(ram)
                (args.work / "ram_cars.json").write_text(json.dumps(
                    [dict((k, v) for k, v in car_state(peers, 0, s).items()
                          if k != "raw") for s in range(peers.n)]))
            return
    finally:
        try:
            if peers.procs and all(p.poll() is None for p in peers.procs):
                stats = [peers.ask(i, "dispatch_stats") for i in range(peers.n)]
                result["dispatch_misses"] = [st.get("miss_total") for st in stats]
                result["segment_misses"] = [st.get("segment_miss_total")
                                            for st in stats]
        except Exception as exc:  # noqa: BLE001
            result["dispatch_error"] = str(exc)
        try:
            if args.frontend == "hidden" and peers.procs and \
                    all(p.poll() is None for p in peers.procs):
                result["local_view_stats"] = [
                    {k: v for k, v in peers.ask(i, "render_pass_stats").items()
                     if k in ("local_views", "local_attempts", "local_status",
                              "aborted", "watchdog", "vram_leaks",
                              "verify_mismatch", "avg_pass_ms",
                              "avg_guest_ms", "last_failure")}
                    for i in range(peers.n)]
        except Exception as exc:  # noqa: BLE001
            result["local_view_error"] = str(exc)
        peers.stop()
        rows = [peers.digests(i) for i in range(peers.n)] if peers.logs[0].exists() else []
        if rows:
            common = sorted(set.intersection(*(set(r) for r in rows)))
            result["digest_common"] = len(common)
            result["digest_mismatch"] = [t for t in common
                                         if len({r[t] for r in rows}) != 1][:20]
        args.report.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({k: v for k, v in result.items() if k != "steps"}))


if __name__ == "__main__":
    main()
