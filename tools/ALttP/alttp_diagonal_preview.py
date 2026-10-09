"""Independent ROM-word/authoring audit of the effective diagonal preview.

No asset hashes, room IDs, guessed bases or authored writes are used here.
"""
def diagonal_previews(snapshot, profile, template_data, layer=1):
    if template_data is None:
        return {}
    # Legacy cell-only synthetic adapters cannot decode unplaced ROM words.
    # They get no preview proof, not guessed graphics bases or tile identities.
    # A real RoomSnapshot is always strict: absent/corrupt decode inputs fail.
    from alttp_room_geometry import RoomSnapshot
    has_layers = hasattr(snapshot, 'layers')
    has_decoder = callable(getattr(snapshot, '_tile_hash', None))
    if not has_layers and not has_decoder and not isinstance(snapshot, RoomSnapshot):
        return {}
    if not has_layers or not has_decoder:
        raise ValueError('diagonal preview requires complete graphics layers and tile decoder')
    if len(template_data) != 12876:
        raise ValueError('diagonal preview requires complete ROM template data')
    if not 0 <= layer < len(snapshot.layers) or len(snapshot.layers[layer]) != 2:
        raise ValueError('diagonal preview requires a valid graphics layer')
    assets = {a['tile_hash']: a for a in profile.get('assets', [])}
    def word(off):
        return int.from_bytes(template_data[off:off+2], 'little')
    def local_for_word(w):
        address=(snapshot.layers[layer][1]+(w&1023)*16)&32767
        return assets.get(snapshot._tile_hash(address), {}).get('height', [])
    foot = None
    for axis,off in enumerate((0x2e8,0x2c8)):
        for band in range(3):
            h = local_for_word(word(off+band*2))
            if len(h) != 64:
                return {}
            f = h[63]-8*(2-band)
            if foot is not None and f != foot:
                return {}
            foot = f
            if any(h[y*8+x] != f+8*(2-band)+7-(x if axis else y)
                   for y in range(8) for x in range(8)):
                return {}
    if not 0 <= foot < 231:
        return {}
    claims = {}
    for kind in range(12):
        off=0x420+kind*10
        step=-1 if kind%4 in (0,3) else 1
        words=[word(off+k*2) for k in range(5)]
        owned=set()
        for y in range(1,59):
            for x in range(63):
                if any(all(snapshot.cell(layer,x+c,y+c*step+r).word == words[r]
                           for c in range(2) for r in range(start,start+3))
                       for start in range(3)):
                    owned.update((x+c,y+c*step+r) for c in range(2) for r in range(5)
                                 if snapshot.cell(layer,x+c,y+c*step+r).word==words[r])
        initial=set(owned)
        for y in range(60):
            for x in range(64):
                neighbor=any(sum((x+sign,y+sign*step+r) in initial for r in range(5))>=3
                             for sign in (-1,1))
                if not neighbor: continue
                if all(snapshot.cell(layer,x,y+r).word==words[r] for r in range(5)):
                    owned.update((x,y+r) for r in range(5))
                for start in (0,3):
                    if all(snapshot.cell(layer,x,y+r).word==words[r] for r in range(start,start+2)):
                        owned.update((x,y+r) for r in range(start,start+2))
        for r in (0,4):
            for y in range(64):
                for x in range(64):
                    if snapshot.cell(layer,x,y).word!=words[r]:continue
                    if any(all(0<=x+sign*n<64 and 0<=y+sign*n*step<64 and
                               (x+sign*n,y+sign*n*step) in owned and
                               snapshot.cell(layer,x+sign*n,y+sign*n*step).word==words[r]
                               for n in (1,2)) and
                           any((x+sign*n,y+sign*n*step) in initial for n in (1,2))
                           for sign in (-1,1)):
                        owned.add((x,y))
        for x,y in owned:
            c=snapshot.cell(layer,x,y);a=assets.get(c.tile_hash,{})
            if a.get('tile_role') in ('floor','obstacle','door','stair_top','stair_bottom') or len(a.get('height',[]))!=64:
                continue
            rows={4-r if c.vflip else r for r,w in enumerate(words) if w==c.word}
            if len(rows)!=1 or next(iter(rows))>3:continue
            row=next(iter(rows))
            h=[foot+max(0,min(24,31-row*8-x-y)) for y in range(8) for x in range(8)]
            claims.setdefault((x,y),set()).add((tuple(h),row,foot))
    result = {xy: {'height':list(next(iter(v))[0]),'row':next(iter(v))[1],
                  'foot':next(iter(v))[2], 'intercept':foot+31-next(iter(v))[1]*8,
                  'minimum':foot,'maximum':foot+24} for xy,v in claims.items() if len(v)==1}
    low_foot=None
    for axis,off in enumerate((0x330,0x308)):
        for band in range(3):
            h=local_for_word(word(off+band*2))
            if len(h)!=64:return result
            f=h[63]-8*(2-band)
            if low_foot is not None and f!=low_foot:return result
            low_foot=f
            if any(h[y*8+x]!=f+8*(2-band)+7-(x if axis else y) for y in range(8) for x in range(8)):return result
    displacement=foot-low_foot
    if low_foot<0 or displacement<=0 or displacement%8:return result
    shift=displacement//8;extent=4+shift;composed={}
    for kind in range(12):
        high=0x420+kind*10;low=high+120;reverse=bool(word(high)&0x8000)
        step=-1 if kind%4 in (0,3) else 1
        offsets=[(low+(r+1)*2 if r<4 else high+(r-shift+1)*2) if reverse else
                 (high+r*2 if r<shift else low+(r-shift)*2) for r in range(extent)]
        if any(not(high<=off<high+10 or low<=off<low+10) for off in offsets):continue
        if len({word(off)&0xdfff for off in offsets})!=extent:continue
        for y in range(1,64-extent):
            for x in range(63):
                members=[snapshot.cell(layer,x+c,y+c*step+r) for c in range(2) for r in range(extent)]
                if any(c.word&0xdfff!=word(offsets[n%extent])&0xdfff or len(assets.get(c.tile_hash,{}).get('height',[]))!=64 or
                       assets.get(c.tile_hash,{}).get('tile_role') in ('floor','obstacle','door','stair_top','stair_bottom')
                       for n,c in enumerate(members)):continue
                for n,c in enumerate(members):
                    row=n%extent;b=extent-1-row if reverse else row
                    intercept=low_foot+31+displacement-b*8
                    composed.setdefault((c.x,c.y),set()).add((intercept,low_foot,foot+24))
    for xy,values in composed.items():
        if len(values)!=1:
            result.pop(xy,None);continue
        intercept,minimum,maximum=next(iter(values))
        result[xy]=dict(height=[max(minimum,min(maximum,intercept-x-y)) for y in range(8) for x in range(8)],
                        row=0,foot=0,intercept=intercept,minimum=minimum,maximum=maximum)
    # Explicit semantic extent ends at a classified nonmatching usage. This
    # independent audit retains legacy four-band shell and ROM-owned foot datum.
    groups=profile.get('asset_groups',{})
    known=set().union(*(set(v.get('tile_hashes',[])) for v in groups.values())) if groups else set()
    def semantic(c):
        a=assets.get(c.tile_hash,{})
        return a.get('tile_semantics_authored') and a.get('tile_role')=='wall' and a.get('tile_semantic_normals')==['south_east'] and not a.get('tile_semantic_context_dependent')
    def boundary(c):
        a=assets.get(c.tile_hash,{})
        return a.get('tile_semantics_authored') or a.get('tile_role','unknown')!='unknown' or c.tile_hash in known
    for y in range(64):
        for x in range(64):
            c=snapshot.cell(layer,x,y);a=assets.get(c.tile_hash,{})
            if not semantic(c) or len(a.get('height',[]))!=64:continue
            h=a['height'];normals=a.get('normal_xyz',[])
            grounded=len(normals)==192 and any(h[k]==0 and normals[k*3:k*3+3]==[128,128,255] for k in range(64))
            old=result.get((x,y));proven=old and old['row']==3
            if not grounded and not proven:continue
            minimum=0 if grounded else old['foot'];up=1 if c.vflip else -1
            low_y=y-up
            if not 0<=low_y<64:continue
            low=snapshot.cell(layer,x,low_y)
            if semantic(low) or not boundary(low):continue
            tangent=-1 if c.hflip==c.vflip else 1
            companion=False
            for sign in (-1,1):
                nx,ny=x+sign,y+sign*tangent
                if not(0<=nx<64 and 0<=ny<64):continue
                n=snapshot.cell(layer,nx,ny);na=assets.get(n.tile_hash,{})
                nh=na.get('height',[]);nn=na.get('normal_xyz',[]);old_n=result.get((nx,ny))
                grounded_n=len(nh)==64 and len(nn)==192 and any(nh[k]==0 and nn[k*3:k*3+3]==[128,128,255] for k in range(64))
                if semantic(n) and n.hflip==c.hflip and n.vflip==c.vflip and (grounded_n or (old_n and old_n['row']==3)):companion=True
            if not companion:continue
            extent=0
            for row in range(64):
                yy=y+row*up
                if not 0<=yy<64:break
                t=snapshot.cell(layer,x,yy)
                if not semantic(t):break
                if t.hflip!=c.hflip or t.vflip!=c.vflip or len(assets[t.tile_hash].get('height',[]))!=64:extent=0;break
                extent+=1
            top=y+extent*up
            if extent<2 or not 0<=top<64 or not boundary(snapshot.cell(layer,x,top)):continue
            if extent<=4:continue
            maximum=minimum+(extent-1)*8
            if maximum>254:continue
            for row in range(extent):
                intercept=minimum+7+row*8
                result[x,y+row*up]=dict(height=[max(minimum,min(maximum,intercept-xx-yy)) for yy in range(8) for xx in range(8)],
                                     row=0,foot=0,intercept=intercept,minimum=minimum,maximum=maximum)
    return result
