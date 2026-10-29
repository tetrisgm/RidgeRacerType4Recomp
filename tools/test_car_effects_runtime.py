#!/usr/bin/env python3
"""Verify enabled Car Effects actually draws in an existing hidden race.

Start the runtime with Car Effects enabled and load a race before running.
This catches session activation dropping the shared HUD callback even when
feature resolution and race-state sampling still report success.
"""
import argparse
import json
import time
from dbg import cmd


def verify(port=4797, seconds=2.0):
    def counters():
        reply = cmd({"cmd": "mod_counters"}, port=port)
        assert reply.get("ok"), reply
        return {c["name"]: c["count"] for c in reply["counters"]}
    before = counters()
    time.sleep(seconds)
    after = counters()
    draws = after.get("r4.effects.draw", 0) - before.get("r4.effects.draw", 0)
    views = after.get("r4.race.view", 0) - before.get("r4.race.view", 0)
    assert views > 0, "No race-state samples: load an unpaused race"
    assert draws > 0, "Car Effects is not drawing; enabled configuration alone is insufficient"
    uniforms = cmd({"cmd": "post_shader"}, port=port)["uniforms"]
    assert uniforms["u_race"][3] > 0, "Car Effects did not consume live race metadata"
    return {"race_samples": views, "effect_draws": draws, "uniforms": uniforms}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4797)
    args = parser.parse_args()
    print(json.dumps(verify(args.port), indent=2))
