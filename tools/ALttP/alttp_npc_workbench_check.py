"""Audit a generated NPC manifest and exercise its export with the runtime parser."""
import argparse
import json
from pathlib import Path
import subprocess
import tomllib
import struct
import zlib

from alttp_npc_workbench import merge_profile, silhouette_defaults, validate_layers, atomic_write
from alttp_atlas_graphics import tile_content_id


def contact_sheet(output, manifest):
    actors = list({a['name']: a for a in reversed(manifest['actors'])}.values())
    actors.reverse()
    width, height = 640, ((len(actors)+7)//8)*100
    rgb = bytearray([30, 33, 40] * width * height)
    for index, actor in enumerate(actors):
        row, col = divmod(index, 8)
        for pose in actor['poses'][:1]:
            x0, y0, x1, y1 = pose['bounds']
            for part in reversed(pose['parts']):
                for p, ink in enumerate(part['indices']):
                    if not ink:
                        continue
                    x = col*80 + max(0, (80-2*(x1-x0))//2) + 2*(part['x']-x0+(7-p%8 if part['hflip'] else p%8))
                    y = row*100 + 4 + 2*(part['y']-y0+(7-p//8 if part['vflip'] else p//8))
                    c = part['colors'][p]
                    color = bytes(round(((c >> shift) & 31)*255/31) for shift in (0, 5, 10))
                    for dy in (0, 1):
                        for dx in (0, 1):
                            if 0 <= x+dx < width and 0 <= y+dy < height:
                                at = ((y+dy)*width+x+dx)*3
                                rgb[at:at+3] = color
    def chunk(kind, payload):
        return struct.pack('>I', len(payload))+kind+payload+struct.pack('>I', zlib.crc32(kind+payload))
    scanlines = b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(scanlines))+chunk(b'IEND', b'')
    (output / 'contact-sheet.png').write_bytes(png)
    atomic_write(output / 'contact-sheet-index.json', json.dumps([dict(cell=i, name=a['name'], context=a['id']) for i,a in enumerate(actors)], indent=2))


def check(output, profile):
    manifest = json.loads((output / 'poses.json').read_text())
    contact_sheet(output, manifest)
    edits, conflicting, poses, parts = {}, set(), 0, 0
    bg = set(manifest['background_shared_hashes'])
    for actor in manifest['actors']:
        for pose in actor['poses']:
            poses += 1
            for part in pose['parts']:
                parts += 1
                assert part['tile_hash'] == tile_content_id(bytes(part['indices']), 4)
                assert len(part['colors']) == 64
                assert any(part['indices'])
            if actor['graphics_inherited']:
                continue
            for key, proposal in silhouette_defaults(pose).items():
                if key in bg:
                    continue
                layers = proposal['layers']
                validate_layers(layers)
                if proposal['conflict'] or key in edits and edits[key] != layers:
                    conflicting.add(key)
                edits[key] = layers
    for key in conflicting:
        edits[key].pop('normal_xyz', None)
    before = profile.read_text()
    candidate, count = merge_profile(before, edits)
    target = output / 'npc-profile-check.toml'
    atomic_write(target, candidate)
    subprocess.run([str(output / 'alttp-profile-validate'), str(target)], check=True)
    old, new = tomllib.loads(before), tomllib.loads(candidate)
    old_assets = {a['tile_hash']: a for a in old['assets']}
    new_assets = {a['tile_hash']: a for a in new['assets']}
    for key, asset in old_assets.items():
        for field, value in asset.items():
            assert new_assets[key][field] == value, (key, field)
    assert profile.read_text() == before
    assert merge_profile(candidate, edits) == (candidate, 0)
    result = dict(actors=len(manifest['actors']), character_families=len({a['name'] for a in manifest['actors']}),
                  missing_models=manifest['coverage'].get('missing_models', []), poses=poses, tile_parts=parts,
                  suggested_tiles=len(edits), normal_conflicts=len(conflicting),
                  added_layers=count, original_assets_preserved=len(old_assets),
                  candidate=str(target), status='passed')
    atomic_write(output / 'check-report.json', json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', type=Path, required=True)
    args = parser.parse_args()
    check(args.output.resolve(), args.profile.resolve())
