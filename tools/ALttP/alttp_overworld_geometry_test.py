#!/usr/bin/env python3
"""Portable offline contract regressions; optional --core exercises C++ bridge."""
import argparse
import copy
import json
from pathlib import Path
import unittest

import alttp_overworld_geometry as ow

CORE = None


def placement(x=0, surface=0):
    return dict(x=x, y=0, layer=0, surface=surface, quadrant=0, map32=2,
                map32_member=0, map16=10, map16_member=0, word=271, writer=0,
                raw_collision=0, collision=0, behavior="normal", role="unknown",
                status="unresolved", hash=ow.SHARED_ART, base=None, local=None,
                normals=None, support=None, assembly=None, geometry_source=None)


def candidate():
    return {"pin": dict(rom_sha256=ow.SUPPORTED_US_SHA256, state_key="test-state",
                        graphics_context="fixture-gfx", metadata_revision="test",
                        provenance_sha256="test-provenance", area=0),
            "placements": [placement(0), placement(4), placement(8)],
            "support_graph": {}, "coverage": {}}


class Contracts(unittest.TestCase):
    def test_collision(self):
        for raw in range(256):
            for flip in range(4):
                self.assertEqual(ow.adjusted_attribute(raw, flip << 14),
                                 raw | (flip & 1) if 16 <= raw < 28 else raw)
        self.assertEqual([ow.behavior(a) for a in (0, 1, 0x14, 8, 0x28, 0x50)],
                         ["normal", "solid", "normal", "water", "transition", "interactive"])

    def test_seams_planes_and_nonmovement(self):
        ps = [placement(63), placement(64), placement(64, 1), placement(65)]
        ps[-1]["behavior"] = "transition"
        components = ow.collision_components(ps)
        self.assertEqual(sorted(c["members"] for c in components), [1, 2])
        self.assertTrue(all(c["level"] is None for c in components))
        self.assertIsNone(ps[-1].get("collision_component"))

    def test_complete_snapshot_and_state_pin(self):
        s = dict(format="ALTPOWS1", rom_sha256="rom", area=27, state_key="event-2",
                 source_sha256="capture", graphics_context="vram-hash",
                 quadrants=[dict(index=27, map16=[0]*1024, map8=[0]*4096, writers=[0]*4096)])
        self.assertEqual(set(ow.validate_snapshot(s, "rom", 27, [27])), {27})
        for mutation in (lambda t: t.update(rom_sha256="stale"),
                         lambda t: t.update(state_key=""),
                         lambda t: t["quadrants"][0]["map8"].pop(),
                         lambda t: t["quadrants"].append(t["quadrants"][0])):
            bad = copy.deepcopy(s); mutation(bad)
            with self.assertRaises(ValueError): ow.validate_snapshot(bad, "rom", 27, [27])

    def test_gate_rejects_viewport_and_fake_solved(self):
        c = candidate(); c.update(format="ALTPOW1", blockers=[], capture_checks=dict(unobserved=0, mismatched=0),
                                  scope=dict(complete_gameplay_state=True, unsupported_layers=[]))
        c["pin"]["quadrants"] = [0]
        c["coverage"] = dict(placements=3, solved=3)
        for p in c["placements"]: p["status"] = "solved"
        errors = ow.validate_candidate(c)
        self.assertIn("incomplete/duplicate area grid", errors)
        self.assertIn("solved placement lacks effective geometry evidence", errors)

    def test_bridge_discovery_support_and_shared_art(self):
        if CORE is None: self.skipTest("supply --core for end-to-end bridge")
        c = candidate()
        # Same reusable family at x=0 and x=4 gets independently proven support.
        # x=8 is deliberately same art but different provenance => unresolved.
        c["placements"][2]["map16"] = 11
        family = dict(id="synthetic-floor", evidence="test-only assembly contract",
                      graphics_context="fixture-gfx", width=1, height=1,
                      contacts=[dict(endpoint=0, x=0, y=1)],
                      members=[dict(x=0, y=0, map16=10, word=271, map16_member=0,
                                    role="floor", geometry="flat", support=0, detail=[3]*64)])
        rules = dict(format="ALTPOWR1", pin=c["pin"], families=[family],
                     support_graph=dict(nodes=2, anchors=[dict(node=0, height=0, evidence="datum"),
                                                        dict(node=1, height=48, evidence="measured upper endpoint")],
                                        observations=[dict(x=x, y=1, layer=0, surface=0, node=n,
                                                           evidence="accepted foot contact") for x, n in ((0, 0), (4, 1))]))
        out = ow.apply_rules(c, rules, CORE)
        self.assertEqual(out["automatic_assemblies"], 2)
        self.assertEqual([p["base"] for p in out["placements"]], [0, 48, None])
        self.assertEqual([p["status"] for p in out["placements"]], ["solved", "solved", "unresolved"])
        self.assertEqual(out["placements"][1]["local"], [3]*64)
        self.assertIn("synthetic-floor", out["placements"][1]["evidence"])
        stale = copy.deepcopy(out)
        no_support = copy.deepcopy(rules); no_support["support_graph"]["observations"] = []
        cleared = ow.apply_rules(stale, no_support, CORE)
        self.assertIsNone(cleared["placements"][1]["local"])
        self.assertIsNone(cleared["placements"][1]["base"])
        bad = copy.deepcopy(rules); bad["pin"] = dict(bad["pin"], state_key="stale")
        with self.assertRaises(ValueError): ow.apply_rules(candidate(), bad, CORE)
        bad = copy.deepcopy(rules); bad["families"][0]["supports"] = {0: 1}
        with self.assertRaises(ValueError): ow.apply_rules(candidate(), bad, CORE)

    def test_real_rom_castle_family(self):
        if CORE is None: self.skipTest('supply --core')
        fixture=Path(__file__).resolve().parents[2]/'build/remaster-geometry/overworld-castle-core-v1.json'
        if not fixture.exists(): self.skipTest('generate complete ROM base fixture first')
        from alttp_overworld_castle import generate
        base=json.loads(fixture.read_text())
        out,rules=generate(copy.deepcopy(base),CORE)
        self.assertGreater(out['coverage']['solved'],4000)
        self.assertEqual(out['coverage']['conflicts'],0)
        self.assertEqual(out['coverage']['placements'],16384)
        shared=[p for p in out['placements'] if p['hash']==ow.SHARED_ART]
        self.assertTrue(any(p['role']=='wall' and p['status']=='solved' for p in shared))
        self.assertTrue(any(p['role']=='floor' and p['base']==32 for p in shared))
        self.assertTrue(any(p['role']=='unknown' for p in shared))
        self.assertTrue(any(p['role']=='obstacle' and p['base']==32 and max(p['local'])==8
                            for p in out['placements'] if p['status']=='solved'))
        # Same intrinsic floor artwork occurs at datum and terrace, but level is
        # attached only through a supported semantic component/contact graph.
        self.assertTrue(any(p['role']=='floor' and p['base']==0 for p in out['placements']))
        stairs=[p for p in out['placements'] if p['role']=='transition' and p['status']=='solved']
        self.assertEqual(len(stairs),8)
        self.assertTrue(all(p['collision']==0x22 and p['base']==0 for p in stairs))
        self.assertEqual(max(max(p['local']) for p in stairs),32)
        self.assertTrue(all(0 < n[2] < 1 for p in stairs for n in p['normals']))
        self.assertTrue(ow.validate_candidate(out)) # never publication-approved
        # Real western recess is a mixed cap/face assembly with shared endpoint
        # ownership, not 192 tile-local restarts or a nearby-floor fill.
        recess=next(a for a in out['assemblies'] if a['family']=='castle-courtyard-recess-crosssection')
        owned=[p for p in out['placements'] if p['assembly']==recess['id']]
        self.assertEqual(len(owned),96)
        self.assertTrue(all(p['status']=='solved' for p in owned))
        self.assertTrue(any(len(set(p['local']))>1 for p in owned))
        self.assertTrue(any(any(n[2]==1 for n in p['normals']) for p in owned))
        self.assertTrue(any(any(n[2]==0 for n in p['normals']) for p in owned))
        columns=[a for a in out['assemblies'] if a['family']=='castle-projecting-round-column-crosssection']
        self.assertGreaterEqual(len(columns),2)
        col=[p for p in out['placements'] if p['assembly']==columns[0]['id']]
        self.assertTrue(all(p['base']==32 for p in col))
        self.assertGreater(max(max(p['local']) for p in col),8)
        scope=out['castle_architecture_inventory']['reference_viewport']
        self.assertGreaterEqual(scope['by_status']['solved'],843)
        self.assertEqual(scope['placements'],865)
        self.assertNotIn('1132',scope['residual_map16'])
        # The decorated crest projects above its own terrace: treating it as a
        # second 32-unit ground-to-terrace wall creates a contradictory cycle.
        crest=next(a for a in out['assemblies'] if a['family']=='castle-complete-crest-1112-1232-crosssection')
        crest_cells=[p for p in out['placements'] if p['assembly']==crest['id']]
        self.assertTrue(all(p['base']==32 for p in crest_cells))
        self.assertTrue(any(max(p['local'])>0 for p in crest_cells))
        # Continuous decorated face, not a raised flat crest on its landing.
        self.assertEqual(max(p['base']+max(p['local']) for p in crest_cells),64)
        # User RGB-labelled F-G reuses approved LOWER C-D shoulder. Horizontal
        # side progression must turn into the south projection, not remain flat.
        decorated=next(a for a in out['assemblies'] if a['family']=='castle-decorated-east-side-crosssection')
        tiles={(p['x']-decorated['x'],p['y']-decorated['y']):p
               for p in out['placements'] if p['assembly']==decorated['id']}
        self.assertEqual(len(tiles),32)
        for y in range(8):
            face,cap=tiles[1,y],tiles[2,y]
            for py in range(8):
                if y<2:
                    self.assertEqual(face['base']+face['local'][py*8+7],
                                     cap['base']+cap['local'][py*8])
                    self.assertEqual(cap['normals'][py*8],[0,0,1])
            self.assertEqual(tiles[0,y]['local'][0],0)
        # Real overwritten crosssection cannot retain old wall ownership.
        wall=next(p for p in out['placements'] if p['role']=='wall' and p['status']=='solved')
        broken=copy.deepcopy(base)
        target=next(p for p in broken['placements'] if (p['x'],p['y'])==(wall['x'],wall['y']))
        target['writer']=17
        rejected,_=generate(broken,CORE)
        changed=next(p for p in rejected['placements'] if (p['x'],p['y'])==(wall['x'],wall['y']))
        self.assertNotEqual(changed['status'],'solved')
        # Authored rise scales effective real ramps, never palette/hash labels.
        higher,_=generate(copy.deepcopy(base),CORE,48)
        same=next(p for p in higher['placements'] if (p['x'],p['y'])==(wall['x'],wall['y']))
        self.assertGreaterEqual(max(same['local']),max(wall['local']))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--core", type=Path)
    args, rest = parser.parse_known_args(); CORE = args.core.resolve() if args.core else None
    unittest.main(argv=[__file__, *rest])
