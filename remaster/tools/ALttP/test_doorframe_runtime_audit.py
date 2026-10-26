import unittest
from types import SimpleNamespace
from alttp_validate_room_heights import audit_doorframes


class DoorframeAudit(unittest.TestCase):
    def test_region_preservation_and_template_rejection(self):
        words = bytearray(12876)
        for n in range(12):
            words[0x2716+2*n:0x2718+2*n] = (100+n).to_bytes(2,'little')
        class Snapshot:
            def cell(self, layer, x, y):
                inside = layer == 1 and 10 <= x < 14 and 10 <= y < 13
                return SimpleNamespace(layer=layer,x=x,y=y,tile_hash='door' if (layer,x,y)==(1,10,10) else 'other',
                    word=100+(x-10)*3+y-10 if inside else 0)
        profile={'assets':[dict(tile_hash='door',tile_role='doorway',doorway_support=True,doorway_support_direction='north',doorframe_mask='0000000000000001',height=list(range(64)))]}
        # This outer north band is two cells above the roomward end. An
        # accepted claim must name the explicit directional landing datum;
        # arbitrary frame-only offsets are no longer admissible evidence.
        r=dict(layer=1,plane=0,x=10,y=10,frame_base=37,opening_base=21,
               template_offset=0x2716,mask='1',effective_height=[37]+[v+21 for v in range(1,64)])
        floors=[[0,10,9,21]]
        solution={'doorframe_attachments':[r],'cells_plane_x_y_base':floors}
        result=audit_doorframes(solution,Snapshot(),profile,words)
        self.assertEqual(result['solved_marked_placements'],1)
        self.assertFalse(result['issues'])
        self.assertEqual(audit_doorframes({'doorframe_attachments':[r]},Snapshot(),profile,words)['issues'][0]['reason'],
                         'opening_landing_mismatch')
        flattened={**r,'frame_base':21,'effective_height':[21]+[v+21 for v in range(1,64)]}
        self.assertEqual(audit_doorframes({'doorframe_attachments':[flattened],'cells_plane_x_y_base':floors},Snapshot(),profile,words)['issues'][0]['reason'],
                         'opening_landing_mismatch')

        landed = {**r, 'frame_base': 66, 'opening_base': 50, 'effective_height': [66]+[v+50 for v in range(1,64)]}
        solution = {'doorframe_attachments':[landed], 'cells_plane_x_y_base':[[0,10,9,50]]}
        self.assertFalse(audit_doorframes(solution,Snapshot(),profile,words)['issues'])
        solution['cells_plane_x_y_base'][0][3]=0
        self.assertEqual(audit_doorframes(solution,Snapshot(),profile,words)['issues'][0]['reason'],
                         'opening_landing_mismatch')

        hidden = {**r, 'plane': 1, 'visible': False, 'frame_base': -19,
                  'opening_base': -35, 'effective_height': [-19]+[v-35 for v in range(1,64)]}
        both={'doorframe_attachments':[r,hidden],'cells_plane_x_y_base':floors+[[1,10,9,-35]]}
        result=audit_doorframes(both,Snapshot(),profile,words)
        self.assertEqual(result['solved_marked_placements'],1)
        self.assertFalse(result['issues'])
        hidden['visible']=True
        self.assertEqual(audit_doorframes(both,Snapshot(),profile,words)['issues'][0]['reason'],
                         'ambiguous_visible_frame_plane')
        r['effective_height'][1]+=37
        self.assertEqual(audit_doorframes({'doorframe_attachments':[r],'cells_plane_x_y_base':floors},Snapshot(),profile,words)['issues'][0]['reason'],
                         'frame_mask_or_opening_preservation')
        r['effective_height'][1]-=37
        words[0x2716]=0
        self.assertEqual(audit_doorframes({'doorframe_attachments':[r],'cells_plane_x_y_base':floors},Snapshot(),profile,words)['issues'][0]['reason'],
                         'missing_intact_rom_door_template')


if __name__ == '__main__':
    unittest.main()
