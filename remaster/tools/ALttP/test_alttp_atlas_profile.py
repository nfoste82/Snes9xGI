#!/usr/bin/env python3
"""Focused checks for semantic ALTTP profile geometry."""

import unittest
import tomllib
import tempfile
import sqlite3
import json
import subprocess
from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace

from alttp_atlas_profile import _complete_semantic_groups, _semantic_wall_layers, _render_profile, generate_profile


class SemanticProfileTests(unittest.TestCase):
    def test_oob_compiler_preserves_present_and_absent_channels_in_stale_floor_group(self):
        ids = ['v1:4bpp:'+f'{i:016x}' for i in range(1, 7)]
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); db=root/'atlas.sqlite'; source=root/'source.toml'; out=root/'out.toml'
            con=sqlite3.connect(db)
            con.executescript('CREATE TABLE run_info(key TEXT,value TEXT); CREATE TABLE decoded_tile(content_id TEXT,indices BLOB); CREATE TABLE graphics_source(content_id TEXT,source_kind TEXT,pack_index INTEGER);')
            rom='66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'
            con.execute('INSERT INTO run_info VALUES(?,?)', ('rom_sha256',rom))
            for h in ids:
                con.execute('INSERT INTO decoded_tile VALUES(?,?)',(h,bytes([1]*64)))
                con.execute('INSERT INTO graphics_source VALUES(?,?,?)',(h,'background',1))
            con.commit(); con.close()
            text=f'schema_version = 34\n[game]\ntitle = "OOB compiler fixture"\nrom_sha256 = "{rom}"\n[lighting_space]\nupper_floor_height = 50\n'
            text+='[asset_groups.dungeon_floor]\ntile_hashes = '+json.dumps(ids)+'\n'
            text+='[asset_groups.wall_faces]\ntile_hashes = ["v1:4bpp:0000000000000099"]\n'
            for i,h in enumerate(ids):
                text+=f'\n[[assets]]\ntile_hash = "{h}"\ntile_role = "out-of-bounds"\n'
                if i in (1,2,3):text+='height = '+json.dumps([0]*64 if i==1 else [5]*64 if i==2 else list(range(64)))+'\n'
                if i==4:text+='normal_xyz = '+json.dumps([0,128,255]*64)+'\n'
                if i==5:text+='occlusion = '+json.dumps([0]*64)+'\n'
            source.write_text(text)
            probe=root/'probe'
            repo=Path(__file__).resolve().parents[3]
            subprocess.run(['clang++','-std=c++17','-O0',str(repo/'remaster/tests/out_of_bounds_compiled_profile_probe.cpp'),'-o',str(probe)],check=True)
            for preserve in (False,True):
                generate_profile(db,source,out,preserve_authored_geometry=preserve)
                self.assertEqual(tomllib.loads(out.read_text())['assets'],tomllib.loads(text)['assets'])
                self.assertEqual(source.read_text(),text)
                subprocess.run([str(probe),str(out)],check=True)
            sidecar=root/'semantics.json'
            for native in (False,True):
                annotations={h: {'role':'floor' if native else 'out-of-bounds', 'normals':[], 'context_dependent':False} for h in ids}
                sidecar.write_text(json.dumps({'rom_sha256':rom,'tiles':annotations}))
                source.write_text(text if native else text.replace('tile_role = "out-of-bounds"\n',''))
                for preserve in (False,True):
                    with patch('alttp_atlas_profile.AnnotationStore', return_value=SimpleNamespace(tiles=annotations.copy())), patch(
                            'alttp_atlas_profile.TileCatalog', return_value=SimpleNamespace(semantic_group=lambda key: [key])):
                        generate_profile(db,source,out,sidecar,preserve_authored_geometry=preserve)
                    self.assertEqual(tomllib.loads(out.read_text())['assets'],tomllib.loads(text)['assets'])
                    subprocess.run([str(probe),str(out)],check=True)
            # Context role persists, but no local geometry is generated. Runtime
            # classifier deliberately reports it unsupported without placement proof.
            annotations={h: {'role':'out-of-bounds','normals':[], 'context_dependent':True} for h in ids}
            sidecar.write_text(json.dumps({'rom_sha256':rom,'tiles':annotations}))
            source.write_text(text.replace('tile_role = "out-of-bounds"\n',''))
            with patch('alttp_atlas_profile.AnnotationStore', return_value=SimpleNamespace(tiles=annotations)), patch(
                    'alttp_atlas_profile.TileCatalog', return_value=SimpleNamespace(semantic_group=lambda key: [key])):
                generate_profile(db,source,out,sidecar,preserve_authored_geometry=True)
            for a in tomllib.loads(out.read_text())['assets']:
                self.assertEqual(a['tile_role'],'out-of-bounds')
                self.assertTrue(a['tile_semantic_context_dependent'])
            subprocess.run([str(probe),str(out)],check=True)

    def test_schema22_groups_interleaved_and_appended_roundtrip(self):
        a, b = ['v1:4bpp:'+f'{i:016x}' for i in (1, 2)]
        group = ('\n[[obstacle_groups]]\nname = "example"\n'
                 f'[[obstacle_groups.members]]\ntile_hash = "{a}"\nx = -1\ny = 0\nz = -3\n'
                 'hflip = true\nvflip = false\nsupport_contact = false\n'
                 f'[[obstacle_groups.members]]\ntile_hash = "{a}"\nx = 1\ny = 0\nz = 2\n'
                 'hflip = false\nvflip = true\nsupport_contact = true\n')
        assets = [f'\n[[assets]]\ntile_hash = "{h}"\nheight = '+json.dumps([7]*64)+'\n' for h in (a, b)]
        for schema in (22, 30):
            assembly = group if schema == 22 else group.replace('name = "example"\n', 'name = "example"\nsprite_assembly = true\n')
            for at in range(3):
                parts = assets.copy(); parts.insert(at, assembly)
                source = f'schema_version = {schema}\n'+''.join(parts)
                rendered = _render_profile(source, {h: dict(tile_hash=h, height=[7]*64) for h in (a,b)}, {})
                self.assertEqual(tomllib.loads(rendered), tomllib.loads(source))
                self.assertIn(assembly.strip(), rendered)
        with self.assertRaisesRegex(ValueError, 'schema_version22'):
            _render_profile(source.replace('schema_version = 30', 'schema_version = 21'), {}, {})

    def test_compiler_preserves_group_layers_and_missing_source(self):
        ids = ['v1:4bpp:'+f'{i:016x}' for i in (1,2)]
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); db=root/'atlas.sqlite'; source=root/'source.toml'; out=root/'out.toml'
            con=sqlite3.connect(db)
            con.executescript('CREATE TABLE run_info(key TEXT,value TEXT); CREATE TABLE decoded_tile(content_id TEXT,indices BLOB); CREATE TABLE graphics_source(content_id TEXT,source_kind TEXT,pack_index INTEGER);')
            con.execute('INSERT INTO run_info VALUES(?,?)', ('rom_sha256','rom'))
            for h in ids:
                con.execute('INSERT INTO decoded_tile VALUES(?,?)',(h,bytes(64)))
                con.execute('INSERT INTO graphics_source VALUES(?,?,?)',(h,'background',1))
            con.commit(); con.close()
            text='schema_version = 22\n[game]\nrom_sha256 = "rom"\n[lighting_space]\nupper_floor_height = 50\n'
            for i,h in enumerate(ids):
                text+=f'\n[[assets]]\ntile_hash = "{h}"\ntile_role = "obstacle"\n'
                if i==0:text+='height = '+json.dumps([37]*64)+'\n'
            text+='\n[[obstacle_groups]]\nname = "example"\n'
            for i,h in enumerate(ids):
                text+=f'[[obstacle_groups.members]]\ntile_hash = "{h}"\nx = {i}\ny = 0\nz = {-i}\nhflip = false\nvflip = false\nsupport_contact = true\n'
            source.write_text(text)
            generate_profile(db,source,out)
            parsed=tomllib.loads(out.read_text()); original=tomllib.loads(text)
            self.assertEqual(parsed['obstacle_groups'],original['obstacle_groups'])
            self.assertEqual(parsed['schema_version'],22)
            self.assertEqual(parsed['assets'], original['assets'])
            self.assertEqual(source.read_text(),text)
            # A member with NO asset table must remain absent, not get generated
            # zero height, default normals, occlusion/emission or an invalid shell.
            text_without_second = text.replace(f'\n[[assets]]\ntile_hash = "{ids[1]}"\ntile_role = "obstacle"\n', '')
            source.write_text(text_without_second)
            report=generate_profile(db,source,out)
            parsed=tomllib.loads(out.read_text())
            self.assertEqual([a['tile_hash'] for a in parsed['assets']],[ids[0]])
            self.assertEqual(report['obstacle_missing_local_sources'],[ids[1]])
            self.assertEqual(parsed['obstacle_groups'],original['obstacle_groups'])

    def test_activation_preserves_arrays_roles_schema_and_reports_deferred_usage(self):
        ids = ['v1:4bpp:' + f'{i:016x}' for i in range(1, 5)]
        annotations = {
            ids[0]: {'role': 'wall', 'normals': ['south'], 'context_dependent': False},
            ids[1]: {'role': 'floor', 'normals': [], 'context_dependent': False},
            ids[2]: {'role': 'obstacle', 'normals': [], 'context_dependent': False},
            ids[3]: {'role': 'wall', 'normals': ['east'], 'context_dependent': False},
        }
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); db = root / 'atlas.sqlite'; source = root / 'source.toml'; out = root / 'out.toml'
            con = sqlite3.connect(db)
            con.executescript('CREATE TABLE run_info(key TEXT,value TEXT); CREATE TABLE decoded_tile(content_id TEXT,indices BLOB); CREATE TABLE graphics_source(content_id TEXT,source_kind TEXT,pack_index INTEGER);')
            con.execute('INSERT INTO run_info VALUES(?,?)', ('rom_sha256', 'rom'))
            for key in ids:
                con.execute('INSERT INTO decoded_tile VALUES(?,?)', (key, bytes(64)))
                con.execute('INSERT INTO graphics_source VALUES(?,?,?)', (key, 'background', 1))
            con.commit(); con.close()
            text = ('schema_version = 28\n[game]\nrom_sha256 = "rom"\n'
                    '[lighting_space]\nupper_floor_height = 50\n'
                    '[asset_groups.dungeon_floor]\ntile_hashes = []\n'
                    f'[asset_groups.wall_faces]\ntile_hashes = ["{ids[1]}", "{ids[3]}"]\n'
                    '[asset_groups.stair_treads]\ntile_hashes = []\n')
            for i, key in enumerate(ids):
                text += f'\n[[assets]]\ntile_hash = "{key}"\n'
                if i == 1:
                    text += 'tile_role = "wall"\n'
                text += 'height = ' + json.dumps([40 if i == 0 else 7 if i == 1 else 0] * 64) + '\n'
                text += 'normal_xyz = ' + json.dumps([128, 128, 255] * 64) + '\n'
                text += 'occlusion = ' + json.dumps([0] * 64) + '\n'
                text += 'emission_rgba = ' + json.dumps([0] * 256) + '\n'
            source.write_text(text)
            (root / 'semantics.json').write_text(json.dumps({'tiles': annotations, 'rom_sha256': 'rom'}))
            with patch('alttp_atlas_profile.AnnotationStore', return_value=SimpleNamespace(tiles=annotations)), patch(
                    'alttp_atlas_profile.TileCatalog', return_value=SimpleNamespace(semantic_group=lambda key: [key])):
                report = generate_profile(db, source, out, root / 'semantics.json', preserve_authored_geometry=True)
                parsed = tomllib.loads(out.read_text()); original = tomllib.loads(text)
                self.assertEqual(parsed['schema_version'], 28)
                self.assertEqual(parsed['lighting_space'], original['lighting_space'])
                for a, b in zip(parsed['assets'], original['assets']):
                    for layer in ('height', 'normal_xyz', 'occlusion', 'emission_rgba'):
                        self.assertEqual(a[layer], b[layer])
                self.assertNotIn('tile_role', parsed['assets'][0])  # absolute assembly relief not promoted globally
                self.assertEqual(parsed['assets'][1]['tile_role'], 'wall')
                self.assertEqual(parsed['assets'][2]['tile_role'], 'obstacle')
                self.assertEqual(parsed['assets'][3]['tile_role'], 'wall')  # newly unknown zero-local annotation activates
                self.assertEqual(report['height_arrays_changed'], 0)
                self.assertEqual(set(report['deferred_semantic_role_conflicts']), set(ids[:2]))
                self.assertEqual(source.read_text(), text)
                # Existing regeneration workflow still deliberately replaces explicit geometry.
                report = generate_profile(db, source, out, root / 'semantics.json')
                regenerated = tomllib.loads(out.read_text())
                self.assertEqual(regenerated['assets'][0]['height'], [7-y for y in range(8) for x in range(8)])
                self.assertEqual(regenerated['assets'][1]['height'], [0]*64)
                self.assertEqual(regenerated['schema_version'], 28)

    def test_obstacle_replaces_legacy_group_and_preserves_relief(self) -> None:
        source = ('[asset_groups.dungeon_floor]\ntile_hashes = ["floor"]\n\n'
                  '[asset_groups.wall_faces]\ntile_hashes = ["rail", "wall"]\n\n'
                  '[[assets]]\ntile_hash = "rail"\nheight = [3, 7]\n')
        profile = tomllib.loads(source)
        rendered, _ = _complete_semantic_groups(source, profile, {
            "rail": {"role": "obstacle", "context_dependent": False}})
        result = tomllib.loads(rendered)
        self.assertEqual(result["asset_groups"]["obstacles"]["tile_hashes"], ["rail"])
        self.assertEqual(result["asset_groups"]["wall_faces"]["tile_hashes"], ["wall"])
        self.assertEqual(result["assets"][0]["height"], [3, 7])

    def test_explicit_wall_annotation_replaces_absolute_authored_geometry(self) -> None:
        content_id = "v1:4bpp:0123456789abcdef"
        source = f'[[assets]]\ntile_hash = "{content_id}"\nheight = [24]\n'
        authored = {content_id: {"tile_hash": content_id, "height": [24]}}
        local = _semantic_wall_layers({"role": "wall", "normals": ["east"]})
        rendered = _render_profile(source, authored, {content_id: local}, {content_id})
        self.assertEqual(tomllib.loads(rendered)["assets"][0]["height"], list(local["height"]))

    def test_cardinal_wall_descends_toward_its_normal(self) -> None:
        east = _semantic_wall_layers({"role": "wall", "normals": ["east"]})["height"]
        south = _semantic_wall_layers({"role": "wall", "normals": ["south"]})["height"]
        self.assertEqual(east[:8], tuple(range(7, -1, -1)))
        self.assertEqual(south[::8], tuple(range(7, -1, -1)))

    def test_authored_roles_replace_stale_labels_without_merging_corners(self) -> None:
        content_id = "v1:4bpp:0123456789abcdef"
        source = f'[[assets]]\ntile_hash = "{content_id}"\ntile_role = "wall"\nheight = [24]\n'
        authored = {content_id: {"tile_hash": content_id, "height": [24], "tile_role": "wall"}}
        rendered = _render_profile(source, authored, {}, tile_roles={content_id: "wall_corner"})
        self.assertEqual(tomllib.loads(rendered)["assets"][0]["tile_role"], "wall_corner")
        rendered = _render_profile(source, authored, {}, tile_roles={})
        self.assertNotIn("tile_role", tomllib.loads(rendered)["assets"][0])

    def test_explicit_native_annotation_survives_stale_compiler_labels(self):
        key = 'v1:4bpp:0123456789abcdef'
        source = (f'[[assets]]\ntile_hash = "{key}"\ntile_role = "obstacle"\n'
                  'tile_semantics_authored = true\ntile_semantic_normals = []\n'
                  'tile_semantic_note = "native evidence"\n')
        authored = {key: tomllib.loads(source)['assets'][0]}
        rendered = _render_profile(source, authored, {}, tile_roles={key: 'floor'})
        self.assertEqual(tomllib.loads(rendered)['assets'][0], authored[key])

    def test_corner_topology_combines_directional_ramps(self) -> None:
        outside = _semantic_wall_layers({"role": "wall_corner", "normals": ["east", "south"],
                                         "corner_type": "outside"})["height"]
        inside = _semantic_wall_layers({"role": "wall_corner", "normals": ["east", "south"],
                                        "corner_type": "inside"})["height"]
        self.assertEqual(outside[0], 7)
        self.assertEqual(outside[7], 0)
        self.assertEqual(outside[56], 0)
        self.assertEqual(inside[7], 7)
        self.assertEqual(inside[56], 7)
        self.assertEqual(inside[63], 0)

    def test_semantics_extend_existing_material_groups(self) -> None:
        source = ('[asset_groups.dungeon_floor]\ntile_hashes = ["old-floor"]\nmaterial = "floor"\n\n'
                  '[asset_groups.wall_faces]\ntile_hashes = ["old-wall"]\nmaterial = "wall"\n')
        profile = {"asset_groups": {"dungeon_floor": {"tile_hashes": ["old-floor"]},
                                    "wall_faces": {"tile_hashes": ["old-wall"]}}}
        semantics = {"new-floor": {"role": "floor", "context_dependent": False},
                     "new-wall": {"role": "wall", "context_dependent": False},
                     "unknown-corner": {"role": "wall_corner", "context_dependent": False,
                                        "corner_type": "unknown"}}
        rendered, additions = _complete_semantic_groups(source, profile, semantics)
        self.assertIn('tile_hashes = ["new-floor", "old-floor"]', rendered)
        self.assertIn('tile_hashes = ["new-wall", "old-wall"]', rendered)
        self.assertNotIn("unknown-corner", rendered)
        self.assertEqual(additions, {"dungeon_floor": 1, "wall_faces": 1})


if __name__ == "__main__":
    unittest.main()
