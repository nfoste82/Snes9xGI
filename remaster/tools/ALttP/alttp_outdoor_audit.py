#!/usr/bin/env python3
"""Read-only ALTPOW1 evidence adapter. No inferred origins or numeric geometry.

Build alttp_outdoor_evidence.cpp and alttp_capture_debug_sources.cpp separately;
pass --bridge and --trace. JSON output and optional JSONL retain CPU-only evidence.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tomllib

from alttp_overworld_geometry import adjusted_attribute, behavior
from alttp_capture_source_activation import classify

FORMAT='alttp-live-overworld-evidence-v1'


class Unsupported(ValueError):
    pass


def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&0xffffffffffffffff
    return format(h,'x')


def sha(data):return hashlib.sha256(data).hexdigest()


def sections(raw,m):
    if (raw[:8]!=b'ALTPOW1\0' or m.get('format')!=FORMAT or
        m.get('byte_order')!='little' or m.get('input_bytes')!=len(raw) or
        m.get('input_fnv1a64')!=fnv(raw)):
        raise ValueError('ALTPOW1 magic/schema/size/whole FNV mismatch')
    result={};end=8
    for s in m['sections']:
        name,start,size=s['name'],s['offset'],s['size']
        if (type(start) is not int or type(size) is not int or size<0 or
            start!=end or start+size>len(raw) or name in result):
            raise ValueError('section bounds/overlap/gap/duplicate: '+name)
        data=raw[start:start+size]
        if fnv(data)!=s['fnv1a64']:raise ValueError('section FNV mismatch: '+name)
        result[name]=data;end=start+size
    if end!=len(raw):raise ValueError('unclaimed trailing bytes')
    return result


def u16(data,at):return struct.unpack_from('<H',data,at)[0]


def association(m,packet):
    for key in ('width','height','rom_sha256'):
        if m[key]!=packet[key]:raise ValueError('packet association mismatch: '+key)


def raster_sample(v,bg,h,vscroll,x,y):
    # Matches DrawBackground mode1 8x8 SC0..SC3 addressing; not an area origin.
    if bg['tile_size']:raise Unsupported('16x16 PPU tile addressing not implemented')
    size=bg['map_size'];w=512 if size&1 else 256;ht=512 if size&2 else 256
    px=(h+x)%w;py=(vscroll+y)%ht;cx=px//8;cy=py//8
    block=(cx//32)+(cy//32)*(2 if size&1 else 1)
    address=((bg['map_base_words']+block*1024+(cy%32)*32+cx%32)*2)&65535
    word=u16(v,address);kx=px%8;ky=py%8
    if word&0x4000:kx=7-kx
    if word&0x8000:ky=7-ky
    slot=word&1023;ga=((bg['name_base_words']*2)+slot*32)&65535
    indices=[]
    for yy in range(8):
        for xx in range(8):
            indices.append(sum(((v[(ga+(p//2)*16+yy*2+p%2)&65535]>>(7-xx))&1)<<p for p in range(4)))
    return dict(cell=[cx,cy],word=word,k=ky*8+kx,ink=indices[ky*8+kx],indices=indices,
                slot=slot,palette=(word>>10)&7,priority=(word>>13)&1,hflip=bool(word&0x4000),vflip=bool(word&0x8000),
                map_address=address,graphics_address=ga)


def source_check(asset,s):
    # Context/occurrence selection can intentionally replace canonical metadata.
    if s.get('context') or s.get('placement') or s.get('status'):return []
    return [q for q in classify(asset,s) if q.startswith('authored_')]


def priority_check(q,i):
    result=dict(map_bit=q['priority'],recorded=i.get('priority'))
    if (i.get('source')!=2 or i.get('priority_serialized') is False or
        i.get('priority_semantics')!='map-bit-13' or i.get('priority') not in (0,1)):
        return dict(result,status='unsupported',reason='BG priority is not attested as map bit 13; current BG SetDraw defaults to zero')
    return dict(result,status='match' if i['priority']==q['priority'] else 'mismatch')


def compare_sample(q,i,z,indices,bg):
    reasons=[]
    for k in ('slot','palette','hflip','vflip'):
        if i[k]!=q[k]:reasons.append(k)
    if i['source']!=2 or i['bg']!=bg+1:reasons.append('layer')
    if z['k']!=q['k']:reasons.append('canonical_sample')
    if indices!=q['indices']:reasons.append('graphics_identity')
    if z.get('ink')!=q['ink']:reasons.append('opacity')
    if priority_check(q,i)['status']=='mismatch':reasons.append('priority')
    return reasons


def effective(s):
    return s['base']+s['local'] if s.get('has_height') else None


def wall_contract(endpoints):
    if not endpoints or any(not all(k in e for k in ('surface','base','foot','top','span')) for e in endpoints):
        return dict(numeric_status='unsupported',reason='no independent placement endpoint/rise oracle',checked=0)
    return dict(numeric_status='explicit-fixture-contract-only',checked=len(endpoints),
                rises=[e['top']-e['foot'] for e in endpoints],surfaces=[e['surface'] for e in endpoints])


def validate_state(s,m):
    sizes=dict(wram=131072,vram=65536,fillram_register_mirror=32768,
               rom_map16_definitions=30016,rom_map8_attributes=512,ppu_oam=544,ppu_cgram_u16=512)
    for name,size in sizes.items():
        if len(s.get(name,b''))!=size:raise ValueError('required section size: '+name)
    # Independent fixed field ABI from pinned variables.h; do not trust offsets
    # supplied by a modified manifest to validate themselves.
    fields={'module':(0x10,1),'submodule':(0x11,1),'indoors':(0x1b,1),
      'link_y':(0x20,2),'link_x':(0x22,2),'link_z':(0x24,2),
      'map16_load_src_off':(0x84,2),'map16_load_dst_off':(0x86,2),'map16_load_var2':(0x88,2),
      'overworld_screen_index':(0x8a,2),'subsubmodule':(0xb0,1),
      'BG1HOFS_copy2':(0xe0,2),'BG2HOFS_copy2':(0xe2,2),'BG1VOFS_copy2':(0xe6,2),'BG2VOFS_copy2':(0xe8,2),
      'link_plane':(0xee,1),'overworld_map_state':(0x200,1),'overworld_area_index':(0x40a,2),
      'overworld_screen_transition':(0x418,1),'current_area_of_player':(0x700,2),
      'overworld_offset_base_y':(0x708,2),'overworld_offset_mask_y':(0x70a,2),
      'overworld_offset_base_x':(0x70c,2),'overworld_offset_mask_x':(0x70e,2),
      'overworld_area_is_big':(0x712,2),'main_tile_theme_index':(0xaa1,1),
      'aux_tile_theme_index':(0xaa2,1),'sprite_graphics_index':(0xaa3,1),'is_in_dark_world':(0xfff,1),
      'sram_progress_indicator':(0xf3c5,1),'savegame_is_darkworld':(0xf3ca,1)}
    if set(m['wram_fields'])!=set(fields):raise ValueError('WRAM field schema mismatch')
    values={}
    for key,(off,width) in fields.items():
        v=int.from_bytes(s['wram'][off:off+width],'little');values[key]=v
        if m['wram_fields'][key]!=dict(offset=off,width=width,value=v):raise ValueError('WRAM field mismatch: '+key)
    if values['module']!=9 or values['indoors']:raise Unsupported('not outdoor module9')
    ls=m['line_scroll']
    if ls!=dict(lines=m['height'],stride=16,bg_order=[0,1,2,3],v_includes_renderer_plus_one=True):
        raise ValueError('line scroll schema mismatch')
    if len(s['renderer_line_scroll_hv_u16'])!=m['height']*16:raise ValueError('line scroll bounds')
    if m['map16_layout']!=dict(wram_offset=8192,bytes=8192,width=64,height=64,row_stride=128,
                              members='ROM Map16 definitions: TL,TR,BL,BR u16'):
        raise ValueError('Map16 layout schema mismatch')
    if m['rom_definitions']!=dict(map16_snes='8f8000',map16_file=491520,count=3752,
                                 map8_attrs_snes='8e9459',map8_attrs_file=463961,count_attrs=512,attribute_index_mask=511):
        raise ValueError('ROM definitions schema mismatch')
    return values


def verify_ppu_mirror(s,m):
    """Check redundant manifest/mirror facts, NOT write-only latch history."""
    r=s['fillram_register_mirror'];p=m['ppu'];errors=[]
    expected=dict(mode=r[0x2105]&7,bg3_priority=(r[0x2105]>>3)&1,
                  forced_blank=r[0x2100]>>7,brightness=r[0x2100]&15,mosaic=(r[0x2106]>>4)+1)
    for k,v in expected.items():
        if p[k]!=v:errors.append(k)
    for b in range(4):
        q=p['bg'][b];sc=r[0x2107+b];nb=(r[0x210b+b//2]>>(4*(b%2)))&15
        for k,v in dict(index=b,map_base_words=(sc&252)<<8,map_size=sc&3,
                        name_base_words=nb<<12,tile_size=(r[0x2105]>>(4+b))&1,
                        mosaic=r[0x2106]&(1<<b)).items():
            if q[k]!=v:errors.append(f'BG{b+1}:{k}')
    if errors:raise ValueError('PPU manifest/register mirror mismatch: '+','.join(errors))
    return dict(status='redundant mirror fields verified; not raster history',
                main_enable=r[0x212c],sub_enable=r[0x212d])


def audit(capture,profile,bridge,trace,sample_path=None):
    raw=Path(str(capture)+'.overworld-input.bin').read_bytes()
    manifest_raw=Path(str(capture)+'.overworld.json').read_bytes();m=json.loads(manifest_raw)
    s=sections(raw,m);state=validate_state(s,m);ppu_mirror=verify_ppu_mirror(s,m)
    profile_bytes=profile.read_bytes()
    if not m.get('profile_pinned') or s.get('profile_toml')!=profile_bytes:
        raise ValueError('embedded/current profile SHA mismatch or absent profile')
    p=tomllib.loads(profile_bytes.decode());assets={int(a['tile_hash'].split(':')[-1],16):a for a in p.get('assets',[])}
    inv=json.loads(subprocess.check_output([str(bridge),str(capture),'--inventory']))
    association(m,inv)
    if p.get('game',{}).get('rom_sha256')!=inv['rom_sha256']:
        raise ValueError('embedded profile/packet ROM mismatch')
    for asset in inv['assets']:
        if fnv(bytes([1,1,asset['bit_depth'],8,8]+asset['indices']))!=asset['hash']:
            raise ValueError('packet asset hash/indices mismatch')
    instances={i['id']:i for i in inv['instances']};art={int(a['hash'],16):a['indices'] for a in inv['assets']}
    counts=Counter();mismatches=Counter();first={};placements={};used=set();perbuffer=defaultdict(Counter)
    selected_ids=set();hash_bases=defaultdict(set)
    priorities=defaultdict(Counter);priority_first={}

    def record_priority(buf,q,i,screen):
        check=priority_check(q,i);c=priorities[buf]
        c['linked_samples']+=1;c[check['status']]+=1
        c['map_bit_'+str(q['priority'])]+=1
        if check['recorded'] is not None:
            c['raw_field_equal' if check['recorded']==q['priority'] else 'raw_field_different']+=1
        priority_first.setdefault(buf+':'+check['status'],dict(screen=screen,**check))
        if check['recorded']!=q['priority']:
            priority_first.setdefault(buf+':raw_field_different',dict(screen=screen,**check))
        return check
    map16=Counter(u16(s['wram'],i) for i in range(8192,16384,2))
    # Reverse definitions are candidates only, never placement ownership. Repeated
    # words cannot establish a unique WRAM cell, much less world coordinates.
    members=defaultdict(list)
    for mid in map16:
        if mid<3752:
            for member in range(4):members[u16(s['rom_map16_definitions'],mid*8+member*2)].append([mid,member])
    output=sample_path.open('w') if sample_path else None
    try:
        with subprocess.Popen([str(trace),str(capture)],stdout=subprocess.PIPE,text=True) as proc:
            for line in proc.stdout:
                row=json.loads(line);x,y=row['sx'],row['sy'];counts['screen_coordinates']+=1
                visible=row['visible']
                if visible.get('source')==2:
                    counts['selected_BG_pixels']+=1
                    for channel,flag in (('height','has_height'),('normals','has_normals')):
                        donor=row['donors'][channel]
                        counts['selected_'+channel+('_present' if donor.get(flag) else '_absent')]+=1
                    if visible.get('context'):counts['selected_context_geometry']+=1
                    elif visible.get('placement'):counts['selected_placement_height']+=1
                    else:counts['selected_asset_variant_or_fallback']+=1
                    if visible.get('role')==2:counts['selected_inherited_wall_role_pixels']+=1
                counts['cpu_field_known' if row['field_known'] else 'cpu_field_unknown']+=1
                for buf in ('main','sub','scene'):
                    z=row.get(buf,{})
                    perbuffer[buf]['pixels' if z else 'unobserved']+=1
                    perbuffer[buf]['linked' if z.get('id') else 'unlinked']+=1
                    if z.get('id'):
                        selected_ids.add(z['id'])
                        if z.get('source')==2:
                            hash_bases[z['hash']].add(z['base'])
                            perbuffer[buf]['opaque' if z.get('ink',0)>0 else 'transparent']+=1
                            bg=z['bg']-1
                            if bg in (0,1):
                                settings=m['ppu']['bg'][bg]
                                if m['ppu']['mode']==1 and not settings['tile_size'] and not (settings['mosaic'] and m['ppu']['mosaic']>1):
                                    h,vs=struct.unpack_from('<HH',s['renderer_line_scroll_hv_u16'],y*16+bg*4)
                                    q=raster_sample(s['vram'],settings,h,vs,x,y)
                                    reasons=compare_sample(q,instances[z['id']],z,art.get(int(z['hash'],16)),bg)
                                    record_priority(buf,q,instances[z['id']],[x,y])
                                    perbuffer[buf]['raster_compared']+=1
                                    for reason in reasons:
                                        mismatches[buf+':'+reason]+=1
                                        first.setdefault(buf+':'+reason,dict(screen=[x,y],actual=z,
                                            expected={k:v for k,v in q.items() if k!='indices'}))
                                else:perbuffer[buf]['raster_unsupported']+=1
                checks=[]
                for b,o in enumerate(row['observed']):
                    z=o['pixel'];bg=b%2;buf=('main' if b<2 else 'sub')+f'_BG{bg+1}'
                    if o['depth'] is None:counts[buf+'_unknown']+=1;continue
                    settings=m['ppu']['bg'][bg]
                    if m['ppu']['mode']!=1 or (settings['mosaic'] and m['ppu']['mosaic']>1):
                        counts[buf+'_unsupported_mode_or_mosaic_phase']+=1;continue
                    h,vs=struct.unpack_from('<HH',s['renderer_line_scroll_hv_u16'],y*16+bg*4)
                    try:q=raster_sample(s['vram'],settings,h,vs,x,y)
                    except Unsupported:counts[buf+'_unsupported_tile_size']+=1;continue
                    key=(bg,*q['cell']);g=placements.setdefault(key,dict(bg=bg+1,cell=q['cell'],words=set(),ids=set(),pixels=Counter()))
                    g['words'].add(q['word']);status='opaque' if q['ink'] else 'transparent';counts[buf+'_'+status]+=1;g['pixels'][status]+=1
                    actual=z.get('id',0);reasons=[]
                    if actual:
                        used.add(actual);g['ids'].add(actual);i=instances[actual]
                        reasons=compare_sample(q,i,z,art.get(int(i['hash'],16)),bg)
                        priority=record_priority(buf,q,i,[x,y])
                        counts[buf+'_compared']+=1
                        selected=row.get('main' if b<2 else 'sub',{}).get('id')==actual
                        counts[buf+('_selected' if selected else '_hidden')]+=1
                        for issue in source_check(assets.get(int(i['hash'],16),{}),z):
                            counts['source_candidate_'+issue]+=1
                    elif q['ink']:
                        priority=dict(status='unlinked',reason='no observed instance')
                        # Disabled/window-clipped layers have depth0 too. End-frame
                        # TM/TS cannot prove an enabled raster interval. Do not
                        # label their expected opaque graphics a capture defect.
                        counts[buf+'_opaque_unobserved_ownership_unsupported']+=1
                    else:
                        priority=dict(status='unlinked',reason='transparent without observed instance')
                        counts[buf+'_transparent_no_owner']+=1
                    for reason in reasons:
                        mismatches[buf+':'+reason]+=1
                        first.setdefault(buf+':'+reason,dict(screen=[x,y],expected={k:v for k,v in q.items() if k!='indices'},actual=z))
                    attr=s['rom_map8_attributes'][q['word']&511];adj=adjusted_attribute(attr,q['word'])
                    checks.append(dict(buffer=buf,raster={k:v for k,v in q.items() if k!='indices'},
                        map16_member_candidates=members[q['word']],map16_ownership='unsupported',
                        world=None,raw_collision=attr,collision=adj,behavior=behavior(adj),mismatches=reasons,priority_check=priority))
                donor=row['donors']['height'];actual_height=effective(donor)
                if row['field_known'] and actual_height is not None and abs(actual_height-row['height'])>.001:
                    counts['cpu_field_differs_from_donor_base_plus_local']+=1
                if output:output.write(json.dumps(dict(row, outdoor=checks),sort_keys=True,separators=(',',':'))+'\n')
                if x==0 and y==0:sample=row
            if proc.wait():raise ValueError('production CPU trace failed')
    finally:
        if output:output.close()
    placement_rows=[]
    for key,g in sorted(placements.items()):
        ids=sorted(g['ids']);bases=sorted({instances[i]['base'] for i in ids})
        placement_rows.append(dict(bg=g['bg'],cell=g['cell'],words=sorted(g['words']),ids=ids,bases=bases,
            pixels=dict(g['pixels']),ownership='ambiguous' if len(g['words'])>1 else 'snapshot-raster-cell',world=None))
    return dict(format='ALTPOW-AUDIT1',approval=False,cpu_only=True,
        pins=dict(packet_sha256=sha(capture.read_bytes()),input_sha256=sha(raw),manifest_sha256=sha(manifest_raw),
                  profile_sha256=sha(profile_bytes),bridge_sha256=sha(bridge.read_bytes()),trace_sha256=sha(trace.read_bytes())),
        packet={k:inv[k] for k in ('schema','width','height','rom_sha256')},state=state,ppu_mirror=ppu_mirror,
        association=dict(rom_and_dimensions='verified',profile_bytes='verified',atomic_frame_identity='unsupported: no packet frame/profile digest in sidecar'),
        capabilities=dict(section_integrity='verified',wram_fields='verified',per_line_raster='compared, not forced aligned',
            world_origin='unsupported',map32='unsupported: definitions/compressed ROM streams absent',
            map16='resident IDs and definition-member candidates only; WRAM-to-raster ownership unsupported',
            animation='current VRAM indices compared; history unsupported',ppu_register_history='unsupported: mirror is not latch history',
             endframe_replay='unsupported: exact offline outdoor EndFrame state adapter unavailable',gpu='not run'),
        priority_comparison=dict(buffers={k:dict(v,checked=v['match']+v['mismatch'],mismatch=v['mismatch'] if v['match']+v['mismatch'] else None,unsupported=v['unsupported']) for k,v in priorities.items()},
            first=priority_first,scope='already-linked samples only; buffers overlap',
            semantics='map bit 13 is PPU visibility priority, not physical elevation; serialized BG field lacks comparable producer semantics',
            raw_field_counts='diagnostic only, NOT priority mismatch counts'),
        word_comparison='component comparison only; priority unsupported for current producer; no captured complete word',
        vram_address_comparison='not compared: packet graphics-cache address versus map-entry address are different quantities; graphics address parity not independently attested',
        counts=dict(counts),buffers={k:dict(v) for k,v in perbuffer.items()},raster_mismatches=dict(mismatches),
        first_mismatches=first,selected_0_0=sample,placements=placement_rows,
        zero_counts={k:counts[k] for k in ('selected_height_absent','selected_normals_absent',
            'selected_context_geometry','selected_placement_height','cpu_field_unknown',
            'cpu_field_differs_from_donor_base_plus_local','source_candidate_authored_height_not_selected',
            'source_candidate_authored_normal_xyz_not_selected')},
        reused_hash_bases={h:sorted(b) for h,b in sorted(hash_bases.items())},
        denominators=dict(raster_cells=len(placements),resident_map16_cells=4096,resident_map16_distinct_ids=len(map16),
            packet_instances=len(instances),observed_instances=len(used),unobserved_packet_instances=len(set(instances)-used),
            selected_instances=len(selected_ids),ambiguous_raster_cells=sum(len(g['words'])>1 for g in placements.values()),
            offscreen_placements=None,complete_area_placements=None,world_verified_pixels=0,numeric_wall_checked=0),
        wall_contract=wall_contract([]),
        failure_groups={k:'unsupported without independent source/placement endpoint evidence' for k in
            ('authoring_gap','transport_admission','wrong_placement_base','wrong_local_shape')},
        limitations=['No numeric height defect confirmed. Mismatches compare final snapshot to raster, not an expected height.',
            'Observed BG depths are stock observation, not proof of physical scene ownership; main/sub/scene donors remain separate.',
            'Resident WRAM and raster cells are not complete area coverage. Hidden underpasses and overwritten historical writers are unobserved.',
            'Canonical-source candidate counts are diagnostic, not confirmed activation failures.'])


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for key in ('capture','profile','bridge','trace','output'):ap.add_argument('--'+key,type=Path,required=True)
    ap.add_argument('--samples',type=Path)
    args=ap.parse_args()
    try:r=audit(args.capture,args.profile,args.bridge,args.trace,args.samples)
    except (ValueError,KeyError,struct.error) as e:
        args.output.write_text(json.dumps(dict(format='ALTPOW-AUDIT1',approval=False,status='invalid-or-unsupported-input',error=str(e)),sort_keys=True)+'\n')
        return 2
    args.output.write_text(json.dumps(r,sort_keys=True,indent=2)+'\n')
    print(json.dumps({k:r[k] for k in ('packet','denominators','raster_mismatches')},sort_keys=True))
    return 0


if __name__=='__main__':raise SystemExit(main())
