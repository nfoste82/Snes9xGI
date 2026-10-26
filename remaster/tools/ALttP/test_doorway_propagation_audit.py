import unittest
from types import SimpleNamespace
from alttp_validate_room_heights import audit_doorframes


class DoorwayPropagationAuditTests(unittest.TestCase):
    def test_complete_bands_transformed_split_and_overwrite(self):
        for flip in range(4):
            for split in range(2):
                cells = [[[SimpleNamespace(layer=bg,x=x,y=y,word=0xffff,tile_hash='empty')
                           for x in range(64)] for y in range(64)] for bg in range(2)]
                snapshot=SimpleNamespace(cell=lambda bg,x,y:cells[bg][y][x])
                data=bytearray([255]*12876);assets=[];records=[];floors=[[],[]]
                for x in range(4):
                    for y in range(3):
                        if x not in (0,3) and y!=0:continue
                        word=10+x*3+y;n=0x2716+2*(x*3+y);data[n:n+2]=word.to_bytes(2,'little')
                        px=20+(3-x if flip&1 else x);py=20+(2-y if flip&2 else y)
                        bg=(1 if y==0 else 0) if split else 1
                        c=cells[bg][py][px];c.word=word ^ (0x4000 if flip&1 else 0) ^ (0x8000 if flip&2 else 0);c.tile_hash=f'm{x}{y}'
                        a=dict(tile_hash=c.tile_hash,tile_role='doorway',height=[-3]*64)
                        # Omitted mask is still an exact structural member;
                        # explicit empty mask carries only the opening datum.
                        if y==2:a['doorframe_mask']='0000000000000000'
                        if x==0 and y==2:
                            a.update(doorway_support=True,doorway_support_direction='south')
                            floors[bg].append([0,px,py+(-1 if flip&2 else 1),24])
                            floor=cells[bg][py+(-1 if flip&2 else 1)][px];floor.tile_hash='floor';floor.word=0
                            assets.append(dict(tile_hash='floor',tile_role='floor',height=[0]*64))
                        assets.append(a);mask=a.get('doorframe_mask','ffffffffffffffff');base=24+8*(2-y)
                        records.append(dict(layer=bg,x=px,y=py,plane=0,frame_base=base,opening_base=24,
                                            template_offset=0x2716,split_dispatch=bool(split),mask=mask,
                                            effective_height=[-3+(base if int(mask,16)>>k&1 else 24) for k in range(64)]))
                solution=dict(doorframe_attachments=records,bg1_cells_plane_x_y_base=floors[0],cells_plane_x_y_base=floors[1])
                result=audit_doorframes(solution,snapshot,dict(assets=assets),data)
                self.assertEqual(result['required_marked_placements'],8)
                self.assertEqual(result['solved_marked_placements'],8)
                self.assertFalse(result['issues'])
                records[0]['frame_base']-=8
                result=audit_doorframes(solution,snapshot,dict(assets=assets),data)
                self.assertTrue(result['issues'])
                records[0]['frame_base']+=8
                cells[records[0]['layer']][records[0]['y']][records[0]['x']].word^=1
                result=audit_doorframes(solution,snapshot,dict(assets=assets),data)
                # The overwritten outer band destroys whole-frame ownership,
                # not the independently marked surviving same-BG jamb column.
                # Split dispatch is never inferred from this partial evidence.
                self.assertEqual(result['solved_marked_placements'],0 if split else 1)
                if not split:
                    foot=next(r for r in records if r['x']==20+(3 if flip&1 else 0) and r['y']==20+(0 if flip&2 else 2))
                    foot['frame_base']+=8
                    result=audit_doorframes(solution,snapshot,dict(assets=assets),data)
                    self.assertEqual(result['solved_marked_placements'],0)
                    foot['frame_base']-=8
                    cells[foot['layer']][foot['y']][foot['x']].word^=1
                    result=audit_doorframes(solution,snapshot,dict(assets=assets),data)
                    self.assertEqual(result['solved_marked_placements'],0)


if __name__=='__main__':unittest.main()
