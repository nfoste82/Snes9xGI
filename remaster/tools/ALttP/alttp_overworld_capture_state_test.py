#!/usr/bin/env python3
"""Real capture evidence guards and effective aperture regeneration."""
import argparse
import copy
import json
from pathlib import Path
import unittest
from collections import Counter
from alttp_atlas import Rom
from alttp_overworld_capture_state import export_manifest,generate_capture_state

ARGS = None

class CaptureState(unittest.TestCase):
    def test_verified_source_and_guards(self):
        if ARGS is None:
            self.skipTest('supply --probe and --core for real capture evidence checks')
        root=Path(__file__).resolve().parents[3]/'build/remaster-geometry'
        base=json.loads((root/'overworld-castle-authored-v1.json').read_text())
        capture=Path.home()/'Library/Application Support/Snes9x/Remaster/frame_overworld_x0771_y0690.s9xrmf'
        rom=Rom(Path.home()/'Downloads/Zelda_ALTTP.zip')
        m=export_manifest(base,capture,ARGS.probe,[1783,1587])
        self.assertEqual(sum(len(a['changes']) for a in m['assemblies']),8)
        c,_=generate_capture_state(base,m,capture,ARGS.probe,ARGS.core,rom)
        self.assertEqual(c['coverage']['by_behavior'],dict(Counter(p['behavior'] for p in c['placements'])))
        for p in c['placements']:
            if p['x'] in (229,230) and p['y'] in (198,199):
                self.assertEqual((p['base'],p['local'],p['support_eligible']),(32,[0]*64,True))
        for mutation in (lambda t:t.update(source_capture_sha256='0'*64),
                         lambda t:t['assemblies'][0]['changes'][0].update(word=0),
                         lambda t:t.update(base_provenance_sha256='stale'),
                         lambda t:t['assemblies'][0]['changes'].pop()):
            bad=copy.deepcopy(m);mutation(bad)
            with self.assertRaises(ValueError):generate_capture_state(base,bad,capture,ARGS.probe,ARGS.core,rom)
        with self.assertRaises(ValueError):export_manifest(base,capture,ARGS.probe,[1784,1587])


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--core',type=Path,required=True)
    ARGS,rest=p.parse_known_args();unittest.main(argv=[__file__,*rest])
