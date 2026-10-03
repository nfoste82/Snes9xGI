#!/usr/bin/env python3
"""Inventory reference arrays, exact legacy signatures and full-room adjacency.

Never restores normals. Equality to a legacy output is evidence of compatibility,
not authorship proof; the parent must review backup restoration explicitly.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import tomllib

from alttp_atlas_profile import _semantic_wall_layers, _neutral_layers, _silhouette_layers
from alttp_room_geometry import RoomSnapshot, RoomProvenance
from alttp_tile_semantic_atlas import AnnotationStore, TileCatalog


def audit(source, backup, database, config, fixtures):
    profile = tomllib.loads(source.read_text())
    assets = {a['tile_hash']: a for a in profile['assets']}
    originals = {a['tile_hash']: a for a in tomllib.loads(backup.read_text())['assets']}
    refs = json.loads(config.read_text())['templates']
    semantics = AnnotationStore(Path('tools/alttp-tile-semantics-v1.json'), profile['game']['rom_sha256']).tiles
    catalog = TileCatalog(database)
    expanded = {}
    for key, annotation in semantics.items():
        for alias in catalog.semantic_group(key):
            expanded[alias] = annotation
    db = sqlite3.connect(f'file:{database}?mode=ro', uri=True)
    signatures = []
    for key, indices, families in db.execute(
            'SELECT d.content_id,d.indices,GROUP_CONCAT(DISTINCT s.source_kind) '
            'FROM decoded_tile d JOIN graphics_source s USING(content_id) GROUP BY d.content_id'):
        current = assets.get(key, {})
        if 'normal_xyz' not in current:
            continue
        annotation = expanded.get(key, {})
        if annotation.get('role') in ('wall', 'wall_corner') and not annotation.get('context_dependent') and annotation.get('corner_type') != 'unknown':
            legacy = _semantic_wall_layers(annotation)['normal_xyz']
            method = 'legacy_semantic_wall'
        elif annotation.get('role') == 'floor' and not annotation.get('context_dependent'):
            legacy = _neutral_layers()['normal_xyz']; method = 'legacy_floor'
        else:
            legacy = (_silhouette_layers(indices) if 'link' in families or 'sprite' in families
                      else _neutral_layers())['normal_xyz']
            method = 'legacy_authored_missing_layer_default'
        before = originals.get(key, {}).get('normal_xyz')
        if tuple(current['normal_xyz']) == tuple(legacy):
            signatures.append({'tile_hash': key, 'method': method,
                               'backup_normal_present': before is not None,
                               'backup_differs': before is not None and before != current['normal_xyz'],
                               'classification': 'legacy_signature_match_not_permission_to_overwrite'})
    db.close()
    ref_ids = {r['reference'] for r in refs}
    reference_data = {key: {'height_rows': [assets[key]['height'][y*8:y*8+8] for y in range(8)],
                           'normal_sha256': hashlib.sha256(bytes(assets[key]['normal_xyz'])).hexdigest(),
                           'backup_normal_present': 'normal_xyz' in originals.get(key, {})}
                      for key in sorted(ref_ids)}
    for key in reference_data:
        db = sqlite3.connect(f'file:{database}?mode=ro', uri=True)
        indices = db.execute('SELECT indices FROM decoded_tile WHERE content_id=?',(key,)).fetchone()[0]
        db.close()
        annotation = expanded.get(key,{})
        legacy = (_semantic_wall_layers(annotation) if annotation.get('role') in ('wall','wall_corner')
                  and annotation.get('corner_type') != 'unknown' else None)
        reference_data[key].update({
            'normal_rows': [[assets[key]['normal_xyz'][p*3:p*3+3]
                             for p in range(y*8,y*8+8)] for y in range(8)],
            'exact_up_pixel_mask': hex(sum(1 << p for p in range(64)
                if assets[key]['normal_xyz'][p*3:p*3+3] == [128,128,255])),
            'decoded_index_rows': [list(indices[y*8:y*8+8]) for y in range(8)],
            'legacy_semantic_height_match': legacy is not None and tuple(assets[key]['height']) == legacy['height'],
            'legacy_semantic_normal_match': legacy is not None and tuple(assets[key]['normal_xyz']) == legacy['normal_xyz'],
            'backup_asset_present': key in originals,
            'backup_height_span': [min(originals[key]['height']),max(originals[key]['height'])]
                                  if 'height' in originals.get(key,{}) else None,
            'current_height_span': [min(assets[key]['height']),max(assets[key]['height'])],
            'backup_height_equal': originals.get(key,{}).get('height') == assets[key]['height'],
            'authorship_conclusion': 'Signature equality cannot distinguish deliberate approval from regeneration; no edit history attestation.'})
    placements = []
    for room in (0x55, 0x60, 0x61):
        path = fixtures / f'room-{room:03x}-door-input.raw'
        snapshot = RoomSnapshot.read(path)
        provenance = RoomProvenance.read(Path(str(path) + '.prov'), snapshot)
        for cell in snapshot.cells[1]:
            if cell.tile_hash not in ref_ids:
                continue
            neighbors = []
            for dx, dy in ((-1,0),(1,0),(0,-1),(0,1)):
                x,y = cell.x+dx,cell.y+dy
                if 0 <= x < 64 and 0 <= y < 64:
                    other = snapshot.cell(1,x,y)
                    neighbors.append({'xy': [x,y], 'tile_hash': other.tile_hash,
                                      'hflip': other.hflip, 'vflip': other.vflip,
                                      'writer': provenance.owner(1,x,y)})
            placements.append({'room': room, 'layer': 1, 'xy': [cell.x,cell.y],
                               'tile_hash': cell.tile_hash, 'hflip': cell.hflip, 'vflip': cell.vflip,
                               'writer': provenance.owner(1,cell.x,cell.y), 'neighbors': neighbors})
    diagonal_ids = {r['reference'] for r in refs if r['id'].startswith('diagonal-')}
    diagonal_links = {}
    for cell in placements:
        if cell['tile_hash'] not in diagonal_ids:
            continue
        for neighbor in cell['neighbors']:
            if neighbor['tile_hash'] not in diagonal_ids:
                continue
            key = (cell['tile_hash'],neighbor['tile_hash'],
                   neighbor['xy'][0]-cell['xy'][0],neighbor['xy'][1]-cell['xy'][1],
                   cell['writer'] == neighbor['writer'])
            diagonal_links[key] = diagonal_links.get(key,0)+1
    return {'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'backup_sha256': hashlib.sha256(backup.read_bytes()).hexdigest(),
            'reference_arrays': reference_data, 'placements': placements,
            'diagonal_adjacency_counts': [dict(source=k[0],target=k[1],delta=list(k[2:4]),
                                              same_writer=k[4],count=v)
                                        for k,v in sorted(diagonal_links.items())],
            'legacy_signature_matches': signatures,
            'backup_normals_changed': [key for key,a in originals.items() if 'normal_xyz' in a and
                                       a['normal_xyz'] != assets.get(key,{}).get('normal_xyz')],
            'restoration_policy': 'No writes. Legacy equality is not provenance. Review backup normal differences; restore only normal_xyz with explicit owner approval.'}


def write_diagonal_preview(report, output):
    """Diagnostic canonical indices and current samples; not a proposed floor mask."""
    import struct
    import zlib
    from alttp_room_geometry import _png_chunk
    # Dependency-free raster: numbered cells, plus a labeled SVG companion.
    class Canvas:
        def __init__(self):
            self.pixels = bytearray([255]) * (780*520*3)
            self.svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="780" height="520"><rect width="100%" height="100%" fill="white"/>']
        def rectangle(self, box, fill, outline):
            color = tuple(bytes.fromhex(fill[1:])) if isinstance(fill,str) else fill
            self.svg.append(f'<rect x="{box[0]}" y="{box[1]}" width="{box[2]-box[0]+1}" height="{box[3]-box[1]+1}" fill="rgb{color}" stroke="{outline}"/>')
            for y in range(box[1],box[3]+1):
                for x in range(box[0],box[2]+1):
                    p = (y*780+x)*3
                    self.pixels[p:p+3] = bytes(color)
        def text(self, xy, text, fill):
            self.svg.append(f'<text x="{xy[0]}" y="{xy[1]+10}" font-family="monospace" font-size="11" fill="{fill}">{text}</text>')
        def save(self,path):
            rows = b''.join(b'\0'+self.pixels[y*780*3:(y+1)*780*3] for y in range(520))
            path.write_bytes(b'\x89PNG\r\n\x1a\n'+_png_chunk(b'IHDR',struct.pack('>IIBBBBB',780,520,8,2,0,0,0))+
                             _png_chunk(b'IDAT',zlib.compress(rows))+_png_chunk(b'IEND',b''))
            path.with_suffix('.svg').write_text(''.join(self.svg)+ '</svg>')
    palette = ['#10110f','#36382f','#555747','#74765f','#939579','#b2b894','#d2d7ad','#f0ebcb',
               '#3e5355','#55777a','#6f9da0','#92c5c6','#5d4936','#85633e','#b68b4f','#e1bd70']
    image = draw = Canvas()
    draw.text((12,8),'Canonical 8x8 samples (no flips); artwork diagnostic palette, NOT gameplay RGB',fill='black')
    for column,(label,key) in enumerate([
            ('Upper - unresolved','v1:4bpp:503ab068dfa941d3'),
            ('Middle - user authorized','v1:4bpp:3bc2b767d36c52c3'),
            ('Bottom/floor - unresolved','v1:4bpp:31aba23efb31666b')]):
        data = report['reference_arrays'][key]
        left = 12+column*256
        draw.text((left,30),label,fill='black')
        draw.text((left,46),key,fill='black')
        for y in range(8):
            for x in range(8):
                value = data['decoded_index_rows'][y][x]
                box = (left+x*28,68+y*28,left+(x+1)*28-1,68+(y+1)*28-1)
                draw.rectangle(box,fill=palette[value],outline='#cccccc')
                draw.text((box[0]+8,box[1]+8),str(value),fill='white' if value<4 else 'black')
                height = data['height_rows'][y][x]
                box = (left+x*28,312+y*23,left+(x+1)*28-1,312+(y+1)*23-1)
                draw.rectangle(box,fill=(max(0,255-height*24),max(0,255-height*16),255),outline='#999999')
                draw.text((box[0]+9,box[1]+5),str(height),fill='black')
        draw.text((left,296),'CURRENT local height (not room base)',fill='black')
    draw.text((12,504),'x: left to right 0..7; y: top to bottom 0..7. Bottom heights are NOT an accepted floor mask.',fill='black')
    image.save(output)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=Path('alttp-profile.toml'))
    p.add_argument('--backup', type=Path, default=Path('alttp-profile.before-heights.toml'))
    p.add_argument('--atlas', type=Path, required=True)
    p.add_argument('--templates', type=Path, default=Path('tools/alttp-reference-wall-templates-v1.json'))
    p.add_argument('--fixtures', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--preview', type=Path, help='optional PNG canonical diagonal artwork/height sample sheet')
    args = p.parse_args()
    if args.output.resolve() in {args.source.resolve(), args.backup.resolve(), args.atlas.resolve(), args.templates.resolve()}:
        p.error('report must be distinct from inputs')
    result = audit(args.source,args.backup,args.atlas,args.templates,args.fixtures)
    args.output.write_text(json.dumps(result,sort_keys=True,indent=2)+'\n')
    if args.preview:
        write_diagonal_preview(result,args.preview)
    print(json.dumps({'backup_normals_changed': len(result['backup_normals_changed']),
                      'legacy_signature_matches': len(result['legacy_signature_matches']),
                      'reference_placements': len(result['placements'])}))
