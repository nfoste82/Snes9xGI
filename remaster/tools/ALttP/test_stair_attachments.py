import copy
import unittest
from types import SimpleNamespace

from alttp_stair_attachments import audit


class StairAttachmentTests(unittest.TestCase):
    def fixture(self, north=True, mirror=False, base=-17):
        cells={}; assets=[]; templates=bytearray(12876); records=[]
        for x in range(4):
            for y in range(4):
                n=x*4+y+1; templates[0x10c8+n*2-2:0x10c8+n*2]=n.to_bytes(2,'little')
        for x in range(4):
            for y in range(4):
                xx=3-x if mirror else x; yy=y if north else 3-y
                word=xx*4+yy+1 | (0x4000 if mirror else 0) | (0 if north else 0x8000)
                h=[max(0,23-(yy*8+k//8)*24//32) for k in range(64)]
                name=f'{x},{y}'
                cell=SimpleNamespace(x=x+10,y=y+20,word=word,tile_hash=name,hflip=mirror,vflip=not north)
                cells[cell.x,cell.y]=cell;assets.append(dict(tile_hash=name,height=h))
                for p in (0,1):records.append(dict(layer=0,plane=p,x=cell.x,y=cell.y,owned_stair=True,base=base,
                    fallback_directions=0,runtime_authored_pixels_preserved=True,runtime_effective_height=[base+v for v in h]))
        low=(24 if north else 19)*64+11; high=(19 if north else 24)*64+11
        bases=[]
        for end,p,b in ((low,1,base),(high,0,base+24)):
            for dx in (0,1):
                x,y=end%64+dx,end//64;name=f'landing{x},{y}'
                cells[x,y]=SimpleNamespace(x=x,y=y,tile_hash=name)
                assets.append(dict(tile_hash=name,height=[0]*64));bases.append([p,x,y,b])
        solution=dict(directional_geometry=records,bg1_cells_plane_x_y_base=bases,
            stair_attachments=[dict(layer=0,origin=20*64+10,low=low,high=high,low_plane=1,high_plane=0,rise=24,
                                    base=base,flip=(1 if mirror else 0)|(0 if north else 2),conflict=False)])
        return solution,SimpleNamespace(cell=lambda layer,x,y:cells[x,y]),dict(assets=assets),templates

    def test_signed_transforms_and_immutable_sources(self):
        for north in (True,False):
            for mirror in (True,False):
                for base in (-17,13):
                    args=self.fixture(north,mirror,base);before=copy.deepcopy(args[2])
                    result=audit(*args)
                    self.assertTrue(result['success'],result['issues'])
                    self.assertEqual(result['required_plane_members'],32)
                    self.assertEqual(args[2],before)

    def test_overwrite_endpoint_plane_consumer_and_claim_rejection(self):
        for kind in ('word','upper','plane','runtime','claim','context','facing_landing'):
            args=self.fixture();s,snapshot,p,t=args
            if kind=='word':snapshot.cell(0,10,20).word^=32
            if kind=='upper':s['bg1_cells_plane_x_y_base'][-1][-1]+=1
            if kind=='plane':s['stair_attachments'][0]['low_plane']=0
            if kind=='runtime':s['directional_geometry'][0]['runtime_effective_height'][0]+=1
            if kind=='claim':s['stair_attachments']=[]
            if kind=='context':p['tile_contexts']=[dict(tile_hash='0,0',name='alternate')]
            if kind=='facing_landing':p['assets'][-1]['geometry_by_facing']=True
            self.assertFalse(audit(*args)['success'],kind)


if __name__=='__main__':unittest.main()
