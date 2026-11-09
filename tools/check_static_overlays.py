#!/usr/bin/env python3
"""Check that the committed native overlay output matches its original-disc audit."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check(root=ROOT):
    profile = json.loads((root / 'aot/overlays.json').read_text())
    audit = json.loads((root / 'generated/AOT_STATIC_AUDIT.json').read_text())
    if hashlib.sha256(json.dumps(profile, sort_keys=True).encode()).hexdigest() != audit['profile_sha256']:
        raise ValueError('Overlay profile changed: regenerate with tools/regen.sh')
    if sha(root / 'game.toml') != audit['generation_inputs']['game_toml_sha256']:
        raise ValueError('Game config changed since overlay generation')
    if (not audit.get('static') or not audit.get('all_guards_match_known_input_bytes')
            or audit['recipe_count'] != profile['expected_records']
            or len(audit['image_coverage']) != profile['expected_records']):
        raise ValueError('Incomplete original-disc overlay audit')
    if not all(item['native_entries'] > 0 for item in audit['image_coverage']):
        raise ValueError('An overlay image has no native entries')
    actual = {p.name for p in (root / 'generated').glob('overlays_static*.c')}
    if actual != set(audit['files']):
        raise ValueError('Missing or extra generated overlay units')
    for name, expected in audit['files'].items():
        if sha(root / 'generated' / name) != expected:
            raise ValueError(f'Generated overlay differs from audit: {name}')
    cps = 'psx_cps_mark_game' in (root / 'generated/SLUS_007.97_dispatch.c').read_text()
    if cps != audit['generation_inputs']['cps']:
        raise ValueError('Overlay and game continuation conventions differ')
    return audit


if __name__ == '__main__':
    receipt = check()
    print(f"Verified {receipt['recipe_count']} images, {receipt['published_variants']} guarded native variants")
