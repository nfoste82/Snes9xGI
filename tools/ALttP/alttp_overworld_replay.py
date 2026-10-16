#!/usr/bin/env python3
"""Produce occurrence-local replay geometry on an existing linked capture.

Exact visible map/graphics matching is checked by the C++ producer. This does
not upgrade base-ROM evidence to pinned runtime state or whole-area coverage.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ROLES = {'unknown': 0, 'floor': 1, 'wall': 2, 'corner': 3,
         'obstacle': 4, 'transition': 5, 'cap': 1}


def candidate_stream(candidate, with_support=False):
    rows = [f"{'OWREPLAY2' if with_support else 'OWREPLAY1'} {candidate['pin']['rom_sha256']} {len(candidate['placements'])}"]
    for p in candidate['placements']:
        solved = p['status'] == 'solved'
        fields = [p['x'], p['y'], p['word'], (p.get('hash') or '0').split(':')[-1],
                  1 if solved else 2 if p['status'] == 'conflict' else 0,
                  ROLES.get(p['role'], 0), p['base'] if solved else 0]
        if with_support:
            fields.extend([p.get('support') if p.get('support') is not None else -1,
                           p['layer'], p['surface']])
        if solved:
            if len(p['local']) != 64 or len(p['normals']) != 64:
                raise ValueError('incomplete solved placement')
            fields.extend(p['local'])
            for n in p['normals']:
                for v in n:
                    if not math.isfinite(v) or abs(v) > 1.0001:
                        raise ValueError('invalid contextual normal')
                    fields.append(max(0, min(255, round((v+1)*127.5))))
        rows.append(' '.join(map(str, fields)))
    return '\n'.join(rows)+'\n'


def standing_stream(manifest, capture_bytes):
    """Explicit user evidence, NOT inferred runtime state or a movement predicate.

    WRAM $22/$20 is the top-left Link anchor. tile_detect.c tabs 1/3 and
    Player_TileDetectNearby use x..x+15, y+8..y+23. This fixture samples the
    seven-pixel sole strip at x+8 +/-3, y+23; it does not assert a legal move.
    Both coordinate systems are required to prevent a filename/visual-bbox seed.
    """
    if (manifest.get('format') != 'ALTTP-STANDING-FIXTURE1' or
            manifest.get('actor') != 'Link' or manifest.get('grounded') is not True or
            manifest.get('grounded_source') != 'user' or not manifest.get('reference') or
            manifest.get('footprint_convention') != 'link-anchor-sole-x8pm3-y23' or
            manifest.get('expected_original_offset') != 0):
        raise ValueError('standing fixture needs explicit user grounded attestation and original offset zero')
    if hashlib.sha256(capture_bytes).hexdigest() != manifest.get('capture_sha256'):
        raise ValueError('standing fixture capture SHA256 mismatch (including reapplication)')
    def integer(v):
        if type(v) is not int or not -32767 <= v <= 32767:
            raise ValueError('invalid standing fixture integer')
        return v
    def pair(v):
        if not isinstance(v, list) or len(v) != 2:
            raise ValueError('standing fixture coordinate pair required')
        return [integer(x) for x in v]
    origin = pair(manifest['origin'])
    world = pair(manifest['link_anchor_map']); screen = pair(manifest['link_anchor_screen'])
    if world != [screen[k]+origin[k] for k in range(2)]:
        raise ValueError('standing fixture anchor alignment mismatch')
    slots = manifest['actor_slots']
    if not slots or len(set(slots)) != len(slots) or any(not 0 <= integer(s) < 128 for s in slots):
        raise ValueError('standing fixture explicit unique OAM slots required')
    samples = manifest['samples']
    expected = [[screen[0]+8+d, screen[1]+23] for d in range(-3, 4)]
    if [pair(s['screen']) for s in samples] != expected or any(
            pair(s['map']) != [s['screen'][k]+origin[k] for k in range(2)] for s in samples):
        raise ValueError('standing fixture sole footprint alignment mismatch')
    fields = ['OWSTAND1', manifest['capture_sha256'], integer(manifest['bg']), *origin,
              *world, *screen, integer(manifest['support']), integer(manifest['local_height']),
              integer(manifest['base']), 0, len(slots), *slots]
    for s in samples:
        fields.extend([*s['screen'], *s['map']])
    return ' '.join(map(str, fields))+'\n'


def generate(candidate_path, capture, output, producer, standing_fixture=None):
    candidate = json.loads(candidate_path.read_text())
    capture_digest = hashlib.sha256(capture.read_bytes()).hexdigest()
    if candidate['pin'].get('capture_sha256') not in (None, capture_digest):
        raise ValueError('capture-specific candidate SHA256 does not match target capture')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='overworld-replay-') as tmp:
        stream = Path(tmp)/'candidate.txt'
        stream.write_text(candidate_stream(candidate, standing_fixture is not None))
        args = [str(producer), str(capture), str(stream), str(output)]
        if standing_fixture is not None:
            manifest = json.loads(standing_fixture.read_text())
            # Producer rechecks the SHA itself before using the attestation.
            attestation = Path(tmp)/'standing.txt'
            attestation.write_text(standing_stream(manifest, capture.read_bytes()))
            args.append(str(attestation))
        run = subprocess.run(args,
                             text=True, capture_output=True, check=True)
    report = json.loads(run.stdout)
    report.update(source_capture=str(capture.resolve()), candidate=str(candidate_path.resolve()),
                   candidate_revision=candidate['pin']['metadata_revision'],
                   candidate_sha256=hashlib.sha256(candidate_path.read_bytes()).hexdigest(),
                   capture_sha256=capture_digest,
                   complete_area_ready=False,
                    provenance='Original Snes9x raster/links; offline occurrence geometry proposal; '+
                    ('partial capture-attested map state' if candidate['pin'].get('capture_sha256') else 'base-ROM map state'))
    if standing_fixture is not None:
        report['standing_fixture'] = dict(manifest, manifest_path=str(standing_fixture.resolve()))
    output.with_suffix('.review.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, default=ROOT/'build/remaster-geometry/overworld-castle-authored-v1.json')
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--producer', type=Path, required=True)
    p.add_argument('--standing-fixture', type=Path,
                   help='opt-in SHA-pinned user-attested original Link standing fixture JSON')
    a = p.parse_args()
    print(json.dumps(generate(a.candidate, a.capture, a.output, a.producer, a.standing_fixture), indent=2))
