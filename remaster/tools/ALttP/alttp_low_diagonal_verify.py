"""Audit intrinsic LOW authoring, exact byte preservation and capture topology."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct
import tomllib

from alttp_diagonal_sets import REFERENCES, ROLES, transfer, transformed
from alttp_room_geometry import RoomSnapshot


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--profile', type=Path, required=True)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--corpus', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    report = json.loads(a.report.read_text())
    text = a.profile.read_text()
    assets = {v['tile_hash']:v for v in tomllib.loads(text)['assets']}
    updates = report['proposed_updates']
    # This task added absent assets and filled only two absent fields on the
    # existing role-only foot. Reconstruct and pin the exact pre-task bytes.
    def strip(match):
        block = match.group()
        key = re.search(r'(?m)^tile_hash\s*=\s*"([^"]+)"',block)
        if not key or key[1] not in updates:
            return block
        if key[1] == 'v1:4bpp:4952263cea5a838f':
            return re.sub(r'(?m)^(height|normal_xyz) = .*\n','',block)
        return ''
    baseline = re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',strip,text)
    assert hashlib.sha256(baseline.encode()).hexdigest() == report['profile_sha256'], 'unrelated bytes changed'
    (a.output/'before-profile.toml').write_text(baseline)
    for key, expected in updates.items():
        for field,value in expected.items():
            assert assets[key][field] == value,(key,field,len(assets[key][field]),len(value))
        assert 'occlusion' not in assets[key]
    prefix = str(a.capture)
    state = json.loads(Path(prefix+'.dungeon.json').read_text())
    blob = Path(prefix+'.dungeon-input.bin').read_bytes()
    pinned = Path(prefix+'.profile.toml').read_bytes()
    fnv = 14695981039346656037
    for byte in pinned:
        fnv = ((fnv^byte)*1099511628211)&((1<<64)-1)
    assert f'{fnv:x}' == state['profile_serialized_fnv1a64']
    assert state['publishable_inputs'] and len(blob)==90120 and blob[:8]==b'ALTPDI1\0'
    maps, vram = blob[8:16392], blob[24584:]
    mismatches=[]
    for layer in range(2):
        for y in range(64):
            for x in range(64):
                addr=(state['ppu_map_base'][layer]+(y//32)*2048+(x//32)*1024+(y%32)*32+x%32)&32767
                pos=(8192 if layer==0 else 0)+2*(y*64+x)
                if maps[pos:pos+2] != vram[addr*2:addr*2+2]:
                    mismatches.append([layer,x,y])
    assert not mismatches
    raw=a.corpus/'room-061-entrance-4.raw'
    snapshot=RoomSnapshot.read(raw)
    # Match all live words, not merely the southwest visible viewport.
    assert all(c.word==struct.unpack_from('<H',maps,(8192 if c.layer==0 else 0)+2*(c.y*64+c.x))[0]
               for layer in snapshot.cells for c in layer)
    old={v['tile_hash']:v for v in tomllib.loads(pinned.decode())['assets']}
    southwest=[]
    for assembly in report['assemblies']:
        if assembly['room']==97 and assembly['family']=='low':
            for cell in assembly['placements']:
                if cell['x']<32 and cell['tile_hash'] in updates:
                    key=cell['tile_hash']
                    southwest.append(dict(**cell,writer=assembly['writer'],handler=assembly['handler'],
                        before={f:old.get(key,{}).get(f) for f in ('height','normal_xyz','occlusion')},
                        after=updates[key],displayed_height=transformed(assets[key]['height'],hflip=cell['hflip'],vflip=cell['vflip'])))
    loaded_low=defaultdict(set)
    for record in report['loaded_templates']:
        if record['handler']>20:
            for band in record['bands']:
                if band['band']!='trim': loaded_low[band['tile_hash']].add(band['band'])
    print('LOADED WITHOUT PROVED PLACEMENTS',{k:sorted(v) for k,v in loaded_low.items() if k not in report['decisions']})
    print('UPDATED',[(k,report['decisions'][k]['bands'],report['decisions'][k]['usages']) for k in updates])
    result=dict(verdict='reviewable but incomplete',approval=False,profile_sha256=hashlib.sha256(text.encode()).hexdigest(),
        before_profile_sha256=report['profile_sha256'],preservation='byte exact except authorized additions',
        capture_pins={suffix:hashlib.sha256(Path(prefix+suffix).read_bytes()).hexdigest()
                      for suffix in ('','.profile.toml','.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin')},
        capture_state=state,wram_ppu_mismatches=mismatches,complete_live_renderer_word_match=True,
        southwest=southwest,updated_ids=list(updates),
        updated_usage_counts=dict(sum((Counter(report['decisions'][k]['usages']) for k in updates),Counter())),
        updated_rooms=sorted({r['room'] for r in report['assemblies'] if any(c['tile_hash'] in updates for c in r['placements'])}),
        loaded_without_placements={k:sorted(v) for k,v in loaded_low.items() if k not in report['decisions']})
    (a.output/'authoring-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: arrays, byte preservation, pinned capture, all8192 live/renderer words; southwest',len(southwest))
    live=a.output/'live029'
    if (live/'live-runtime.json').exists():
        runtime=json.loads((live/'live-runtime.json').read_text())
        print('LIVE RUNTIME', {k:v for k,v in runtime.items() if not isinstance(v,(list,dict))})
        print('UPDATED ROOMS',result['updated_rooms'])
        byxy={(p['x'],p['y']):p for p in runtime.get('structural_placements',[])}
        print('SW BASES',[(c['x'],c['y'],byxy.get((c['x'],c['y']))) for c in southwest if c['layer']==1])
        graph_path=Path('build/remaster-geometry/surface-audit-room-local/room-061-doorway-graph.json')
        graph=json.loads(graph_path.read_text())
        attr=Path(str(live/'live-solver-input.raw')+'.attr')
        assert graph['attribute_sha256']==hashlib.sha256(attr.read_bytes()).hexdigest()
        pin=dict(room_id=state['room'],plane=state['plane'],collision_mode=state['collision_mode'],
                 link_xy=state['link_xy'],live_capture=prefix,missing_transition_playback=True)
        graph['source_graph_state']=graph['state']
        graph['state']=pin
        graph['elevation']={}
        graph['live_repin_evidence']='all collision arrays identical; exact-state transition parity not approved'
        (live/'live-graph.json').write_text(json.dumps(graph,indent=2)+'\n')
        (live/'live-state.json').write_text(json.dumps(pin,indent=2)+'\n')


if __name__=='__main__':
    main()
