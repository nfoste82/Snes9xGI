"""Independent byte/coordinate and source contracts, not generated goldens."""
import copy
import json
import os
from pathlib import Path
import struct
import unittest
import alttp_outdoor_audit as a


class OutdoorTests(unittest.TestCase):
    def test_fnv(self):
        self.assertEqual(a.fnv(b'hello'), 'a430d84680aabd0b')

    def test_strict_sections(self):
        raw=b'ALTPOW1\0'+b'abcd'
        m=dict(format=a.FORMAT,byte_order='little',input_bytes=12,
               input_fnv1a64=a.fnv(raw),sections=[dict(name='x',offset=8,size=4,fnv1a64=a.fnv(b'abcd'))])
        self.assertEqual(a.sections(raw,m)['x'],b'abcd')
        for change in ('whole','section','bounds','duplicate','gap'):
            n=copy.deepcopy(m)
            if change=='whole':n['input_fnv1a64']='0'
            if change=='section':n['sections'][0]['fnv1a64']='0'
            if change=='bounds':n['sections'][0]['size']=5
            if change=='duplicate':n['sections']*=2
            if change=='gap':n['sections'][0]['offset']=9
            with self.subTest(change=change),self.assertRaises(ValueError):a.sections(raw,n)

    def test_map_scroll_flip_wrap(self):
        # Lower-right screen block starts at word 3072, row31 col31.
        v=bytearray(65536); word=0xc405
        v[8190:8192]=word.to_bytes(2,'little')
        bg=dict(map_base_words=0,map_size=3,name_base_words=8192,tile_size=0,mosaic=0)
        q=a.raster_sample(v,bg,511,511,0,0)
        self.assertEqual((q['word'],q['k'],q['cell']), (word,0,[63,63]))
        self.assertEqual(a.raster_sample(v,bg,511,511,1,1)['cell'],[0,0])
        bg['tile_size']=1
        with self.assertRaises(a.Unsupported):a.raster_sample(v,bg,0,0,0,0)

    def test_sources_zero_absent_separate_bases(self):
        authored=dict(height=[0]*64,normal_xyz=[0]*192)
        s=dict(has_height=1,has_normals=1,local=0,base=32,context=0,placement=0,status=0)
        self.assertEqual(a.source_check(authored,s),[])
        s['has_normals']=0
        self.assertEqual(a.source_check(authored,s),['authored_normal_xyz_not_selected'])
        self.assertEqual(a.source_check({'height':[0]*64},s),[])
        self.assertEqual(a.effective(s),32)
        self.assertEqual(a.effective(dict(s,base=7)),7)
        self.assertIsNone(a.effective(dict(s,has_height=0)))

    def test_association(self):
        m=dict(width=256,height=224,rom_sha256='r')
        a.association(m,dict(width=256,height=224,rom_sha256='r'))
        with self.assertRaises(ValueError):a.association(m,dict(width=256,height=224,rom_sha256='wrong'))

    def test_wrong_scroll_graphics_and_owner(self):
        v=bytearray(65536);v[0:2]=(5).to_bytes(2,'little')
        bg=dict(map_base_words=0,map_size=0,name_base_words=8192,tile_size=0,mosaic=0)
        q=a.raster_sample(v,bg,0,0,0,0)
        i=dict(slot=5,palette=0,hflip=False,vflip=False,source=2,bg=1)
        z=dict(k=0,ink=0)
        self.assertEqual(a.compare_sample(q,i,z,[0]*64,0),[])
        shifted=a.raster_sample(v,bg,8,0,0,0)
        self.assertIn('slot',a.compare_sample(shifted,i,z,[0]*64,0))
        self.assertIn('graphics_identity',a.compare_sample(q,i,z,[1]*64,0))
        self.assertIn('layer',a.compare_sample(q,dict(i,bg=2),z,[0]*64,0))
        self.assertIn('canonical_sample',a.compare_sample(q,i,dict(z,k=7),[0]*64,0))

    def test_contract_is_not_collision_or_role(self):
        self.assertEqual(a.wall_contract([])['numeric_status'],'unsupported')
        # Explicit independent endpoints, unequal spans, shared corner and
        # underpass surfaces may coexist; no one-field flattening.
        rows=[dict(surface='upper',base=32,foot=32,top=64,span=16),
              dict(surface='upper',base=32,foot=32,top=64,span=32),
              dict(surface='underpass',base=0,foot=0,top=0,span=16)]
        self.assertEqual(a.wall_contract(rows)['rises'],[32,32,0])
        self.assertEqual(a.wall_contract([dict(role='wall',collision=1)])['numeric_status'],'unsupported')

    def test_priority_only_mismatch_and_controls(self):
        v=bytearray(65536)
        bg=dict(map_base_words=0,map_size=0,name_base_words=8192,tile_size=0,mosaic=0)
        for decorations in (0,0x0400,0xc000,0xdc00):
            for bit in (0,1):
                word=5|decorations|(bit<<13)
                v[:2]=word.to_bytes(2,'little')
                q=a.raster_sample(v,bg,0,0,0,0)
                i=dict(slot=5,palette=(word>>10)&7,hflip=bool(word&0x4000),
                       vflip=bool(word&0x8000),source=2,bg=1,
                       priority=bit,priority_semantics='map-bit-13')
                z=dict(k=q['k'],ink=0)
                self.assertEqual(a.compare_sample(q,i,z,[0]*64,0),[])
                self.assertEqual(a.compare_sample(q,dict(i,priority=1-bit),z,[0]*64,0),['priority'])

    def test_priority_missing_or_incomparable_is_unsupported(self):
        q=dict(priority=1)
        for i in ({},dict(priority=0),dict(priority=0,priority_semantics='bg-default-zero-not-map-bit'),
                  dict(priority=2,priority_semantics='map-bit-13'),dict(source=3,priority=1,priority_semantics='map-bit-13'),
                  dict(source=2,priority=1,priority_serialized=False,priority_semantics='map-bit-13')):
            self.assertEqual(a.priority_check(q,i)['status'],'unsupported')
        self.assertEqual(a.priority_check(q,dict(source=2,priority=0,priority_semantics='map-bit-13'))['status'],'mismatch')

    @unittest.skipUnless(os.environ.get('ALTTP_OUTDOOR_FRESH'), 'optional fresh audit output')
    def test_fresh_linked_hidden_unlinked_and_priority_unsupported(self):
        r=json.loads(Path(os.environ['ALTTP_OUTDOOR_FRESH']).read_text())
        self.assertEqual(r['raster_mismatches'],{})
        c=r['counts'];p=r['priority_comparison']['buffers']
        self.assertEqual((c['main_BG2_selected'],c['main_BG2_hidden']),(55045,2017))
        self.assertEqual(c['main_BG2_transparent_no_owner'],282)
        self.assertEqual(c['main_BG1_opaque_unobserved_ownership_unsupported']+
                         c['sub_BG2_opaque_unobserved_ownership_unsupported'],114406)
        for buf,n in [('main_BG2',57062),('sub_BG1',57344),('main',55045),('sub',57344),('scene',56983)]:
            self.assertEqual((p[buf]['linked_samples'],p[buf]['unsupported'],p[buf]['checked'],p[buf]['mismatch']),
                             (n,n,0,None))
        self.assertEqual(p['main_BG2']['raw_field_different'],3042)
        self.assertEqual(r['priority_comparison']['first']['main_BG2:raw_field_different']['screen'],[132,49])

    @unittest.skipUnless(os.environ.get('ALTTP_OUTDOOR_PINNED'), 'optional immutable capture fixture')
    def test_pinned_independent_bytes_and_tamper(self):
        # Independently sample the specific positive from bytes/ABI rather than
        # obtaining expected values from the adapter's generated report.
        p=Path(os.environ['ALTTP_OUTDOOR_PINNED']);c=p/'inputs/frame_overworld_x08b0_y06cc.s9xrmf'
        raw=Path(str(c)+'.overworld-input.bin').read_bytes()
        m=json.loads(Path(str(c)+'.overworld.json').read_text());s=a.sections(raw,m)
        self.assertEqual(s['wram'][16],9)
        self.assertEqual(struct.unpack_from('<I',c.read_bytes(),8)[0],40)
        line=s['renderer_line_scroll_hv_u16'];h,v=struct.unpack_from('<HH',line,4)
        # Mode1, BG2 map_base0, 64x64 map. Published ABI at screen0,0.
        cx=(h&511)//8;cy=(v&511)//8
        address=2*((cy//32*2+cx//32)*1024+(cy%32)*32+cx%32)
        word=struct.unpack_from('<H',s['vram'],address)[0]
        self.assertEqual(word,7433)
        k=(v%8)*8+h%8;self.assertEqual(k,60)
        ga=16384+(word&1023)*32;indices=[]
        for yy in range(8):
            for xx in range(8):
                bits=[(s['vram'][ga+yy*2+off]>>(7-xx))&1 for off in (0,1,16,17)]
                indices.append(sum(bit<<plane for plane,bit in enumerate(bits)))
        self.assertEqual(a.fnv(bytes([1,1,4,8,8]+indices)),'2d07bf83d9e367cf')
        profile=a.tomllib.loads(s['profile_toml'].decode())
        asset=next(q for q in profile['assets'] if q['tile_hash'].endswith('2d07bf83d9e367cf'))
        self.assertEqual(asset['height'][60],5)
        report=json.loads((p/'audit.json').read_text());z=report['selected_0_0']
        self.assertEqual((z['visible']['base'],z['visible']['local'],z['height']),(0,5,5))
        self.assertEqual(z['visible']['hash'],'2d07bf83d9e367cf')
        a.validate_state(s,m);a.verify_ppu_mirror(s,m)
        for key in m['wram_fields']:
            n=copy.deepcopy(m);n['wram_fields'][key]['value']^=1
            with self.subTest(field=key),self.assertRaises(ValueError):a.validate_state(s,n)
        n=copy.deepcopy(m);n['wram_fields']['module']['offset']=17
        with self.assertRaises(ValueError):a.validate_state(s,n)
        # Mutate one verified section while recomputing only the whole hash;
        # a whole-file match must not waive the independent section gate.
        changed=bytearray(raw);changed[8]^=1
        n=copy.deepcopy(m);n['input_fnv1a64']=a.fnv(changed)
        with self.assertRaisesRegex(ValueError,'section FNV'):a.sections(changed,n)
        n=copy.deepcopy(m);n['ppu']['bg'][1]['map_base_words']+=1024
        with self.assertRaises(ValueError):a.verify_ppu_mirror(s,n)
        n=copy.deepcopy(m);n['line_scroll']['v_includes_renderer_plus_one']=False
        with self.assertRaises(ValueError):a.validate_state(s,n)
        n=copy.deepcopy(m);n['rom_definitions']['count']=3751
        with self.assertRaises(ValueError):a.validate_state(s,n)


if __name__=='__main__':unittest.main()
