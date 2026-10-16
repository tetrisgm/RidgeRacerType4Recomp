#!/usr/bin/env python3
"""Check an in-game menu provider against psxrecomp's menu host contract.

Drives one R4 debug build (hidden window, dummy audio) whose menu provider is
recomp-ui (`--adapter rui`, driven with its keys over `host_key`) or
recomp-launcher (`--adapter rl`, driven with an R4L_OVERLAY_SCRIPT), and
checks, through the runtime's `menu_host`, `frame`, `pad_status` and
`disc_tray_log` debug commands:

  attach       the provider attached (and how it was selected)
  pause        opening the menu pauses the guest; closing resumes it
  capture      a game key pressed while the menu is open never reaches the
               pad (rui: with Pause in menu off, so the game keeps polling)
  settings     texture filtering round trip to xBR (live and saved)
  tray         Disc 2 inserted through the runtime's tray policy
  restart      a restart-only edit (render thread) relaunches on close, and
               the relaunched game keeps the new settings
  presets      (rl) Graphics under R4's stacked presets: a pick, a governed
               edit turning it Custom, xBR and back, the JogCon FFB toggle,
               and every row reachable by wheel and by gamepad nav
  none         PSX_OVERLAY=0: no provider attaches, the menu keys reach the
               game, the game runs

The game dir needs a two-disc `discs = [...]` roster in its game.toml.
"""
import argparse
import json
import os
import re
import socket
import subprocess
import sys
import time
from pathlib import Path


def ask(port, obj, timeout=5):
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
        s.sendall((json.dumps(obj) + "\n").encode())
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            buf += chunk
    for line in reversed(buf.decode(errors="replace").splitlines()):
        try:
            return json.loads(line)
        except ValueError:
            pass
    return {}


class Run:
    def __init__(self, a, extra_env=None, label="run", launcher=False):
        self.a, self.port = a, a.port
        self.log = Path(a.dir) / f"check-{a.adapter}-{label}.log"
        env = os.environ.copy()
        env.update(SDL_AUDIODRIVER="dummy", PSX_HIDDEN_WINDOW="1", HOME=a.home,
                   R4L_HIDDEN="1", R4L_SURFACE="all")
        env.update(extra_env or {})
        self.fh = self.log.open("w")
        exe = str(Path(a.dir) / "r4-runtime.exe") if os.name == "nt" else "./r4-runtime"
        argv = [exe, "--game", "game.toml", "--disc", a.disc, "--debug-port", str(self.port)]
        if not launcher:
            argv.append("--no-launcher")
        self.p = subprocess.Popen(argv,
                                  cwd=a.dir, env=env, stdout=self.fh, stderr=subprocess.STDOUT)

    def q(self, _cmd, **kw):
        kw["cmd"] = _cmd
        return ask(self.port, kw)

    def up(self, secs=60):
        end = time.time() + secs
        while time.time() < end:
            try:
                if self.q("ping").get("ok"):
                    return True
            except OSError:
                pass
            time.sleep(0.3)
        return False

    def frame(self):
        return self.q("frame").get("frame", 0)

    def mh(self):
        return self.q("menu_host")

    def until(self, pred, secs, what):
        end = time.time() + secs
        while time.time() < end:
            try:
                if pred():
                    return True
            except (OSError, KeyError):
                pass
            time.sleep(0.2)
        raise SystemExit(f"FAIL timeout: {what}\n  menu_host={self.safe_mh()}")

    def safe_mh(self):
        try:
            return self.mh()
        except OSError as e:
            return str(e)

    def advancing(self, secs=1.0):
        a = self.frame(); time.sleep(secs); return self.frame() > a

    def key(self, name, down=None):
        kw = {"name": name}
        if down is not None:
            kw["down"] = down
        self.q("host_key", **kw)
        time.sleep(0.25)

    def host_buttons(self):
        return int(self.q("pad_status")["host_buttons"], 16)

    def boot(self, to_frame=900):
        if not self.up():
            raise SystemExit(f"FAIL runtime did not answer; see {self.log}")
        self.q("turbo", enabled=1)
        self.until(lambda: self.frame() >= to_frame, 120, f"frame {to_frame}")
        self.q("turbo", enabled=0)

    def stop(self):
        if self.p.poll() is None:
            self.p.terminate()
            try:
                self.p.wait(10)
            except subprocess.TimeoutExpired:
                self.p.kill()
        self.fh.close()

    def text(self):
        return self.log.read_text(errors="replace")


RESULTS = []


def ok(name, cond, detail=""):
    RESULTS.append((name, bool(cond), detail))
    print(f"{'PASS' if cond else 'FAIL'} {name} {detail}", flush=True)


def saved_filter(a):
    v = settings_value(a, "texture_filtering")
    return {"\"xbr\"": 3, "\"bilinear\"": 1, "\"nearest\"": 0}.get(v, v)


def settings_value(a, key):
    t = (Path(a.dir) / "settings.toml").read_text(errors="replace") if (Path(a.dir) / "settings.toml").exists() else ""
    m = re.search(rf"^{key}\s*=\s*(\S+)", t, re.M)
    return m.group(1) if m else None


def set_render_thread(a, on):
    p = Path(a.dir) / "settings.toml"
    t = p.read_text() if p.exists() else ""
    if re.search(r"^render_thread\s*=", t, re.M):
        t = re.sub(r"^render_thread\s*=.*$", f"render_thread = {'true' if on else 'false'}", t, flags=re.M)
    elif "[video]" in t:
        t = t.replace("[video]", f"[video]\nrender_thread = {'true' if on else 'false'}", 1)
    else:
        t += f"\n[video]\nrender_thread = {'true' if on else 'false'}\n"
    p.write_text(t)


def tray_last(r):
    log = r.q("disc_tray_log")
    ev = log.get("events") or []
    return ev[-1] if ev else {}


# ---- recomp-ui: its keys -----------------------------------------------------
class RuiNav:
    """Section/row bookkeeping for recomp-ui's model (sections in first-seen
    order: Display, Graphics, System, Disc)."""
    SECTIONS = 4

    def __init__(self, r):
        self.r, self.sec, self.inside = r, 0, False

    def open(self):
        # the provider opens at the first section
        self.r.key("F10"); self.inside = False; self.sec = 0
        time.sleep(0.3)

    def close(self):
        self.r.key("F10"); self.inside = False

    def go(self, sec, row):
        if self.inside:
            self.r.key("Backspace"); self.inside = False
        for _ in range((sec - self.sec) % self.SECTIONS):
            self.r.key("Down")
        self.sec = sec
        self.r.key("Return"); self.inside = True
        for _ in range(row):
            self.r.key("Down")


def check_rui(a):
    G, S, D = 1, 2, 3          # Graphics, System, Disc sections
    r = Run(a, label=f"rt{a.render_thread}")
    try:
        r.boot()
        m = r.mh()
        ok("attach", m.get("attached") == 1 and m.get("provider") == "recomp-ui",
           f"provider={m.get('provider')} default_selection={m.get('default_selection')} render_thread={m.get('render_thread')}")
        nav = RuiNav(r)
        nav.open()
        r.until(lambda: r.mh()["open"] == 1, 5, "menu open")
        m = r.mh()
        ok("pause", m["pause_depth"] >= 1 and m["ui_capture"] == 1 and not r.advancing(),
           f"pause_depth={m['pause_depth']} ui_capture={m['ui_capture']}")
        # settings round trip: Graphics > Texture filtering to xBR
        tf0 = r.mh()["texture_filter"]
        nav.go(G, 2)
        for _ in range(3):
            if r.mh()["texture_filter"] == 3:
                break
            r.key("Right")
        time.sleep(0.5)
        ok("settings", r.mh()["texture_filter"] == 3 and saved_filter(a) == 3,
           f"texture_filter {tf0}->{r.mh()['texture_filter']} saved={settings_value(a, 'texture_filtering')}")
        # capture: Pause in menu off -> the game runs under the open menu while
        # the runtime keeps game input captured
        nav.go(S, 1)
        r.key("Left")
        time.sleep(0.5)
        m = r.mh()
        ok("capture", m["pause_depth"] == 0 and m["ui_capture"] == 1 and r.advancing(),
           f"menu open, pause off: game running, ui_capture={m['ui_capture']}")
        r.key("Right")   # Pause in menu back on
        nav.close()
        r.until(lambda: r.mh()["open"] == 0, 5, "menu closed")
        time.sleep(0.5)
        m = r.mh()
        ok("resume", m["pause_depth"] == 0 and m["ui_capture"] == 0 and r.advancing(),
           f"closed: running, ui_capture={m['ui_capture']}")
        # tray: Disc > Selected disc 2 > Insert
        nav.open()
        nav.go(D, 0)
        r.key("Right")
        r.key("Down")
        r.key("Return")
        try:
            r.until(lambda: r.mh()["disc_mounted"] == 2, 20, "disc 2 mounted")
        except SystemExit:
            r.q("present_shot", path=str(Path(a.dir) / "check-tray-fail.png")); time.sleep(1)
            raise
        ev = tray_last(r)
        ok("tray", ev.get("origin") == "menu" and ev.get("to") == 2 and ev.get("result") == "ok" and r.mh()["open"] == 0,
           f"last tray event {ev}")
        # restart-only: Graphics > Render thread, close -> quick relaunch
        rt0 = r.mh()["render_thread"]
        nav.open()
        nav.go(G, 3)
        r.key("Left" if rt0 else "Right")
        nav.close()
        r.until(lambda: "psxrecomp: relaunching" in r.text(), 15, "relaunch")
        time.sleep(3)
        r.boot()
        m = r.mh()
        ok("restart", m.get("attached") == 1 and m["render_thread"] == (0 if rt0 else 1) and m["texture_filter"] == 3,
           f"after relaunch render_thread {rt0}->{m['render_thread']}, texture_filter={m['texture_filter']}, provider re-attached")
    finally:
        r.stop()


def check_rl(a):
    # recomp-launcher: R4L_OVERLAY_SCRIPT drives the menu; the checks watch it.
    script = Path(a.dir) / "check-rl-script.txt"
    script.write_text("\n".join([
        "sleep 30", "key Escape", "wait 30", "sleep 6",          # open: pause check window
        "screen Graphics", "wait 10", "tap Texture filtering", "wait 10", "tap xBR", "wait 10", "sleep 5",
        "screen Disc setup", "wait 10", "tap disc:2", "wait 30", "sleep 8",  # tray (menu closes itself)
        "key Escape", "wait 30", "screen Graphics", "wait 10", "tap Render thread", "wait 10",
        "key Escape", "wait 20", ""]))
    r = Run(a, {"R4L_OVERLAY_SCRIPT": str(script)}, label=f"rt{a.render_thread}")
    try:
        r.boot(300)
        m = r.mh()
        ok("attach", m.get("attached") == 1 and m.get("provider") == "recomp-launcher",
           f"provider={m.get('provider')} default_selection={m.get('default_selection')} render_thread={m.get('render_thread')}")
        r.until(lambda: r.mh()["open"] == 1, 90, "menu open")
        time.sleep(1)
        m = r.mh()
        ok("pause", m["pause_depth"] >= 1 and m["ui_capture"] == 1 and not r.advancing(),
           f"pause_depth={m['pause_depth']} ui_capture={m['ui_capture']}")
        r.until(lambda: r.mh()["texture_filter"] == 3, 40, "texture filter xBR")
        time.sleep(1)
        ok("settings", saved_filter(a) == 3, f"saved={settings_value(a, 'texture_filtering')}")
        r.until(lambda: r.mh()["disc_mounted"] == 2, 60, "disc 2 mounted")
        ev = tray_last(r)
        ok("tray", ev.get("origin") == "menu" and ev.get("to") == 2 and ev.get("result") == "ok", f"last tray event {ev}")
        rt0 = r.mh()["render_thread"]
        r.until(lambda: "psxrecomp: relaunching" in r.text(), 90, "relaunch")
        time.sleep(3)
        r.boot(300)
        m = r.mh()
        ok("restart", m.get("attached") == 1 and m["render_thread"] == (0 if rt0 else 1) and m["texture_filter"] == 3,
           f"after relaunch render_thread {rt0}->{m['render_thread']}, texture_filter={m['texture_filter']}, provider re-attached")
    finally:
        r.stop()


def check_session_rui(a):
    """GL context recreation: Return to launcher tears the session down (the
    provider detaches with the context current), recomp-ui's launcher opens,
    PLAY starts a new session on a new context, and the provider attaches
    again and works."""
    r = Run(a, {"LNG_TEST_HIDDEN": "1", "LNG_SCRIPT": "wait:90;click:986,818;wait:100000"},
            label=f"session-rt{a.render_thread}")
    try:
        r.boot()
        m = r.mh()
        nav = RuiNav(r)
        nav.open()
        nav.go(1, 2)                 # Graphics > Texture filtering: xBR before leaving
        for _ in range(3):
            if r.mh()["texture_filter"] == 3:
                break
            r.key("Right")
        tf_before = r.mh()["texture_filter"]
        nav.go(2, 2)                 # System > Return to launcher (press twice)
        r.key("Return"); r.key("Return")
        r.until(lambda: "in-game menu -> frontend loop" in r.text(), 15, "frontend loop")
        r.until(lambda: r.text().count("in-game menu ready") >= 2, 90, "provider attached to the new session")
        r.boot(300)
        m2 = r.mh()
        nav = RuiNav(r)
        nav.open()
        r.until(lambda: r.mh()["open"] == 1, 5, "menu open in the new session")
        m3 = r.mh()
        nav.close()
        r.until(lambda: r.mh()["open"] == 0, 5, "menu closed")
        ok("session-filter", tf_before == 3 and m2["texture_filter"] == 3,
           f"xBR across Return to launcher -> PLAY: {tf_before} -> {m2['texture_filter']}")
        ok("session", m2.get("attached") == 1 and m3["pause_depth"] >= 1 and r.advancing(),
           f"render_thread={m.get('render_thread')}: detached, launcher, PLAY, re-attached on the new context; "
           f"menu opens (pause_depth={m3['pause_depth']}) and closes, game running")
    finally:
        r.stop()


def check_netplay(a):
    """Two local LAN rollback peers of the same build (no lobby). On the host:
    the menu opens without pausing (refused by the runtime), game input stays
    captured, both peers keep advancing; Esc goes to the menu, not to
    "leave the match"; a texture filter edit waits for the match end and
    never relaunches."""
    import copy
    base = {"PSX_NETPLAY": "1", "PSX_NET_MODE": "rollback", "PSX_NET_TRANSPORT": "lan",
            "PSX_NET_SLOTS": "2", "PSX_NET_SESSION_ID": "77", "PSX_DEBUG_FMV_QUIET": "0"}
    udp = 47950 + (a.port % 10) * 4
    e0 = dict(base, PSX_NET_SLOT="0", PSX_NET_BIND=f"127.0.0.1:{udp}")
    e1 = dict(base, PSX_NET_SLOT="1", PSX_NET_BIND=f"127.0.0.1:{udp + 1}", PSX_NET_PEER=f"127.0.0.1:{udp}")
    a1 = copy.copy(a); a1.dir = a.dir2; a1.port = a.port + 1
    r0 = Run(a, e0, label="net-host")
    time.sleep(1)
    r1 = Run(a1, e1, label="net-guest")
    try:
        for r in (r0, r1):
            if not r.up(90):
                raise SystemExit(f"FAIL peer did not answer; see {r.log}")
        r0.until(lambda: "lockstep armed" in r0.text() and "lockstep armed" in r1.text(), 120, "netplay armed")
        r0.until(lambda: r0.frame() > 600 and r1.frame() > 600, 120, "match running")
        m = r0.mh()
        if a.adapter == "rui":
            nav = RuiNav(r0)
            nav.open()
        else:
            r0.key("Escape")
        r0.until(lambda: r0.mh()["open"] == 1, 10, "menu open in the match")
        m = r0.mh()
        f0, f1 = r0.frame(), r1.frame(); time.sleep(2); g0, g1 = r0.frame(), r1.frame()
        ok("netplay-pause-refused", m["netplay"] == 1 and m["pause_depth"] == 0 and m["ui_capture"] == 1
           and g0 > f0 + 60 and g1 > f1 + 60,
           f"menu open online: pause_depth={m['pause_depth']} ui_capture={m['ui_capture']}, frames host {f0}->{g0} guest {f1}->{g1}")
        tf0 = m["texture_filter"]
        if a.adapter == "rui":
            nav.go(1, 2); r0.key("Right"); time.sleep(0.5)
            r0.key("Backspace")
        r0.key("Escape")          # menu back/close: must not leave the match
        r0.until(lambda: r0.mh()["open"] == 0, 10, "menu closed")
        time.sleep(3)
        alive = r0.p.poll() is None and r1.p.poll() is None
        ended = "netplay ended" in r0.text() or "netplay ended" in r1.text() or "netplay_escape" in r0.text()
        f0, f1 = r0.frame(), r1.frame(); time.sleep(2); g0, g1 = r0.frame(), r1.frame()
        relaunched = "relaunching" in r0.text()
        ok("netplay-capture", alive and not ended and g0 > f0 and g1 > f1 and not relaunched
           and r0.mh()["texture_filter"] == tf0,
           f"Esc went to the menu, match continues ({f0}->{g0}, {f1}->{g1}); texture filter edit waited "
           f"(live {r0.mh()['texture_filter']}), no relaunch")
        desync = any(w in r0.text() + r1.text() for w in ("DESYNC", "desync detected", "digest mismatch"))
        mh = [r.q("dispatch_stats").get("miss_total") for r in (r0, r1)]
        ok("netplay-clean", not desync and mh == [0, 0], f"misses {mh}, desync={desync}")
    finally:
        r0.stop(); r1.stop()


def started(r):
    return len(re.findall(r"psx_netplay: started", r.text()))


def check_lobby(a):
    """LAN lobby on loopback, two instances of one build: start a match from
    the lobby, open the menu during it (no pause, both advance), leave the
    match from the menu, and start a rematch from the lobby. The menu provider
    detaches and attaches with every session's GL context.
      rui: recomp-ui's launcher lobby (LNG_SCRIPT clicks), Return to launcher.
      rl:  recomp-launcher's in-game lobby (R4L_OVERLAY_SCRIPT), Quit game."""
    import copy
    a1 = copy.copy(a); a1.dir = a.dir2; a1.port = a.port + 1
    lan = 47980 + (a.port % 10) * 2
    if a.adapter == "rui":
        for d, port in ((a.dir, lan), (a.dir2, lan + 1)):
            nd = Path(d) / "saves" / "netplay"
            nd.mkdir(parents=True, exist_ok=True)
            (nd / "network settings").write_text(
                f"lobby_url=ws://netplay.retcomm.net:8765\npreferred_ip=127.0.0.1\npreferred_port={port}\n")
        lan_view = "wait:40;view:netplay_mode;wait:20;click:232,281;wait:30;click:548,499;wait:5;text:{n};wait:5;click:583,550;wait:20;"
        bs = "".join("key:Backspace;wait:3;" for _ in range(6))
        host = dict(LNG_TEST_HIDDEN="1", LNG_SCRIPT=lan_view.format(n="mihost") +
                    "click:119,818;wait:30;click:545,609;wait:900;click:970,818;wait:600;click:970,818;wait:100000")
        guest = dict(LNG_TEST_HIDDEN="1", LNG_SCRIPT=lan_view.format(n="miguest") +
                     f"click:979,818;wait:30;click:416,262;wait:5;{bs}text:{lan};wait:5;click:541,354;wait:100000")
        r0 = Run(a, host, label="lobby-host", launcher=True)
        time.sleep(6)
        r1 = Run(a1, guest, label="lobby-guest", launcher=True)
        args_ok = True
    else:
        hs = Path(a.dir) / "check-rl-lobby-host.txt"
        gs = Path(a.dir2) / "check-rl-lobby-guest.txt"
        hs.write_text("\n".join(["sleep 25", "key Escape", "wait 30", "screen Netplay", "wait 10", "tap LAN only",
                                  "wait 5", "tap Host", "sleep 50", "tap Start race", "sleep 40",
                                  "key Escape", "wait 30", "sleep 4", "key Escape", "wait 20",
                                  "key Escape", "wait 30", "tap nav:Quit game", "sleep 45",
                                  "key Escape", "wait 30", "screen Netplay", "wait 10", "tap Start race", "sleep 600", ""]))
        gs.write_text("\n".join(["sleep 32", "key Escape", "wait 30", "screen Netplay", "wait 10", "tap field:address",
                                  f"text 127.0.0.1:{lan}", "wait 5", "tap join:address", "sleep 15", "tap Ready up",
                                  "sleep 75", "sleep 25", "key Escape", "wait 30", "screen Netplay", "wait 10",
                                  "tap Ready up", "sleep 600", ""]))
        # Through the frontend loop (recomp-launcher boots straight into the
        # game from it), which is where the lobby is set up.
        r0 = Run(a, {"R4L_OVERLAY_SCRIPT": str(hs), "R4L_HOST_PORT": str(lan)}, label="lobby-host", launcher=True)
        time.sleep(2)
        r1 = Run(a1, {"R4L_OVERLAY_SCRIPT": str(gs), "R4L_HOST_PORT": str(lan + 1)}, label="lobby-guest", launcher=True)
    try:
        for r in (r0, r1):
            if not r.up(120):
                raise SystemExit(f"FAIL instance did not answer; see {r.log}")
        r0.until(lambda: started(r0) >= 1 and started(r1) >= 1, 240, "lobby match started")
        r0.until(lambda: r0.frame() > 300 and r1.frame() > 300, 120, "match running")
        ok("lobby-start", r0.mh().get("attached") == 1 and r0.mh()["netplay"] == 1,
           f"match from the lobby; provider attached in the match")
        # Menu during the match: no pause, both advance.
        if a.adapter == "rui":
            nav = RuiNav(r0); nav.open()
        r0.until(lambda: r0.mh()["open"] == 1, 60, "menu open in the match")
        m = r0.mh()
        f0, f1 = r0.frame(), r1.frame(); time.sleep(2); g0, g1 = r0.frame(), r1.frame()
        ok("lobby-menu-online", m["pause_depth"] == 0 and g0 > f0 + 60 and g1 > f1 + 60,
           f"menu open online: pause_depth={m['pause_depth']}, frames host {f0}->{g0} guest {f1}->{g1}")
        # Leave the match from the menu -> lobby; rematch.
        if a.adapter == "rui":
            nav.go(2, 2); r0.key("Return"); r0.key("Return")
        r0.until(lambda: "netplay ended" in r0.text(), 120, "match left from the menu")
        r0.until(lambda: started(r0) >= 2 and started(r1) >= 2, 240, "rematch started")
        r0.until(lambda: r0.frame() > 300 and r1.frame() > 300, 120, "rematch running")
        f0, f1 = r0.frame(), r1.frame(); time.sleep(2); g0, g1 = r0.frame(), r1.frame()
        attaches = r0.text().count("in-game menu ready")
        sess = re.findall(r"psx_netplay: started .*?session=(\d+)", r0.text())
        ok("lobby-rematch", g0 > f0 and g1 > f1 and r0.mh().get("attached") == 1 and r0.mh()["netplay"] == 1,
           f"rematch sessions {sess}, both advancing; provider attached {attaches}x across GL contexts")
        desync = any(w in r0.text() + r1.text() for w in ("DESYNC", "desync detected", "digest mismatch"))
        mh = [r.q("dispatch_stats").get("miss_total") for r in (r0, r1)]
        ok("lobby-clean", not desync and mh == [0, 0], f"misses {mh}, desync={desync}")
    finally:
        r0.stop(); r1.stop()


def check_none(a):
    """PSX_OVERLAY=0: no provider. recomp-ui builds keep #652's standalone disc
    menu on F10 (multi-disc); recomp-launcher builds have no menu at all."""
    r = Run(a, {"PSX_OVERLAY": "0"}, label="none")
    try:
        r.boot()
        m = r.mh()
        if a.adapter == "rui":
            r.key("F10")
            time.sleep(0.5)
            standalone = r.q("runtime_menu").get("open") is True
            r.key("F10")
            time.sleep(0.5)
            closed = r.q("runtime_menu").get("open") is False
            ok("none", m.get("attached") == 0 and standalone and closed and r.advancing(),
               f"attached={m.get('attached')}; F10 opened #652's standalone disc menu={standalone}, closed={closed}, game running")
        else:
            r.key("Escape"); r.key("F10")
            time.sleep(0.5)
            m2 = r.mh()
            ok("none", m.get("attached") == 0 and m2.get("open") == 0 and m2.get("pause_depth") == 0 and r.advancing(),
               f"attached={m.get('attached')}; Esc/F10 open nothing, game running")
    finally:
        r.stop()


def check_presets_rl(a):
    # The in-game Graphics page under R4's stacked presets: a pick, a governed
    # edit (Smooth motion) turning it Custom, the texture filter row below the
    # presets (xBR and back), and the Controls page's JogCon FFB toggle.
    script = Path(a.dir) / "check-rl-presets.txt"
    script.write_text("\n".join([
        "sleep 30", "key Escape", "wait 30", "sleep 4",
        "screen Graphics", "wait 10", "seen Ultra", "tap High", "wait 10", "sleep 4",
        "tap Smooth motion", "wait 10", "sleep 4",
        "find Texture filtering", "tap Texture filtering", "wait 10", "tap xBR", "wait 10", "sleep 4",
        "tap Texture filtering", "wait 10", "tap Nearest", "wait 10", "sleep 4",
        "top", "wait 5", "find High",
        "screen Controls", "wait 10", "tap Wheel force feedback (experimental)", "wait 10", "sleep 4",
        "screen Graphics", "wait 10"] + ["pad dpdown"] * 30 + ["wait 10", 
        "visible Skin"] + ["pad dpup"] * 30 + ["wait 10", "visible High",
        ""]))  # ends with the menu open: a closed menu stops stepping the script
    # Hidden-window runs never detect a preset; a saved one still applies.
    sp = Path(a.dir) / "settings.toml"
    t = sp.read_text() if sp.exists() else "[video]\n"
    t = re.sub(r"^quality_\w+\s*=.*\n", "", t, flags=re.M)
    sp.write_text(t.replace("[video]", '[video]\nquality_preset = "ultra"', 1))
    r = Run(a, {"R4L_OVERLAY_SCRIPT": str(script)}, label="presets")
    q = lambda: (settings_value(a, "quality_preset") or "").strip('"')
    try:
        r.boot(300)
        r.until(lambda: r.mh()["open"] == 1, 90, "menu open")
        r.until(lambda: q() == "high", 40, "preset High saved")
        ok("preset", True, "High picked and saved")
        r.until(lambda: q() == "custom", 40, "governed edit -> Custom")
        ok("custom", True, f"Smooth motion edit -> quality_preset=custom, frame_generation={settings_value(a, 'frame_generation')}")
        r.until(lambda: r.mh()["texture_filter"] == 3, 40, "texture filter xBR")
        time.sleep(1)
        ok("xbr", saved_filter(a) == 3, f"live 3, saved={settings_value(a, 'texture_filtering')}")
        r.until(lambda: r.mh()["texture_filter"] == 0, 40, "texture filter back to Nearest")
        time.sleep(1)
        ok("xbr-back", saved_filter(a) == 0, f"live 0, saved={settings_value(a, 'texture_filtering')}")
        r.until(lambda: settings_value(a, "jogcon_force_feedback") == "true", 40, "FFB saved on")
        ok("ffb", True, "jogcon_force_feedback = true")
        r.until(lambda: "overlay script done" in r.text(), 300, "script done")
        m = re.search(r"overlay script done, failures=(\d+)", r.text())
        ok("script", m and m.group(1) == "0", f"failures={m.group(1) if m else '?'} (wheel and gamepad nav reached every row)")
    finally:
        r.stop()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--adapter", choices=["rui", "rl"], required=True)
    ap.add_argument("--dir", required=True, help="game dir with r4-runtime and a two-disc game.toml")
    ap.add_argument("--disc", required=True)
    ap.add_argument("--home", required=True, help="HOME for the run (saves stay out of the real one)")
    ap.add_argument("--port", type=int, default=4951)
    ap.add_argument("--render-thread", type=int, choices=[0, 1], default=1)
    ap.add_argument("--dir2", help="second game dir (netplay guest)")
    ap.add_argument("--only", default="", help="comma list: provider,none")
    a = ap.parse_args()
    parts = a.only.split(",") if a.only else ["provider", "session", "netplay", "lobby", "none"]
    set_render_thread(a, a.render_thread)
    if "presets" in parts and a.adapter == "rl":
        check_presets_rl(a)
    if "provider" in parts:
        (check_rui if a.adapter == "rui" else check_rl)(a)
    if "netplay" in parts and a.dir2:
        check_netplay(a)
    if "lobby" in parts and a.dir2:
        check_lobby(a)
    if "session" in parts and a.adapter == "rui":
        check_session_rui(a)
    if "none" in parts:
        check_none(a)
    failed = [n for n, c, _ in RESULTS if not c]
    print(f"{a.adapter} render_thread={a.render_thread}: {len(RESULTS) - len(failed)}/{len(RESULTS)} passed"
          + (f"; failed: {', '.join(failed)}" if failed else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
