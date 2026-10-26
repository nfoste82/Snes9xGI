import unittest
from types import SimpleNamespace

from alttp_exposed_height_detection import cardinal_claims, compare_seam, scene_source_issue


class ExposedDetectionTest(unittest.TestCase):
    def test_scene_height_present_not_sufficient(self):
        sub = dict(id=1, source=2, bg=1, ink=9, has_height=1, has_normals=0, role=1)
        p = dict(sx=0, sy=0, use_scene=1, field_known=0, participation=255,
                 scene=dict(id=0), main=dict(id=0), sub=sub)
        self.assertEqual(scene_source_issue(p)['reason'], 'scene_restore_requires_normals')
        p['sub'] = dict(sub, has_normals=1, role=4)
        self.assertEqual(scene_source_issue(p)['reason'], 'scene_restore_role_not_admitted')
        for change in (dict(use_scene=0), dict(field_known=1), dict(scene=dict(id=2)),
                       dict(sub=dict(sub, ink=0)), dict(main=dict(id=2))):
            self.assertIsNone(scene_source_issue(dict(p, **change)))

    def test_signed_cross_layer_step_not_flattened(self):
        high = dict(kind='high', writer=1, axis=0, reverse=0)
        low = dict(high, kind='low', writer=2)
        local = [7-y for y in range(8) for x in range(8)]
        a = dict(k=56, has_height=1, actual=39, displayed_local=local)
        b = dict(k=0, has_height=1, actual=23)
        r = compare_seam(a, b, high, low, (0, 1))
        self.assertEqual((r['expected_step'], r['actual_step'], r['residual']), (-1,-16,-15))
        self.assertEqual(compare_seam(a, dict(b, actual=38), high, low, (0,1))['status'], 'pass')
        # V-transformed south wall reverses the SIGNED slope.
        a = dict(a, displayed_local=[y for y in range(8) for x in range(8)], actual=23)
        r = compare_seam(a, dict(b, actual=39), dict(low, reverse=1), dict(high, reverse=1), (0,1))
        self.assertEqual((r['expected_step'],r['residual']), (1,15))
        # Horizontal/H-transformed east-facing section, same ownership rule.
        a = dict(a, k=7, displayed_local=[x for y in range(8) for x in range(8)])
        r = compare_seam(a,dict(b,k=0,actual=39),dict(low,axis=1,reverse=1),dict(high,axis=1,reverse=1),(1,0))
        self.assertEqual((r['expected_step'],r['residual']), (1,15))

    def test_ambiguous_or_incompatible_ownership_rejected(self):
        a = dict(k=56,has_height=1,actual=0,displayed_local=[7-y for y in range(8) for x in range(8)])
        b = dict(k=0,has_height=1,actual=0)
        h = dict(kind='high',writer=1,axis=0,reverse=0)
        l = dict(h,kind='low',writer=2)
        for ca, cb, direction in [(None,l,(0,1)),(h,h,(0,1)),(h,dict(l,reverse=1),(0,1)),(h,l,(1,0))]:
            self.assertIsNone(compare_seam(a,b,ca,cb,direction))
        self.assertEqual(compare_seam(a,dict(b,has_height=0),h,l,(0,1))['status'],'unresolved')

    def test_section_proof_priority_and_partial_overwrites(self):
        # Actual handler flip words, no hash identity or coordinate whitelist.
        template=bytearray(12876)
        for n in range(8):template[0x2f8+n*2:0x2fa+n*2]=(0x8001+n).to_bytes(2,'little')
        words={(0,x,y):0x8001+x*4+y for x in range(2) for y in range(4)}
        owners={k:1 for k in words}
        snapshot=SimpleNamespace(cell=lambda l,x,y:SimpleNamespace(word=words.get((l,x,y),0)))
        provenance=SimpleNamespace(objects=[dict(phase=1,ordinal=0,is_door=0)],owner=lambda l,x,y:owners.get((l,x,y),0))
        objects={(1,0):dict(subtype=0,object_id=2,width_code=0,height_code=1,x=0,y=0)}
        self.assertEqual(len(cardinal_claims(snapshot,provenance,objects,template)),6)
        # Priority-only change remains valid, changed word or overwritten writer does not.
        words[0,0,1] ^= 0x2000
        self.assertEqual(len(cardinal_claims(snapshot,provenance,objects,template)),6)
        owners[0,0,2]=2
        self.assertEqual(len(cardinal_claims(snapshot,provenance,objects,template)),3)
        owners[0,0,2]=1;words[0,0,2]^=1
        self.assertEqual(len(cardinal_claims(snapshot,provenance,objects,template)),3)
        # Fourth trim row for north (first for V-flipped south) is not a face.
        owners[0,1,0]=2
        self.assertEqual(len(cardinal_claims(snapshot,provenance,objects,template)),3)


if __name__ == '__main__': unittest.main()
