import unittest
from types import SimpleNamespace
from alttp_obstacle_assemblies import obstacle_assemblies

class OwnershipTest(unittest.TestCase):
    def test_transforms_priority_overwrite_roles_and_ambiguity(self):
        templates = bytearray(12876)
        for n in range(8):
            templates[0x84a+n*2:0x84c+n*2] = (n+1).to_bytes(2, 'little')
        profile = {'assets': [dict(tile_hash=str(n+1), height=[n]*64) for n in range(8)]}
        for flip in range(4):
            words = {(10+x,10+y): ((1-x if flip&1 else x)*4+(3-y if flip&2 else y)+1) |
                     (0x4000 if flip&1 else 0) | (0x8000 if flip&2 else 0) | 0x2000
                     for x in range(2) for y in range(4)}
            class Snapshot:
                def cell(self, layer, x, y):
                    w = words.get((x,y),1023)
                    return SimpleNamespace(x=x,y=y,word=w,tile_hash=str(w&1023))
            s = Snapshot()
            self.assertEqual(len(obstacle_assemblies(s,profile,templates)),8)
            saved = words[10,10];words[10,10]=1023
            self.assertFalse(obstacle_assemblies(s,profile,templates));words[10,10]=saved
            profile['assets'][0]['tile_role']='floor'
            self.assertFalse(obstacle_assemblies(s,profile,templates));profile['assets'][0].pop('tile_role')
        words.clear()
        words.update({(20+x,20+y):(x%2)*4+y+1 for x in range(3) for y in range(4)})
        for n in range(8):
            templates[0x85a+n*2:0x85c+n*2]=((n+4)%8+1).to_bytes(2,'little')
        self.assertFalse(obstacle_assemblies(s,profile,templates))

if __name__ == '__main__':
    unittest.main()
