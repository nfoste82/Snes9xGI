#!/usr/bin/env python3
"""Run the room validator's directional seam gate on packet-proven placed pixels.

This narrow visible-surface proof is not a complete-room coverage substitute.
The C++ exact005 regression emits actual captured offsets/local sources, not
solver bases, after validating every observed pixel's producer and mapping.
"""
import argparse
import hashlib
import json
from pathlib import Path
from types import SimpleNamespace
import tomllib

from alttp_validate_room_heights import directional_seams


def audit(data, profile):
    samples = data['samples']
    results = {}
    for version in ('before','after'):
        records = [r for r in samples if r['version']==version]
        cells = {(r['layer'],r['x'],r['y']):SimpleNamespace(tile_hash=r['tile_hash'],
                  hflip=r['hflip'],vflip=r['vflip']) for r in records}
        snapshot = SimpleNamespace(cell=lambda layer,x,y:cells[layer,x,y])
        attributes = [bytearray([3]*4096),bytearray([3]*4096)]
        for r in records:
            attributes[r['plane']][r['y']*64+r['x']] = r['attribute']
        report = directional_seams(records,snapshot,attributes,profile)
        # Only the observed along-face wall/wall and wall/floor contacts are
        # this chain's denominator; north/south pixels were not exported.
        axial = [e for e in report['edges'] if e['direction'] in ('east','west') and
                 41<=e['x']<=43 and 41<=e['neighbor_xy'][0]<=44]
        results[version] = {'directional_inventory':report,
                           'observed_chain_edges':axial,
                           'observed_chain_edge_denominator':len(axial),
                           'conflicts':sum(e['status']=='effective_seam_conflict' for e in axial),
                           'continuous':sum(e['status']=='continuous' for e in axial),
                           'excluded':sum(e['status']=='independent_support_or_occluder' for e in axial)}
    return results


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chain',type=Path,required=True)
    parser.add_argument('--profile',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    data=json.loads(args.chain.read_text())
    if data.get('format')!='alttp-exact-capture-chain-v1':
        parser.error('expected exact capture chain')
    results=audit(data,tomllib.loads(args.profile.read_text()))
    # This regression must reject the old visible packet even when a separately
    # calculated solver map is correct. Merely testing after cannot pass.
    if not (results['before']['conflicts']==8 and results['before']['continuous']==2 and
            results['after']['conflicts']==0 and results['after']['continuous']==10 and
            results['before']['excluded']==results['after']['excluded']==0):
        parser.error('exact005 before/after directional regression failed')
    report={'format':'alttp-exact-capture-directional-validation-v1','room':0x60,
            'scope':'observed_BG1_chain_not_complete_room_approval','results':results,
            'inputs':{'chain':str(args.chain),'profile':str(args.profile),
                      'chain_sha256':hashlib.sha256(args.chain.read_bytes()).hexdigest(),
                      'profile_sha256':hashlib.sha256(args.profile.read_bytes()).hexdigest()}}
    args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    for version,result in results.items():
        print(f"{version}: edges={result['observed_chain_edge_denominator']} "
              f"conflicts={result['conflicts']} continuous={result['continuous']} excluded={result['excluded']}")
        for e in result['observed_chain_edges']:
            source=next(r for r in data['samples'] if r['version']==version and r['layer']==e['layer'] and
                        r['plane']==e['plane'] and r['x']==e['x'] and r['y']==e['y'])
            print(f"  BG{e['layer']+1} plane{e['plane']} ({e['x']},{e['y']}) {e['direction']} "
                  f"{e['neighbor_xy']}: attr={source['attribute']} {e['status']} steps={e.get('actual_steps')}")


if __name__=='__main__':
    main()
