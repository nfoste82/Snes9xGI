import unittest
from alttp_cap_contact_probe import regional_votes, status


class RegionalCapTests(unittest.TestCase):
    def test_exact_authored_cap_face_contradiction(self):
        a = [5, 5, 4, 3, 2, 1, 1, 0]
        b = [13, 13, 12, 11, 10, 9, 8, 7]
        def samples(values):
            return [{'height': h, 'normal': [128, 128, 255] if k < 2 else [220, 215, 128]}
                    for k, h in enumerate(values)]
        cap, face = regional_votes(samples(a), samples(b), [12, 12, 11, 10, 9, 8, 7, 6])
        self.assertEqual([v['base_delta'] for v in cap], [8, 8])
        self.assertEqual([v['base_delta'] for v in face], [9, 9, 9, 9, 8, 8])
        self.assertEqual(status(cap), 'compatible_relative_contact')
        self.assertEqual(status(face), 'unmatched_or_conflicting')
        # A cap/face split still cannot provide one coherent offset for the
        # face. Reverse displayed order (flip) must not hide the discrepancy.
        cap, face = regional_votes(samples(a)[::-1], samples(b)[::-1], [6, 7, 8, 9, 10, 11, 12, 12])
        self.assertEqual(status(face), 'unmatched_or_conflicting')

    def test_regular_authored_seam_can_be_split(self):
        a = [{'height': 5 if k < 2 else 6-k, 'normal': [128, 128, 255] if k < 2 else [220, 215, 128]}
             for k in range(8)]
        b = [{'height': 13 if k < 2 else 14-k, 'normal': v['normal']} for k, v in enumerate(a)]
        cap, face = regional_votes(a, b, [v['height']-1 for v in b])
        self.assertEqual(status(cap), 'compatible_relative_contact')
        self.assertEqual(status(face), 'compatible_relative_contact')
        self.assertEqual({v['base_delta'] for v in cap}, {8})
        self.assertEqual({v['base_delta'] for v in face}, {9})

    def test_unknown_evidence_not_filled(self):
        cap, face = regional_votes([{'height': None, 'normal': None}],
                                   [{'height': 4, 'normal': None}], [3])
        self.assertEqual((cap, face), ([], []))
        self.assertEqual(status(cap), 'unmatched_or_conflicting')


if __name__ == '__main__':
    unittest.main()
