"""Read-only preservation regression against the pinned before-change artifact."""
import hashlib
import json
from pathlib import Path
import tomllib
import unittest
from alttp_room060_live016_update import ROOT, blocks


def relocated_profile(path):
    path = Path(path)
    if path.parent == ROOT and path.name.startswith('alttp-profile'):
        return ROOT / 'Profiles/ALttP' / path.name
    if path.parent == ROOT / 'tools' and path.name.startswith('alttp'):
        return ROOT / 'tools/ALttP' / path.name
    return path


class Live016Preservation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out=ROOT/'build/remaster-geometry/live016-review'
        cls.report=json.loads((cls.out/'update-report.json').read_text())
        cls.before=relocated_profile(cls.report['backup']).read_text()
        cls.after=(ROOT/'Profiles/ALttP/alttp-profile_backup.toml').read_text()
        cls.b=tomllib.loads(cls.before)
        cls.a=tomllib.loads(cls.after)
        cls.bb={v['tile_hash']:v for v in cls.b['assets']}
        cls.aa={v['tile_hash']:v for v in cls.a['assets']}

    def test_digests_and_exact_literals(self):
        self.assertEqual(hashlib.sha256(self.before.encode()).hexdigest(),self.report['before_sha256'])
        self.assertEqual(hashlib.sha256(self.after.encode()).hexdigest(),self.report['after_sha256'])
        old,new=blocks(self.before),blocks(self.after)
        for key,block in old.items():
            for line in block.splitlines():
                if line.startswith(('height =','normal_xyz =','occlusion =','emission_rgba =')):
                    self.assertIn(line,new[key])

    def test_all_existing_authored_fields(self):
        for key,asset in self.bb.items():
            for field,value in asset.items():
                if field!='tile_role':
                    self.assertEqual(self.aa[key][field],value,(key,field))
        for key,value in self.b.items():
            if key not in ('assets','schema_version'):
                self.assertEqual(self.a[key],value,key)

    def test_east_reference_exact_not_derivative(self):
        ref=self.bb['v1:4bpp:83ff6d16868aaa72']
        edits=[e for e in self.report['edits'] if e['operation']=='fill_neutral_wall_geometry']
        self.assertEqual(len(edits),5)
        for e in edits:
            self.assertNotIn(e['tile_hash'],self.bb)
            for field in ('height','normal_xyz'):
                self.assertEqual(self.aa[e['tile_hash']][field],ref[field])
            self.assertEqual(e['reference']['matches'],[{'template':'cardinal-east','hflip':False,'vflip':False}])

    def test_other_inputs_unchanged(self):
        manifest=json.loads((self.out/'source-inspection.json').read_text())['inputs']
        for path,value in manifest.items():
            current_path=relocated_profile(path)
            if current_path!=ROOT/'Profiles/ALttP/alttp-profile_backup.toml':
                self.assertEqual(hashlib.sha256(current_path.read_bytes()).hexdigest(),value['sha256'],path)

    def test_all35_both_layers_no_missing_normal_or_height(self):
        from alttp_room060_live016_placeholder_audit import audit
        report=audit()
        self.assertEqual(report['semantic_wall_hashes'],35)
        for field in ('height_absent','height_zero','height_flat','normal_absent','normal_all_up'):
            self.assertEqual(report['counts'][field],0,field)
        self.assertEqual(report['counts']['normal_constant'],15)
        self.assertEqual(report['counts']['unknown_corner_type'],2)
        self.assertEqual(report['metadata_scope_counts'],{
            'in_room_matches':17,'outside_room_matches':3,
            'in_room_excluded':17,'outside_room_excluded':4})
        self.assertEqual(len(report['role_edits_outside_room']),3)


if __name__=='__main__':
    unittest.main()
