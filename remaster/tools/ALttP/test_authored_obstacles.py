import copy
import unittest
import pathlib
import tomllib
from types import SimpleNamespace
from alttp_authored_obstacles import audit, validate_groups, movement_components

A = 'v1:4bpp:1111111111111111'
B = 'v1:4bpp:2222222222222222'


class Snapshot:
    def __init__(self):
        self.cells = [[SimpleNamespace(tile_hash='empty', hflip=False, vflip=False) for _ in range(4096)] for _ in range(2)]
    def cell(self, layer, x, y):
        return self.cells[layer][y*64+x]


def member(h, x, y, z=0, hf=False, vf=False, contact=False):
    return dict(tile_hash=h, x=x, y=y, z=z, hflip=hf, vflip=vf, support_contact=contact)


class AuthoredObstacles(unittest.TestCase):
    def test_canonical_curtain_variants_read_only(self):
        # Read authored East unchanged; West is a test-only requested variant.
        profile = tomllib.loads((pathlib.Path(__file__).resolve().parents[2] / 'profiles/ALttP/alttp-profile.toml').read_text())
        east = next(g for g in profile['obstacle_groups'] if g['name'] == 'East_facing_curtains')
        west = copy.deepcopy(east); west['name'] = 'West_facing_curtains'
        replacements = {'v1:4bpp:221db4615adb55fd': 'v1:4bpp:f91826cad69954c9',
                        'v1:4bpp:ad3b150df3ea5b3b': 'v1:4bpp:a2af9baf8d303df7'}
        for m in west['members']:
            m['tile_hash'] = replacements.get(m['tile_hash'], m['tile_hash'])
        self.assertEqual(sum(a != b for a, b in zip(east['members'], west['members'])), 2)
        for flip in range(4):
            s = Snapshot()
            for gi, group in enumerate((east, west)):
                for m in group['members']:
                    x, y = 12+gi*16+(-m['x'] if flip&1 else m['x']), 12+(-m['y'] if flip&2 else m['y'])
                    s.cells[1][y*64+x] = SimpleNamespace(tile_hash=m['tile_hash'], hflip=m['hflip'] ^ bool(flip&1), vflip=m['vflip'] ^ bool(flip&2))
            proposal = dict(schema_version=profile['schema_version'], obstacle_groups=[east, west])
            r = audit(s, (bytes(4096),)*2, proposal, dict(plane=0, stairs=[]), components=[[-1]*4096 for _ in range(2)])
            self.assertEqual(r['required_cells'], 16)
            self.assertEqual(r['conflicting_cells'], 0)
            self.assertEqual(len(r['assemblies']), 2)
            self.assertEqual({a['group'] for a in r['assemblies']}, {1, 2})
            self.assertTrue(all(a['status'] == 'missing_contact_support' for a in r['assemblies']))

    def test_horizontal_mirrored_pillar_layout_owns_correct_support_side(self):
        floor='v1:4bpp:3333333333333333'
        for flip in range(4):
            s=Snapshot();members=[]
            hashes=[f'v1:4bpp:{n:016x}' for n in range(1,5)]
            for y in range(2):
                for x,h in enumerate(hashes):members.append(member(h,x,y,vf=bool(y),contact=x==3))
            p=dict(obstacle_groups=[dict(name='wall',members=members)],
                   assets=[dict(tile_hash=h,height=[7]*64) for h in hashes]+[dict(tile_hash=floor,tile_role='floor',height=[0]*64)])
            components=[[-1]*4096 for _ in range(2)];sol=dict(plane=0,stairs=[],cells_plane_x_y_base=[])
            for m in members:
                x=20+(-m['x'] if flip&1 else m['x']);y=20+(-m['y'] if flip&2 else m['y'])
                s.cells[1][y*64+x]=SimpleNamespace(tile_hash=m['tile_hash'],hflip=bool(flip&1),vflip=m['vflip']^bool(flip&2))
                if m['support_contact']:
                    x+=-1 if flip&1 else 1;j=y*64+x
                    s.cells[1][j]=SimpleNamespace(tile_hash=floor,hflip=False,vflip=False)
                    components[0][j]=0;sol['cells_plane_x_y_base'].append([0,x,y,24])
            r=audit(s,(bytes(4096),)*2,p,sol,components=components)
            self.assertEqual((r['solved_cells'],r['conflicting_cells']),(8,0))
            self.assertEqual({v['floor_base'] for v in r['placements']},{24})

    def test_declared_contact_zero_side_edge(self):
        floor='v1:4bpp:3333333333333333'
        for flip in range(4):
            s,p,c,sol=self.fixture([member(A,0,0),member(B,0,1,contact=True)])
            if flip&2:
                s.cells[1][9*64+10]=s.cells[1][11*64+10]
                s.cells[1][11*64+10]=SimpleNamespace(tile_hash='empty',hflip=False,vflip=False)
            foot=(9 if flip&2 else 11)*64+10
            for i in (10*64+10,foot):
                s.cells[1][i].hflip=bool(flip&1);s.cells[1][i].vflip=bool(flip&2)
            local=[7]*64
            for y in range(8):local[y*8+7]=0
            p['assets'][1]['height']=local
            p['assets'].append(dict(tile_hash=floor,tile_role='floor',height=[0]*64))
            j=foot+(-1 if flip&1 else 1)
            s.cells[1][j]=SimpleNamespace(tile_hash=floor,hflip=False,vflip=False)
            c[0]=[-1]*4096;c[0][j]=0
            sol['cells_plane_x_y_base']=[[0,j%64,j//64,24]]
            r=audit(s,(bytes(4096),)*2,p,sol,components=c)
            self.assertEqual(r['solved_cells'],2)
            self.assertEqual({v['floor_base'] for v in r['placements']},{24})
            p['assets'][1]['height']=[7]*64
            self.assertEqual(audit(s,(bytes(4096),)*2,p,sol,components=c)['missing_cells'],2)

    def test_sprite_groups_are_not_background_constraints(self):
        s, p, c, sol = self.fixture()
        p['schema_version'] = 30
        p['obstacle_groups'][0]['sprite_assembly'] = True
        self.assertEqual(len(validate_groups(p)[0]), 1)
        r = audit(s, (bytes(4096), bytes(4096)), p, sol, components=c)
        self.assertEqual(r['required_cells'], 0)
        self.assertEqual(r['sprite_groups_excluded'], 1)
        p['schema_version'] = 29
        with self.assertRaises(ValueError):
            validate_groups(p)

    def test_mixed_source_groups_retain_runtime_indexes(self):
        s, p, c, sol = self.fixture()
        p['schema_version'] = 30
        sprite = dict(name='moving', sprite_assembly=True,
                      members=[member('v1:4bpp:4444444444444444', 0, 0, contact=True)])
        p['obstacle_groups'].insert(0, sprite)
        r = audit(s, (bytes(4096), bytes(4096)), p, sol, components=c)
        self.assertEqual(r['groups'], 1)
        self.assertEqual(r['sprite_groups_excluded'], 1)
        self.assertTrue(r['assemblies'])
        self.assertEqual({a['group'] for a in r['assemblies']}, {2})

    def test_distinct_lower_floor_is_not_upper_obstacle_owner(self):
        floor = 'v1:4bpp:3333333333333333'
        for flip in range(4):
            s, p, c, sol = self.fixture()
            sol['stairs'] = [[20, 20, 1, 1]]
            p['assets'].append(dict(tile_hash=floor, tile_role='floor', height=[0]*64))
            attrs = [bytearray(4096), bytearray(4096)]
            for y in (10, 11):
                s.cells[1][y*64+10].hflip = bool(flip&1)
                s.cells[1][y*64+10].vflip = bool(flip&2)
                s.cells[0][y*64+10] = SimpleNamespace(tile_hash=floor, hflip=False, vflip=False)
                attrs[0][y*64+10] = 0x27
            if flip&2:
                s.cells[1][9*64+10] = s.cells[1][11*64+10]
                s.cells[1][11*64+10] = SimpleNamespace(tile_hash='empty', hflip=False, vflip=False)
                s.cells[0][9*64+10] = SimpleNamespace(tile_hash=floor, hflip=False, vflip=False)
                attrs[0][9*64+10] = 0x27
            j = (8 if flip&2 else 12)*64+10
            c[0][j] = 2; c[1][j] = 3
            sol['cells_plane_x_y_base'] = [[0, 10, j//64, -16], [1, 10, j//64, 50]]
            r = audit(s, tuple(map(bytes, attrs)), p, sol, components=c)
            self.assertTrue(r['success'])
            self.assertEqual({v['floor_base'] for v in r['placements']}, {-16}) # NOT highest/actor plane
            p['assets'][-1]['height'][0] = 1
            self.assertEqual(audit(s, tuple(map(bytes, attrs)), p, sol, components=c)['conflicting_cells'], 2)
            p['assets'][-1]['height'][0] = 0
            attrs[0][10*64+10] = 0x70
            self.assertEqual(audit(s, tuple(map(bytes, attrs)), p, sol, components=c)['conflicting_cells'], 2)

    def test_all_flips_sparse_repeated_z_signed_support(self):
        for flip in range(4):
            s = Snapshot()
            ms = [member(A, 0, 0, -2, vf=True), member(A, 0, 1, 3), member(B, 2, 2, 0, contact=True)]
            profile = dict(obstacle_groups=[dict(name='example', members=ms)], assets=[dict(tile_hash=h, height=list(range(64))) for h in (A, B)])
            components = [[-1]*4096 for _ in range(2)]; solution = dict(plane=0, stairs=[], cells_plane_x_y_base=[])
            for layer in range(2):
                for oy, base, n in ((10, -16, 1), (30, 50, 2)):
                    for m in ms:
                        x, y = 10+(-m['x'] if flip & 1 else m['x']), oy+(-m['y'] if flip & 2 else m['y'])
                        s.cells[layer][y*64+x] = SimpleNamespace(tile_hash=m['tile_hash'], hflip=m['hflip'] ^ bool(flip & 1), vflip=m['vflip'] ^ bool(flip & 2))
                        if m['support_contact']:
                            j = (y+(-1 if flip & 2 else 1))*64+x
                            components[0][j] = n; solution['cells_plane_x_y_base'].append([0, j%64, j//64, base])
            r = audit(s, (bytes(4096), bytes(4096)), profile, solution, components=components)
            self.assertTrue(r['success']); self.assertEqual(r['solved_cells'], 12)
            self.assertEqual({p['floor_base'] for p in r['placements']}, {-16, 50})
            solution['directional_geometry'] = [dict(layer=p['layer'], plane=q, x=p['x'], y=p['y'], base=p['floor_base'] if q == 0 else 255,
                                                    role=3, authored_obstacle=True, fallback_directions=0) for p in r['placements'] for q in range(2)]
            self.assertTrue(audit(s, (bytes(4096), bytes(4096)), profile, solution, components=components)['success'])
            solution['directional_geometry'][0]['base'] += 1
            self.assertEqual(len(audit(s, (bytes(4096), bytes(4096)), profile, solution, components=components)['mismatches']), 1)

    def fixture(self, ms=None):
        ms = ms or [member(A, 0, 0), member(B, 0, 1, contact=True)]
        s = Snapshot()
        for m in ms:
            s.cells[1][(10+m['y'])*64+10+m['x']] = SimpleNamespace(tile_hash=m['tile_hash'], hflip=m['hflip'], vflip=m['vflip'])
        p = dict(obstacle_groups=[dict(name='example', members=ms)], assets=[dict(tile_hash=h, height=[7]*64) for h in (A, B)])
        c = [[-1]*4096 for _ in range(2)]; c[0][12*64+10] = 0
        sol = dict(plane=0, stairs=[], cells_plane_x_y_base=[[0, 10, 12, 50]])
        return s, p, c, sol

    def test_missing_source_partial_and_explicit_contacts(self):
        s, p, c, sol = self.fixture()
        self.assertTrue(audit(s, (bytes(4096),)*2, p, sol, components=c)['success'])
        del p['assets'][1]['height']
        r = audit(s, (bytes(4096),)*2, p, sol, components=c)
        self.assertEqual(r['solved_cells'], 2)
        self.assertEqual(next(v for v in r['placements'] if v['tile_hash']==B)['local_source'], 'placement_obstacle_vertical_fallback')
        s.cells[1][10*64+10].tile_hash = 'overwrite'
        r = audit(s, (bytes(4096),)*2, p, sol, components=c)
        self.assertEqual(r['required_cells'], 1); self.assertEqual(r['issues'][0]['reasons'], ['incomplete_or_unmatched_footprint'])
        s, p, c, sol = self.fixture(); c[0][12*64+10] = -1; c[0][10*64+10] = 0
        sol['cells_plane_x_y_base'] = [[0, 10, 10, 50]]
        self.assertEqual(audit(s, (bytes(4096),)*2, p, sol, components=c)['assemblies'][0]['status'], 'missing_contact_support')

    def test_plane_and_component_conflicts_do_not_merge(self):
        for kind in ('plane', 'component', 'base'):
            s, p, c, sol = self.fixture([member(A, 0, 0, contact=True), member(B, 2, 1, contact=True)])
            c[0][11*64+10] = 0; c[0][12*64+12] = 1 if kind == 'component' else 0
            sol['cells_plane_x_y_base'] = [[0, 10, 11, 50], [0, 12, 12, 0 if kind == 'base' else 50]]
            if kind == 'plane':
                sol['stairs'] = [[20, 20, 1, 1]]; c[1][11*64+10] = 0
                sol['cells_plane_x_y_base'].append([1, 10, 11, 50])
            r = audit(s, (bytes(4096),)*2, p, sol, components=c)
            self.assertEqual(r['conflicting_cells'], 2); self.assertEqual(r['solved_cells'], 0)

    def test_overlap_and_semantic_symmetry(self):
        s, p, c, sol = self.fixture([member(A, 0, 0, 1, contact=True), member(A, 1, 0, 1, hf=True, contact=True)])
        r = audit(s, (bytes(4096),)*2, p, sol, components=c)
        self.assertEqual(len(r['assemblies']), 1) # H symmetric same footprint collapsed
        p['obstacle_groups'][0]['members'][1]['z'] = 2
        self.assertEqual(audit(s, (bytes(4096),)*2, p, sol, components=c)['conflicting_cells'], 2)
        s, p, c, sol = self.fixture([member(A, 0, 0, contact=True), member(A, 1, 0)])
        s.cells[1][10*64+12] = SimpleNamespace(tile_hash=A, hflip=False, vflip=False)
        self.assertEqual(audit(s, (bytes(4096),)*2, p, sol, components=c)['conflicting_cells'], 3)

    def test_shared_identity_index_and_ambiguous_complete_claims(self):
        s, p, c, sol = self.fixture()
        variant = copy.deepcopy(p['obstacle_groups'][0])
        variant['name'] = 'variant'
        variant['members'][-1]['tile_hash'] = 'v1:4bpp:3333333333333333'
        p['obstacle_groups'].append(variant)
        self.assertEqual(validate_groups(p)[1][A], {0, 1})
        self.assertEqual(audit(s, (bytes(4096),)*2, p, sol, components=c)['solved_cells'], 2)
        duplicate = copy.deepcopy(p['obstacle_groups'][0]); duplicate['name'] = 'competing'
        p['obstacle_groups'].append(duplicate)
        r = audit(s, (bytes(4096),)*2, p, sol, components=c)
        self.assertEqual(r['solved_cells'], 0)
        self.assertEqual(r['conflicting_cells'], 2)

    def test_strict_schema_and_contacts(self):
        _, p, _, _ = self.fixture()
        for change in ('contact', 'duplicate', 'missing_key'):
            bad = copy.deepcopy(p)
            if change == 'contact':
                for m in bad['obstacle_groups'][0]['members']: m['support_contact'] = False
            elif change == 'duplicate': bad['obstacle_groups'][0]['members'].append(copy.deepcopy(bad['obstacle_groups'][0]['members'][0]))
            else: del bad['obstacle_groups'][0]['members'][0]['z']
            with self.assertRaises(ValueError): validate_groups(bad)

    def test_actual_body_projection_and_disconnected_blockers(self):
        attrs = [bytearray([1]*4096), bytearray([1]*4096)]
        for y in range(8, 20):
            for x in range(8, 20): attrs[0][y*64+x] = 0
        for y in range(8, 20): attrs[0][y*64+14] = 1
        c = movement_components(tuple(map(bytes, attrs)), 0, (0, 64, 64), [])
        self.assertNotEqual(c[12*64+10], c[12*64+17]); self.assertGreaterEqual(c[12*64+10], 0)
        self.assertEqual(c[12*64+14], -1)
        # Tile00 in a one-cell strip cannot fit the actual16x16 collision body.
        for y in range(30, 40): attrs[0][y*64+30] = 0
        c = movement_components(tuple(map(bytes, attrs)), 0, (0, 64, 64), [])
        self.assertEqual(c[34*64+30], -1)

    def test_runtime_pixel_arrays_and_unknown_inventory(self):
        s, p, c, sol = self.fixture()
        proof = audit(s, (bytes(4096),)*2, p, sol, components=c)
        sol['directional_geometry'] = [dict(layer=r['layer'], plane=q, x=r['x'], y=r['y'], base=r['floor_base'] if q == 0 else 255,
                                            role=3, authored_obstacle=True, fallback_directions=0,
                                            runtime_effective_height=[57]*64, runtime_authored_pixels_preserved=True)
                                      for r in proof['placements'] for q in range(2)]
        self.assertTrue(audit(s, (bytes(4096),)*2, p, sol, components=c)['success'])
        sol['directional_geometry'][0]['runtime_effective_height'][0] = 58
        self.assertEqual(audit(s, (bytes(4096),)*2, p, sol, components=c)['mismatches'][0]['reason'], 'runtime_pixel_composition')
        del sol['directional_geometry'][0]
        self.assertTrue(any(r.get('reason') == 'missing_runtime_inventory' for r in audit(s, (bytes(4096),)*2, p, sol, components=c)['mismatches']))

    def test_mixed_missing_local_fallback_z_and_world_vertical_flips(self):
        for flip in range(4):
            s,p,c,sol=self.fixture();del p['assets'][1]['height']
            p['obstacle_groups'][0]['members'][0]['z']=9
            p['obstacle_groups'][0]['members'][1]['z']=-2
            for i in (10*64+10,11*64+10):s.cells[1][i].hflip=bool(flip&1);s.cells[1][i].vflip=bool(flip&2)
            if flip&2:
                s.cells[1][9*64+10]=s.cells[1][11*64+10];s.cells[1][11*64+10]=SimpleNamespace(tile_hash='empty',hflip=False,vflip=False)
                c[0][8*64+10]=0;sol['cells_plane_x_y_base']=[[0,10,8,50]]
            r=audit(s,(bytes(4096),)*2,p,sol,components=c)
            self.assertTrue(r['success'])
            authored=next(v for v in r['placements'] if v['tile_hash']==A)
            fallback=next(v for v in r['placements'] if v['tile_hash']==B)
            self.assertEqual(authored['floor_base'],50);self.assertEqual(fallback['floor_base'],34)
            self.assertEqual(fallback['local_height'][::8],list(range(8)) if flip&2 else list(range(7,-1,-1)))

    def test_validator_accepts_proved_group_and_retains_unrelated_gate_failure(self):
        from alttp_validate_room_heights import validate
        s, p, _, sol = self.fixture()
        for layer in s.cells:
            for cell in layer:
                cell.word = 0xffff
        attrs = [bytearray([1]*4096), bytearray([1]*4096)]
        for y in range(8, 16):
            for x in range(8, 16): attrs[0][y*64+x] = 0
        for y in (10, 11): attrs[0][y*64+10] = 1
        # No viewport/native assembly evidence: plain body support and explicit
        # existing solved floor input are sufficient for this terminal consumer.
        g = dict(format='alttp-doorway-graph-review-v1', room_id=7,
                 planes=[dict(reachable_body_cells=[]), dict(reachable_body_cells=[])],
                 elevation={**sol, 'link_xy': [64, 64], 'conflicting_components': [], 'unresolved_stairs': []})
        report = validate(g, 50, snapshot=s, attributes=tuple(map(bytes, attrs)), profile=p)
        self.assertTrue(report['authored_obstacle_groups']['success']); self.assertTrue(report['success'])
        g['elevation']['unknown_wall_cells'] = 1
        self.assertFalse(validate(g, 50, snapshot=s, attributes=tuple(map(bytes, attrs)), profile=p)['success'])


if __name__ == '__main__': unittest.main()
