#!/usr/bin/env python3
"""Apply the owner-reviewed 4K/8x profile to a built runtime, with backups."""
import argparse
import json
from pathlib import Path
import re
import shutil
import tomllib

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "docs/recovery/approved-4k.toml"


def scalar(value):
    if isinstance(value, bool):
        return str(value).lower()
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    raise ValueError("Unsupported saved setting type")


def state_text(state):
    # Preserve all existing selections, source bindings and quick-mod choices.
    if state.get("format_version") != 2 or set(state) - {
        "format_version", "quick_mods", "sources", "package", "feature"
    }:
        raise ValueError("Use a fresh build directory: unsupported saved mod-state schema")
    lines = ["format_version = 2"]
    for table in ("quick_mods", "sources"):
        if state.get(table):
            lines += ["", f"[{table}]"]
            lines += [f"{json.dumps(k)} = {scalar(v)}" for k, v in state[table].items()]
    for table in ("package", "feature"):
        for row in state.get(table, []):
            lines += ["", f"[[{table}]]"]
            lines += [f"{k} = {scalar(v)}" for k, v in row.items() if not isinstance(v, dict)]
            for key, values in row.items():
                if isinstance(values, dict):
                    lines += [f"[{table}.{key}]"]
                    lines += [f"{json.dumps(k)} = {scalar(v)}" for k, v in values.items()]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-dir", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    runtime = args.runtime_dir.resolve()
    if not any((runtime / exe).is_file() for exe in ("r4-runtime", "r4-runtime.exe")):
        parser.error("--runtime-dir must contain the built r4-runtime executable")
    game = runtime / "game.toml"
    mods = runtime / "mods"
    preset = tomllib.loads(PROFILE.read_text(encoding="utf-8"))
    state_file = mods / "state.toml"
    state = tomllib.loads(state_file.read_text(encoding="utf-8")) if state_file.exists() else {"format_version": 2}
    for desired in preset["feature"]:
        manifests = sorted((mods / "bundled" / desired["package_id"]).glob("*/manifest.toml"))
        if not manifests:
            parser.error(f"Missing staged package: {desired['package_id']}; build psx-runtime first")
        manifest = tomllib.loads(manifests[-1].read_text(encoding="utf-8"))
        if desired["id"] not in {f["id"] for f in manifest.get("feature", [])}:
            parser.error(f"Incompatible staged package: {desired['package_id']}")
        rows = state.setdefault("feature", [])
        matches = [f for f in rows if (f["package_id"], f["id"]) == (desired["package_id"], desired["id"])]
        if len(matches) > 1:
            parser.error("Duplicate saved feature selections")
        if matches:
            row = matches[0]
            row["enabled"] = desired["enabled"]
            if desired.get("values"):
                row.setdefault("values", {}).update(desired["values"])
        else:
            rows.append(desired.copy())
    saved_state = state_text(state)
    original = game.read_text(encoding="utf-8")
    video = re.search(r"(?ms)^\[video\][^\n]*\n.*?(?=^\[|\Z)", original)
    if not video:
        parser.error("Built game.toml has no [video] section")
    section = video.group()
    for key, value in preset["video"].items():
        line = f"{key} = {scalar(value)}"
        pattern = rf"(?m)^{re.escape(key)}\s*=.*$"
        section = re.sub(pattern, lambda _: line, section) if re.search(pattern, section) else section + line + "\n"
    updated = original[:video.start()] + section + original[video.end():]
    tomllib.loads(updated)
    tomllib.loads(saved_state)
    for path in (game, state_file):
        backup = path.with_name(path.name + ".before-recovery-4k")
        if path.exists() and not backup.exists():
            shutil.copy2(path, backup)
    game.write_text(updated, encoding="utf-8")
    state_file.write_text(saved_state, encoding="utf-8")
    print(f"Applied 4K + 8x anisotropic filtering to {runtime}")
    print("Other saved mod choices and native saves preserved; existing settings.toml retained.")
    print("Launch tools/run_r4.sh (macOS/MSYS2) or tools/run_r4.cmd (Windows).")


if __name__ == "__main__":
    main()
