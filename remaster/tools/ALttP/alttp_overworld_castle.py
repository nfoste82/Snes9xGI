"""Authored castle curtain-wall family v1, compiled from exact ROM metatiles.

Evidence: ROM-derived labelled contact sheets produced by alttp_overworld_art.py;
Map16 $8f8000 definitions; $8e9459 collision. These annotations describe reusable
artwork/metatile assemblies, NOT map-coordinate rectangles or hash-global levels.

Calibration POLICY (review candidate, not measured physics): terrace rise 32
lighting units. South face spans 32 pixels below a 16-pixel cap; side artwork
spans 16 pixels. Same endpoints, independent structural normals. Caps/terrace
are coplanar in this first coarse family; cap bevel and battlement relief remain
unmodelled and explicitly excluded from detailed-geometry approval. Courtyard
datum is the largest NATURAL-GROUND component touching a curtain-wall foot.
Disconnected natural ground is NOT independently flattened to zero. Elevation
propagates through exact semantic contacts and compatible parallel run seams.

Floor membership is authored from metatile artwork AND collision; connectivity
is semantic topology, not proof of actual Link movement. Stair/ledge handlers,
curved decorative crests/columns use declared coarse pixel patches below; foliage
and unrecognized tower/corner variants remain unsupported.
"""
from collections import Counter, defaultdict, deque

from alttp_overworld_geometry import stable_digest, apply_rules


def sampled_patch(member, height_normal, low=1, high=0, role='wall', detail=False):
    """Compile a canonical assembly-local pixel patch, not hash-global metadata.

    height_normal receives assembly pixel XY. Returned height is normalized for
    face endpoints, or signed local units for additive obstacle detail. Normals
    are structural and are transformed by the portable matcher with the pixels.
    """
    member.update(geometry='sampled',low=low,high=high,role=role,progress=[],pixel_normals=[],detail=[])
    for y in range(8):
        for x in range(8):
            z,n=height_normal(member['x']*8+x,member['y']*8+y)
            member['progress'].append(0 if detail else round(max(0,min(1,z))*65535))
            member['detail'].append(round(z) if detail else 0)
            member['pixel_normals'].append(list(n))

# Canonical intrinsic metatile annotations. None of these sets assigns a level.
# Natural ground/grass variants inspected in the complete ROM contact sheet.
GROUND = {52, 53, 113, 159, 160, 161, 164, 165, 171, 218, 225, 226,
          377, 380, 421, 452, 511, 893, 894, 898, 900,
          1197,1198,1199,1202,1266,1267,1549,1551}
# Grey terrace interior, edge insets, eye-in-floor and upper-floor edge trim.
TERRACE = {1135, 1136, 1158, 1165, 1157, 1159, 1160, 1161, 1162,
           1163, 1164, 1182, 1235,1236,1237,1241}
# White horizontal top/cap bands with no face pixels. These may occur at many
# levels: only connected components and assembly constraints establish their Z.
CAPS = {1103, 1105, 1106, 1109, 1110}


def recount(candidate):
    counts = Counter(p['status'] for p in candidate['placements'])
    candidate['coverage'].update({k: counts['conflict' if k == 'conflicts' else k]
                                 for k in ('solved','classified','unresolved','conflicts','empty','non_geometric')})
    candidate['coverage']['by_role'] = dict(Counter(p['role'] for p in candidate['placements']))


def review_v4_families(families, family, rise):
    """User-reviewed architectural policies compiled as exact metatile families.

    No map coordinates or hashes select geometry. Mixed members retain per-pixel
    patches; all support levels still come from final assembly contact constraints.
    """
    def south(x,y):
        return (1,(0,0,1)) if y<16 else (max(0,(47-y)/31),(0,1,0))
    # Existing straight/ornament variants participate in one compatible run.
    for f in families:
        if (f['id'].startswith(('castle-curtain-south','castle-face-variant',
                               'castle-priority-south','castle-royal-crest'))):
            f['facade']=True
        if f['id'].startswith('castle-complete-crest-'):
            f['facade']=True
            f['signed_rise']=rise
            f['contacts']=[dict(endpoint=0,x=0,y=-1),dict(endpoint=1,x=0,y=5)]
            for m in f['members']:
                m['detail']=[0]*64
        # A column is a projecting layer ON the continuous facade, not a cutout
        # whose transparent background falls to the landing. Preserve its authored
        # round relief, but never erase the owning face with background zeroes.
        if ('round-column' in f['id']):
            f['id']+='-crosssection'; f['facade']=True; f['run_axis']='x'
            f['contacts']=[dict(endpoint=0,x=1,y=-1),dict(endpoint=1,x=0,y=8)]
            for m in f['members']:
                old=list(m['detail']); ns=list(m['pixel_normals'])
                sampled_patch(m,south,role='obstacle')
                for i in range(64):
                    y=m['y']*8+i//8
                    face=rise*max(0,min(1,(47-y)/31))
                    # Preserve the cylinder where it projects; background follows
                    # the exact same face projection as adjacent undecorated art.
                    if old[i]>face:
                        m['detail'][i]=round(old[i]-face)
                        m['pixel_normals'][i]=ns[i]
    # Shared corner profile. Outside C-D decorated-east-side family is
    # intentionally NOT edited (golden fixture).
    for name,rows in (('castle-upper-reentrant-west',((1168,),(1218,),(1179,))),):
        family(name+'-crosssection',[(mid,x,y,'floor') for y,row in enumerate(rows)
                                    for x,mid in enumerate(row)],[(0,0,-1),(1,2,6)],'x')
        if families[-1]['id']!=name+'-crosssection': continue
        f=families[-1]
        def elbow(x,y):
            if y<16: return 1,(0,0,1)
            t=(47-y)/31
            # Canonical side strips keep their independent horizontal rise.
            if name=='castle-reentrant-west':
                if x<16: return 1,(0,0,1)
                s=(31-x)/15
            else:
                if name=='castle-reentrant-east' and x>=16: return 1,(0,0,1)
                s=x/15
            return max(t,s), (0,1,0) if t>=s else ((1,0,0) if name=='castle-reentrant-west' else (-1,0,0))
        for m in f['members']: sampled_patch(m,elbow,role='corner')
    # Recess turns are in its complete footprint, never competing partial owners.
    recess=next(f for f in families if f['id']=='castle-courtyard-recess-crosssection')
    def recess_corner(x,y):
        if x<16 or x>=112 or y<16: return 1,(0,0,1)
        t=(47-y)/31
        s=(31-x)/15 if x<32 else (x-96)/15 if x>=96 else 0
        return max(t,s),(0,1,0) if t>=s else ((1,0,0) if x<32 else (-1,0,0))
    for m in recess['members']: sampled_patch(m,recess_corner,role='corner' if m['x']<4 or m['x']>=12 else 'wall')
    # RGB-pinned label correction: the approved LOWER C-D is the west shoulder
    # (234,218), not the decorated strip (240,198), which is UPPER F-G. Reuse
    # the approved shoulder's exact side/south min projection at its new base.
    f=next(f for f in families if f['id']=='castle-decorated-east-side-crosssection')
    f['contacts']=[dict(endpoint=0,x=5,y=2),dict(endpoint=1,x=-1,y=2)]
    def shoulder(x,y):
        if y<16:return (min(1,x/15),(-1,0,0)) if x<16 else (1,(0,0,1))
        t=max(0,(63-y)/47)
        return min(1,x/15,t),(-1,0,0) if x/15<t else (0,1,0)
    for m in f['members']:sampled_patch(m,shoulder,role='corner')
    # E-F reentrant elbow: complete three-row face plus non-owning lower
    # shoulder guard. Both facings share the same solved terrace/crown pair.
    family('castle-facade-reentrant-elbow-crosssection',
           [(1113,0,0,'floor'),(1122,0,1,'floor'),(1131,0,2,'floor'),(1109,1,0,'cap')],
           [(0,-1,0),(1,-1,6)],'x')
    if families[-1]['id']=='castle-facade-reentrant-elbow-crosssection':
        f=families[-1]
        for m in f['members']:
            if m['y']>=4 or m['x']>=2:m['owns']=False
            # This elbow ends at the shoulder's START, not at the end of a
            # separate48px straight face. Shared seam coordinate: side x/15 at
            # y31 meets the shoulder y0 exactly. Do not restart south(47-y)/31.
            def reentrant(x,y):
                if y<16:return 1,(0,0,1)
                t=(31-y)/15;s=x/15
                return max(t,s),(0,1,0) if t>=s else (-1,0,0)
            sampled_patch(m,reentrant,role='corner')
    # Priority1123/1132 is NOT an aperture: RGB/linked slot evidence identifies
    # the true open E doorway as a final-writer variant of the royal crest below.
    # End strips and ornament-overwritten columns. The final variant is mandatory
    # and the entire footprint is checked, rather than retaining old provenance.
    for name,rows in (
        ('castle-column-crest-right',((1141,1142,1143),(1146,1147,1257),(1150,1151,1152),(1186,1154,1187))),
        ('castle-facade-east-turn',((1233,),(1116,),(1130,))),
        ('castle-facade-right-turn',((1217,),(1219,),(1234,))),
        ('castle-facade-ground-right-turn',((1217,),(1219,),(1220,1221))),
    ):
        family(name+'-crosssection',[(mid,x,y,'floor') for y,row in enumerate(rows)
                                    for x,mid in enumerate(row)],[(0,0,-1),(1,0,len(rows)*2)],'x')
        if families[-1]['id']==name+'-crosssection':
            f=families[-1]; f['facade']=True
            for m in f['members']: sampled_patch(m,south,role='wall')
    family('castle-column-closed-door-crosssection',
           [(1253,0,0,'cap')]+[(mid,x+1,y,'floor') for y,row in enumerate(((1254,1141,1142,1143),
             (1116,1146,1147,1148),(1130,1150,1151,1152))) for x,mid in enumerate(row)]+
           [(mid,x,3,'floor') for x,mid in enumerate((1258,1259,1260,1154,1187))],
           [(0,2,-1),(1,10,8)],'x')
    if families[-1]['id']=='castle-column-closed-door-crosssection':
        f=families[-1];f['facade']=True
        for m in f['members']:
            if m['x']<2 and m['y']<6: m['owns']=False
            sampled_patch(m,south,role='wall')
    # The same outside shoulder occurs above both terrace and courtyard facades.
    # Final metatiles identify the corner; landing contacts establish its base.
    for name,rows in (
        ('castle-upper-west-outside',((1104,1107),(1110,1111),(1115,1116))),
        ('castle-upper-east-outside',((1223,1224),(1225,1226),(1227,1228))),
    ):
        family(name+'-crosssection',[(mid,x,y,'floor') for y,row in enumerate(rows)
               for x,mid in enumerate(row)],[(0,0,-1),(1,2,6)],'x')
        if families[-1]['id']==name+'-crosssection':
            f=families[-1]; f['facade']=True
            f['contacts']=([dict(endpoint=0,x=2,y=2),dict(endpoint=1,x=1,y=8)]
                           if name=='castle-upper-west-outside' else
                           [dict(endpoint=0,x=-1,y=2),dict(endpoint=1,x=1,y=8)])
            for m in f['members']:
                # Neighboring plain cap/body retain their independent ownership;
                # their exact final definitions are guards for the corner only.
                if name=='castle-upper-west-outside' and ((m['x']<2 and m['y']<4) or (m['x']>=2 and m['y']>=2)):
                    m['owns']=False
                sampled_patch(m,south,role='corner')
    # Final reentrant face/cap interface above F. Its cap cells are guards, not
    # flat replacements for the side; high and low contacts belong to E itself.
    family('castle-upper-side-interface-crosssection',
           [(1109,0,0,'cap'),(1114,0,1,'floor'),(1124,0,2,'floor')],
           [(0,-1,0),(1,-3,6)],'y')
    if families[-1]['id']=='castle-upper-side-interface-crosssection':
        f=families[-1]
        for m in f['members']:
            if m['y']<2 or m['y']>=4:m['owns']=False
            # Adjacent right band is the geometric cap of that same elbow,
            # not another horizontal face starting x0 and dropping15 units.
            sampled_patch(m,lambda x,y:(1,(0,0,1)),role='cap')
    # Decorative terrace border is a geometric cap, not movement support. Mixed
    # floor/border metatiles use canonical member roles, not asset hash selectors.
    for f in families:
        if f['id'].startswith('castle-semantic-metatile-'):
            mid=f['members'][0]['map16']
            if mid in TERRACE:
                for m in f['members']:
                    # Authored semantic regions in complete canonical metatiles,
                    # not a tile-slot/hash height whitelist. All other members
                    # of these mixed border definitions are geometric caps.
                    floor_parts={1157:{0,1},1159:{0,1},1160:{0,1},
                                 1161:{0,1,3},1162:{0,1,2},1163:{0,1},
                                 1164:{0,1},1182:{0,1}}.get(mid,{0,1,2,3})
                    m['role']='floor' if m['map16_member'] in floor_parts else 'cap'
                    if mid in (1159,1160,1161,1162,1163,1164):
                        m['role']='obstacle' if m['role']=='cap' else 'floor'
                        if m['role']=='obstacle': m.update(geometry='detail',detail=[6]*64)
    # Upper storey final foot above the left column: full cap/body guard excludes
    # the ordinary straight crosssections, so no precedence/ambiguous ownership.
    family('castle-upper-column-interface-crosssection',
           [(1110,0,0,'cap'),(1115,0,1,'floor'),(1108,0,2,'floor'),
            (1141,0,3,'floor')],[(0,0,-1),(1,1,6)],'x')
    if families[-1]['id']=='castle-upper-column-interface-crosssection':
        f=families[-1];f['facade']=True
        for m in f['members']:
            if m['y']<4 or m['y']>=6:m['owns']=False
            # Upper outside already completed its south face at y47. This
            # overwritten foot is its ledge, not a new copy of rows32..47.
            sampled_patch(m,lambda x,y:(0,(0,0,1)),role='cap')
    # Open E doorway is a live final-writer variant of the royal crest, observed
    # in SHA-pinned original0771 RGB/linked tiles. It is NOT the priority1123
    # strip (a different architectural region). Match every frame/aperture word.
    crest=next(f for f in families if f['id']=='castle-royal-crest-crosssection')
    import copy
    doorway=copy.deepcopy(crest);doorway['id']='castle-open-crest-door-crosssection'
    doorway['evidence']='user-attested open walkable aperture; original0771 linked final slots254/255/346/362; exact full crest guard'
    doorway['contacts']=[dict(endpoint=0,x=2,y=-1),dict(endpoint=1,x=2,y=6)]
    slots=((346,254,254,346),(362,255,255,362))
    for m in doorway['members']:
        if 2<=m['x']<6 and 4<=m['y']<6:
            x=m['x']-2;y=m['y']-4
            m['word']=(7<<10)|slots[y][x]|(0x4000 if x>=2 else 0)
            if x in (1,2):m.update(role='floor',geometry='flat',support=1,detail=[0]*64)
    families.append(doorway)
    # Storey foot interfaces where a turn replaces the straight cap. Guard the
    # lower facade crown, and bind to its proven high endpoint, not a nearby floor.
    for top,bottom in ((1222,1217),(1104,1110),(1222,1111)):
        name=f'castle-stacked-ledge-{top}-{bottom}'
        family(name,[(top,0,0,'cap'),(bottom,0,1,'cap')],[(0,0,2)])
        if families[-1]['id']==name:
            for m in families[-1]['members']:
                if m['y']>=2:m['owns']=False
    family('castle-column-crest-upper-ledge',[(1111,0,0,'cap'),(1108,0,1,'cap'),
           (1143,0,2,'cap'),(1233,1,2,'cap')],[(0,0,4)])
    if families[-1]['id']=='castle-column-crest-upper-ledge':
        for m in families[-1]['members']:
            if m['y']!=2 or m['x']>=2:m['owns']=False
    # Highest visible storey is cut by the complete-area north boundary. User
    # ordering authorizes a ledge96 above crown96; no nonexistent off-map cells
    # are invented. Exact cap/body below is its support, not entry/collision.
    family('castle-highest-wall-foot',[(1108,0,0,'floor'),(1111,0,1,'cap'),
           (1116,0,2,'floor'),(1108,0,3,'floor'),(1111,1,1,'cap'),(1116,1,2,'floor'),
           (1108,1,3,'floor'),(1111,0,4,'cap')],[(0,0,2)])
    if families[-1]['id']=='castle-highest-wall-foot':
        for m in families[-1]['members']:
            if m['y']>=2 or m['x']>=2:m['owns']=False
            else:m.update(role='wall',geometry='detail',detail=[round(rise*(15-(m['y']*8+i//8))/31) for i in range(64)])
    # The same upper wall foot over priority strips and right-hand turns.
    for body,foot in ((1123,1132),(1116,1108)):
        if body==1116:continue
        family(f'castle-highest-wall-foot-{body}',[(1108,0,0,'floor'),(1111,0,1,'cap'),
               (body,0,2,'floor'),(foot,0,3,'floor')],[(0,0,2)])
        if families[-1]['id']==f'castle-highest-wall-foot-{body}':
            for m in families[-1]['members']:
                if m['y']>=2:m['owns']=False
                else:m.update(role='wall',geometry='detail',detail=[round(rise*(15-(m['y']*8+i//8))/31) for i in range(64)])
    for below in (1143,):
        family('castle-upper-foot-right-column-crosssection',[(1111,0,0,'cap'),
               (1116,0,1,'floor'),(1108,0,2,'floor'),(below,0,3,'cap'),(1217,1,3,'cap')],
               [(0,0,0),(1,1,8)],'x')
        if families[-1]['id']=='castle-upper-foot-right-column-crosssection':
            f=families[-1]
            for m in f['members']:
                if m['y']!=4 or m['x']>=2:m['owns']=False
                sampled_patch(m,south,role='wall')
    family('castle-highest-wall-foot-end',[(1108,0,0,'floor'),(1111,0,1,'cap'),
           (1116,0,2,'floor'),(1108,0,3,'floor'),(1123,1,2,'floor')],[(0,0,2)])
    if families[-1]['id']=='castle-highest-wall-foot-end':
        for m in families[-1]['members']:
            if m['y']>=2 or m['x']>=2:m['owns']=False
            else:m.update(role='wall',geometry='detail',detail=[round(rise*(15-(m['y']*8+i//8))/31) for i in range(64)])
    family('castle-upper-foot-turn-ledge',[(1225,0,0,'cap'),(1227,0,1,'floor'),
           (1108,0,2,'floor'),(1111,0,3,'cap'),(1121,0,4,'floor')],[(0,-1,0)])
    if families[-1]['id']=='castle-upper-foot-turn-ledge':
        for m in families[-1]['members']:
            if m['y']!=4:m['owns']=False
            else:m.update(role='wall',geometry='detail',detail=[round(rise*(15-(m['y']*8+i//8-32))/31) for i in range(64)])
    # Complete stepped storey crown distinguishes boundary-clipped top members
    # from identical 1108 feet lower down. Sparse ownership claims only interfaces
    # not already owned by the full straight slices; every other final word guards.
    rows=((1104,1107,1108,1108,1108,1108,1108,1108,1108,1108,1222,1223,1224),
          (1110,1111,1111,1111,1111,1111,1111,1111,1111,1111,1111,1225,1226),
          (1115,1116,1116,1116,1123,1123,1123,1123,1116,1116,1116,1227,1228),
          (1108,1108,1108,1108,1132,1132,1132,1132,1108,1108,1108,1108,1222))
    family('castle-stepped-storey-crown',[(mid,x,y,'cap') for y,row in enumerate(rows)
           for x,mid in enumerate(row)],[(0,2,2),(1,2,8)])
    if families[-1]['id']=='castle-stepped-storey-crown':
        for m in families[-1]['members']:
            x,y=m['x']//2,m['y']//2
            m['owns']=(y==0 and x in (2,8,9)) or (y==3 and x==11)
            if m['owns']:
                m.update(role='wall',geometry='detail',support=0 if y==0 else 1,
                         detail=[round(rise*(15-((m['y']%2)*8+i//8))/31) for i in range(64)])
    # Uniform family cap ownership is published below, including guards only
    # through their actual geometry-owning assembly. These ledges stay unwalkable.


def build_rules(candidate, rise=32):
    if not 1 <= rise <= 1024: raise ValueError('candidate castle rise must be 1..1024')
    ps = candidate['placements']; pin = candidate['pin']
    if pin['graphics_context'] != 'initial-ROM-theme:36':
        raise ValueError('castle authored family requires verified castle theme36 graphics; live VRAM activation unproved')
    bycell = {(p['x'],p['y']): p for p in ps if p['layer'] == 0 and p['surface'] == 0}
    definitions = {f['map16']: f['words'] for f in candidate['families']}
    # Final metatile completeness: later writers may still be matched only when
    # all four final members retain one writer and the exact expected definition.
    metatiles = {}
    for p in ps:
        if p['map16_member'] != 0: continue
        x,y = p['x'],p['y']; mid = p['map16']
        members = [bycell.get((x+i%2,y+i//2)) for i in range(4)]
        if mid not in definitions: continue
        if all(q and q['map16'] == mid and q['map16_member'] == i and
               q['word'] == definitions[mid][i] and q['writer'] == p['writer']
               for i,q in enumerate(members)):
            metatiles[x,y] = mid
    # Semantic floor/cap components. Attribute compatibility never invents a role.
    surface = {}
    for (x,y),mid in metatiles.items():
        if mid not in GROUND | TERRACE | CAPS: continue
        for dy in range(2):
            for dx in range(2):
                p = bycell[x+dx,y+dy]
                if mid in GROUND and p['behavior'] not in ('normal','special'): continue
                if mid in GROUND and p['collision'] not in (0,0x48,0x40): continue
                surface[x+dx,y+dy] = 'ground' if mid in GROUND else 'terrace'
    nodes = []; node_at = {}; components = []
    def node(label):
        n=len(nodes); nodes.append(label); return n
    for root in sorted(surface):
        if root in node_at: continue
        n = node('semantic-'+surface[root]); todo=deque([root]); node_at[root]=n; cells=[]
        while todo:
            x,y=todo.popleft(); cells.append((x,y))
            for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                q=x+dx,y+dy
                if q in surface and surface[q] == surface[root] and q not in node_at:
                    node_at[q]=n; todo.append(q)
        components.append(dict(node=n,kind=surface[root],members=len(cells),seed=list(root)))
    semantic_nodes = dict(node_at)
    families = []
    def family(name, grid, contacts, run_axis=None):
        # grid = (Map16 ID, x16,y16, role/geometry policy). No placement locations.
        if any(mid not in definitions for mid,_,_,_ in grid): return
        w=max(x for _,x,_,_ in grid)*2+2; h=max(y for _,_,y,_ in grid)*2+2
        f=dict(id=name,evidence='authored ROM metatile contact-sheet family v1; coarse 32-unit policy',
               graphics_context=pin['graphics_context'],width=w,height=h,
               contacts=[dict(endpoint=e,x=x,y=y) for e,x,y in contacts], members=[])
        for mid,x16,y16,policy in grid:
            for part,word in enumerate(definitions[mid]):
                m=dict(x=x16*2+part%2,y=y16*2+part//2,map16=mid,word=word,map16_member=part,
                       role='floor',geometry='flat',support=0)
                if policy == 'cap': m['role']='cap'
                elif policy in ('west','east','south'):
                    m.update(role='wall',geometry='face',low=1,high=0)
                    if policy == 'west': m.update(axis=[1,0],start=0,span=15,normal=[-1,0])
                    elif policy == 'east': m.update(axis=[-1,0],start=-31,span=15,normal=[1,0])
                    else: m.update(axis=[0,-1],start=-47,span=31,normal=[0,1])
                f['members'].append(m)
        f['run_axis']=run_axis
        families.append(f)
    for mid in sorted((GROUND | TERRACE | CAPS) & definitions.keys()):
        family(f'castle-semantic-metatile-{mid}',[(mid,0,0,'cap' if mid in CAPS else 'floor')],[(0,0,0)])
    family('castle-curtain-west-crosssection',[(1102,0,0,'west'),(1134,1,0,'cap')],
           [(0,4,0),(1,-1,0)],'y')
    family('castle-curtain-east-crosssection',[(1167,0,0,'cap'),(1181,1,0,'east')],
           [(0,-1,0),(1,4,0)],'y')
    family('castle-curtain-south-crosssection',[(1111,0,0,'cap'),(1116,0,1,'south'),(1108,0,2,'south')],
           [(0,0,-1),(1,0,6)],'x')
    # Actual stair-behavior 0x22 metatile, bounded above by terrace interior and
    # below by the courtyard paving. Directed navigation is NOT synthesized:
    # this is an authored signed support relationship for the pictured stair.
    family('castle-stair-route-crosssection',[(1194,0,0,'floor')],[(0,0,-1),(1,0,2)],'x')
    if families[-1]['id']=='castle-stair-route-crosssection':
        for m in families[-1]['members']:
            m.update(role='transition',geometry='ramp',support=-1,low=1,high=0,
                     axis=[0,-1],start=-15,span=15,normal=[0,1])
    # Paving curb joins natural ground and courtyard paving at equal support.
    # Its bevel is coarse coplanar geometry in this candidate, not a 32-unit wall.
    family('castle-courtyard-curb-crosssection',[(1191,0,0,'cap')],[(0,-1,0),(1,2,0)],'y')
    if families[-1]['id']=='castle-courtyard-curb-crosssection': families[-1]['signed_rise']=0
    # Paired column-foot/plinth trim, seen repeatedly below the crest assemblies.
    # It mixes terrace and raised trim within one metatile: annotate each member,
    # never the shared blank asset. Eight-unit plinth detail is an authored coarse
    # relief policy, not a claim that the whole column is only eight units high.
    family('castle-paired-column-foot-trim',[(1137,0,0,'floor'),(1138,1,0,'floor')],
           [(0,0,2),(0,3,2)])
    if families[-1]['id']=='castle-paired-column-foot-trim':
        for m in families[-1]['members']:
            if m['x'] in (1,2):
                m.update(role='obstacle',geometry='detail',detail=[8]*64)
    # Full projecting round-column assemblies. The three-by-four definition is
    # repeated on facade runs. Background/terrace pixels remain at owning base;
    # the crown, cylindrical body and tapered foot are distinct pixel regions.
    # This is coarse authored structural relief, not brightness-derived height.
    column_grid=[(mid,x,y,'floor') for y,row in enumerate(((1141,1142,1143),
                  (1146,1147,1148),(1150,1151,1152),(1153,1154,1155))) for x,mid in enumerate(row)]
    family('castle-projecting-round-column',column_grid,[(0,0,8),(0,5,8)])
    if families[-1]['id']=='castle-projecting-round-column':
        def column_patch(x,y):
            import math
            # Artwork crown is centered at x24, cylinder narrows below y24.
            dx=x-23.5
            if y<16:
                radius=8 if y<4 else 13
                if dx*dx+(y-9)**2 <= radius*radius:
                    return 32,(0,0,1)
            if 16<=y<48 and abs(dx)<=13:
                nx=dx/13; ny=math.sqrt(max(0,1-nx*nx))
                return 28-(y-16)*20/31,(nx,ny,0)
            if 48<=y<60 and abs(dx)<=16-(y-48)*.5:
                return 8-(y-48)*8/11,(0,1,0)
            # Unraised face background belongs to this projecting assembly's
            # support surface, not to the adjacent higher curtain-wall endpoint.
            return 0,(0,0,1)
        for m in families[-1]['members']:
            sampled_patch(m,column_patch,0,0,'obstacle',True)
    # Recessed courtyard top wall (the obstacle immediately west of Link in the
    # supplied viewport). Two-sided end turns and the twin recessed chevrons
    # are authored as ONE final 8x6-Map8 assembly, not independently flattened.
    notch_rows=((1167,1168,1111,1169,1170,1111,1113,1134),
                (1167,1173,1174,1175,1176,1174,1122,1134),
                (1167,1179,1180,1180,1180,1180,1131,1134))
    family('castle-courtyard-recess-crosssection',
           [(mid,x,y,'floor') for y,row in enumerate(notch_rows) for x,mid in enumerate(row)],
           [(0,2,-1),(0,13,-1),(1,6,6),(1,9,6)],'x')
    if families[-1]['id']=='castle-courtyard-recess-crosssection':
        def recess_patch(x,y):
            # Outer bands are top surfaces meeting the independent side walls.
            if x<16 or x>=112: return 1,(0,0,1)
            # The inset is decoration over ONE continuous underlying face.
            # Restarting the ramp below its zigzag changed the centre-third rate.
            cap_end=16
            if y<cap_end: return 1,(0,0,1)
            progress=max(0,(47-y)/(47-cap_end))
            return progress,(0,1,0)
        for m in families[-1]['members']:
            sampled_patch(m,recess_patch,role='corner' if m['x']<2 or m['x']>=14 else 'wall')
    # Four-metatile royal crest: paired hanging shield members between ordinary
    # face ends. Keep this separate from round columns and from terrace plinths.
    crest_rows=((1111,1112,1112,1111),(1117,1118,1119,1120),
                (1126,1127,1128,1129))
    family('castle-royal-crest-crosssection',
           [(mid,x,y,'floor') for y,row in enumerate(crest_rows) for x,mid in enumerate(row)],
           [(0,2,-1),(0,5,-1),(1,2,6),(1,5,6)],'x')
    if families[-1]['id']=='castle-royal-crest-crosssection':
        def crest_patch(x,y):
            if y<16: return 1,(0,0,1)
            # Relief is carried separately from the base face progression below.
            return (47-y)/31,(0,1,0)
        for m in families[-1]['members']:
            sampled_patch(m,crest_patch,role='cap' if m['y']<2 else 'wall')
            # Raised crest is eight authored units, inset to the shield artwork;
            # no extra rise is applied to the repeated plain face background.
            for py in range(8):
                for px in range(8):
                    x=m['x']*8+px; y=m['y']*8+py
                    if 16<=x<48 and 20<=y<44:
                        m['detail'][py*8+px]=round(8*min(1,(y-19)/5,(44-y)/5))
    # Decorated face-end variants use identical endpoints/projection but exact
    # final member definitions. Contact below skips the separately owned foot
    # trim; not a nearest-neighbor search or arbitrary support extrapolation.
    for mid,body,foot in ((1111,1116,1125),(1111,1116,1130),
                          (1111,1121,1108),(1111,1229,1108)):
        name=f'castle-face-variant-{mid}-{body}-{foot}-crosssection'
        family(name,[(mid,0,0,'cap'),(body,0,1,'south'),(foot,0,2,'south')],
               [(0,0,-1),(1,0,8)],'x')
    # Independent side-contact rule for straight strips whose south landing is
    # covered by an ornament. Do not extrapolate underneath it: those placements
    # stay classified until the ornament/interface provenance supplies a seam.
    # Same column above a ground landing: side foot pixels are natural-ground
    # members (1186/1187), NOT the terrace-foot 1153/1155 members. Reuse the
    # cylindrical profile but establish its own complete footprint contacts.
    ground_column=((1141,1142,1143),(1146,1147,1148),
                   (1150,1151,1152),(1186,1154,1187))
    family('castle-ground-round-column',
           [(mid,x,y,'floor') for y,row in enumerate(ground_column) for x,mid in enumerate(row)],
           [(0,0,8),(0,5,8)])
    if families[-1]['id']=='castle-ground-round-column':
        for m in families[-1]['members']: sampled_patch(m,column_patch,0,0,'obstacle',True)
    for left,foot in ((1146,1188),(1185,1187)):
        rows=((1141,1142,1143),(left,1147,1148),(1150,1151,1152),(1186,1154,foot))
        name=f'castle-ground-round-column-{left}-{foot}'
        family(name,[(mid,x,y,'floor') for y,row in enumerate(rows) for x,mid in enumerate(row)],
               [(0,0,8)])
        if families[-1]['id']==name:
            for m in families[-1]['members']: sampled_patch(m,column_patch,0,0,'obstacle',True)
    # Six-wide crest variants retain the identical cap/face/relief functions;
    # their outer plain face strips are owned by this assembly, not competing
    # independently matched face-variant candidates.
    for leftcap in (1112,1184):
        rows=((leftcap,1112,1112,1111),(1117,1118,1119,1120),(1126,1127,1128,1129))
        name=f'castle-royal-crest-cap-{leftcap}-crosssection'
        family(name,[(mid,x,y,'floor') for y,row in enumerate(rows) for x,mid in enumerate(row)],
               [(0,2,-1),(1,2,8)],'x')
        if families[-1]['id']==name:
            for m in families[-1]['members']: sampled_patch(m,crest_patch,role='cap' if m['y']<2 else 'wall')
    # Decorative side-strip variants: canonical face is LEFT of its cap, so its
    # high edge is x15 adjoining cap x16 (a west-facing normal). The old reversed
    # progression incorrectly put zero at that high seam, creating a32-unit jump.
    # Require the complete vertical strip and upper terrace landing; cap remains
    # independent from side face. No level is assigned from the ornament alone.
    family('castle-decorated-east-side-crosssection',
           [(mid,x,y,'floor') for y,row in enumerate(((1131,1124),(1139,1140),
                 (1144,1145),(1149,1108))) for x,mid in enumerate(row)],
           [(0,-1,2),(1,4,2)],'y')
    if families[-1]['id']=='castle-decorated-east-side-crosssection':
        def east_side(x,y):
            if x>=16: return 1,(0,0,1)
            return x/15,(-1,0,0)
        for m in families[-1]['members']: sampled_patch(m,east_side,role='cap' if m['x']>=2 else 'wall')
    # Eye-in-floor medallion is coplanar engraved terrace art, not a tower.
    # Do not claim the already-owned plain interior member. A semantic assembly
    # can have a sparse footprint while all specified members remain mandatory.
    family('castle-terrace-eye-inlay',[(1171,0,0,'floor'),(1172,1,0,'floor'),(1177,1,1,'floor')],
           [(0,-1,0),(0,0,2)])
    # Upper south-face strip distinguished by PPU-priority variant members. Exact
    # top/foot provenance and independent terrace support are required.
    family('castle-priority-south-crosssection',[(1111,0,0,'cap'),(1123,0,1,'south'),(1132,0,2,'south')],
           [(0,0,-1),(1,0,6)],'x')
    # Actual four-wide crest cap variants: right cap belongs to the adjoining
    # wall turn, not always to plain 1111. Match all twelve final metatiles.
    for leftcap,rightcap in ((1112,1232),(1184,1253)):
        rows=((leftcap,1112,1112,rightcap),(1117,1118,1119,1120),
              (1126,1127,1128,1129))
        name=f'castle-complete-crest-{leftcap}-{rightcap}-crosssection'
        family(name,[(mid,x,y,'floor') for y,row in enumerate(rows) for x,mid in enumerate(row)],
               [(0,2,-1),(1,2,7)],'x')
        if families[-1]['id']==name:
            families[-1]['signed_rise']=0 # crest projects from its terrace, not a second storey
            for m in families[-1]['members']:
                sampled_patch(m,crest_patch,role='cap' if m['y']<2 else 'wall')
                m['detail']=[round(16*(v/65535)) for v in m['progress']]
    # Wall shoulder joins side face to the south facade. Mixed cap pixels follow
    # the authored diagonal elbow, with west and south structural normals.
    shoulder=((1102,1156),(1139,None),(1144,None),(1178,None))
    family('castle-west-shoulder-crosssection',
           [(mid,x,y,'floor') for y,row in enumerate(shoulder) for x,mid in enumerate(row) if mid is not None],
           [(0,2,0),(1,-1,0),(1,0,8)],'y')
    if families[-1]['id']=='castle-west-shoulder-crosssection':
        def shoulder_patch(x,y):
            if y<16:
                return (min(1,x/15),(-1,0,0)) if x<16 else (1,(0,0,1))
            # x-dependent bevel keeps the shoulder from becoming a flat block.
            z=min(1,max(0,x/15),max(0,(63-y)/47))
            return z,(-1,0,0) if x/15<(63-y)/47 else (0,1,0)
        for m in families[-1]['members']: sampled_patch(m,shoulder_patch,role='corner')
    # Reentrant upper cap elbow only; these cells contain no vertical face.
    family('castle-inner-cap-elbow',[(1183,0,0,'cap')],
           [(0,-1,0),(0,-1,1)])
    # Non-owning final-member guard distinguishes a cap end from1167's ordinary
    # east-wall crosssection. Both final definitions/writers must match; only
    # the top1167 is claimed, never its separately owned lower elbow.
    family('castle-guarded-cap-end',[(1167,0,0,'cap'),(1105,1,0,'cap'),(1183,0,1,'cap')],
           [(0,-1,0),(0,-1,1)])
    if families[-1]['id']=='castle-guarded-cap-end':
        for m in families[-1]['members']:
            if m['y']>=2 or m['x']>=2: m['owns']=False
    # Low courtyard threshold/plinth strips below crest, ending at stair jambs.
    family('castle-stair-left-threshold',[(1189,0,0,'floor'),(1190,1,0,'floor')],
           [(0,4,0)])
    review_v4_families(families, family, rise)
    # Discover exact final patterns to construct contact graph. The portable
    # matcher independently verifies every final member before claiming geometry.
    matches=[]
    for f in families:
        if not f['id'].endswith('crosssection'): continue
        for (x,y),p in bycell.items():
            if p['map16_member']!=0:continue
            mid=p['map16']
            if mid != f['members'][0]['map16']: continue
            if all((q:=bycell.get((x+m['x'],y+m['y']))) and q['map16']==m['map16'] and
                   q['word']==m['word'] and q['map16_member']==m['map16_member'] and
                   q['writer']==bycell[x,y]['writer'] for m in f['members']):
                matches.append((f,x,y))
    edges=[]; observations={}; foot_nodes=set(); endpoint_nodes={}
    # Component cells publish semantic support observations; unlike collision
    # adjacency these are explicitly authored support surface memberships.
    for cell,n in node_at.items(): observations[cell]=n
    def endpoint(cell,label):
        if cell not in observations: observations[cell]=node(label)
        return observations[cell]
    for f,x,y in matches:
        ends={}
        for c in f['contacts']:
            cell=x+c['x'],y+c['y']; n=endpoint(cell,f['id']+' endpoint')
            if c['endpoint'] in ends:
                edges.append(dict(**{'from':ends[c['endpoint']],'to':n},rise=0,
                                  evidence=f['id']+': independently required compatible endpoint contacts'))
            else: ends[c['endpoint']]=n
        endpoint_nodes[f['id'],x,y]=ends
        delta=f.get('signed_rise',rise)
        edges.append(dict(**{'from':ends[1],'to':ends[0]},rise=delta,
                          evidence=f"{f['id']}: authored support rise {delta}; side16/south32px faces; cap excluded"))
        if nodes[ends[1]] == 'semantic-ground': foot_nodes.add(ends[1])
    # Publish exact cap endpoint ownership to stacked assemblies. This is the
    # missing multi-storey capability: a complete final cap member is geometric
    # support for the next storey even when it is not a movement surface. Only
    # uniformly-high patches qualify; a mixed cap/face tile cannot donate a level.
    for f,x,y in matches:
        ends=endpoint_nodes[f['id'],x,y]
        for m in f['members']:
            cap=(m['geometry']=='flat' and m.get('support')==0 and m['role']=='cap') or (
                m['geometry']=='sampled' and m.get('high')==0 and
                all(v==65535 for v in m['progress']))
            if m['role']=='obstacle':
                cap=(m['y']<2 and 0<m['x']<f['width']-1)
            if not cap or not m.get('owns',True): continue
            cell=x+m['x'],y+m['y']
            n=endpoint(cell,f['id']+' exact cap ownership')
            edges.append(dict(**{'from':ends[0],'to':n},rise=0,
                              evidence=f['id']+': complete final cap member at high endpoint (not movement)'))
    # Exact facade seams work across decorated/plain/closed-door slices. Equal
    # endpoints are justified by a shared cap-row and contiguous full final
    # assembly boundaries, never by proximity or a hash. This also supports the
    # unreachable upper storeys without falsely labelling their caps walkable.
    facade=[(f,x,y) for f,x,y in matches if f.get('facade')]
    for f,x,y in facade:
        for g,gx,gy in facade:
            if gy==y and gx==x+f['width']:
                for e in (0,1):
                    edges.append(dict(**{'from':endpoint_nodes[f['id'],x,y][e],
                                         'to':endpoint_nodes[g['id'],gx,gy][e]},rise=0,
                                      evidence='exact contiguous facade cap/foot seams: '+f['id']+' / '+g['id']))
            if gy==y+6 and max(x,gx)<min(x+f['width'],gx+g['width']):
                edges.append(dict(**{'from':endpoint_nodes[f['id'],x,y][1],
                                     'to':endpoint_nodes[g['id'],gx,gy][0]},rise=0,
                                  evidence='user-reviewed stacked storeys: complete upper face foot / lower facade crown'))
    # Compatible neighboring slices of one authored wall run share endpoints;
    # stop at gaps/overwrites/corners/other families. No flood fill through joins.
    for f,x,y in matches:
        dx,dy=(2,0) if f['run_axis']=='x' else (0,2)
        neighbor=endpoint_nodes.get((f['id'],x+dx,y+dy))
        if neighbor is not None:
            for e in (0,1):
                edges.append(dict(**{'from':endpoint_nodes[f['id'],x,y][e],'to':neighbor[e]},rise=0,
                                  evidence=f['id']+': compatible parallel assembly seam'))
    # Floor self contacts for every complete authored metatile; require all
    # members in the same semantic component (no normal-only member extrapolation).
    for (x,y),mid in metatiles.items():
        if mid in GROUND | TERRACE | CAPS and (x,y) in semantic_nodes:
            n=semantic_nodes[x,y]
            if not all(semantic_nodes.get((x+dx,y+dy))==n for dx in range(2) for dy in range(2)):
                observations.pop((x,y),None)
    eligible=[c for c in components if c['node'] in foot_nodes]
    anchors=[]
    if eligible:
        datum=max(eligible,key=lambda c:(c['members'],-c['node']))
        anchors=[dict(node=datum['node'],height=0,evidence='authored datum: largest natural-ground component at proven curtain-wall low contact')]
    # Canonicalize ONLY explicit equal-height seam/contact nodes, so the portable
    # binder sees one ownership node. Positive rises and disconnected supports
    # remain distinct. All original constraints are retained for contradiction
    # detection; there is no averaging or conflict suppression.
    parent=list(range(len(nodes)))
    def root(n):
        while parent[n]!=n:
            parent[n]=parent[parent[n]]; n=parent[n]
        return n
    for e in edges:
        if e['rise']==0: parent[root(e['to'])]=root(e['from'])
    observations={cell:root(n) for cell,n in observations.items()}
    graph=dict(nodes=len(nodes),constraints=edges,anchors=anchors,
               observations=[dict(x=x,y=y,layer=0,surface=0,node=n,
                                  evidence='authored semantic floor/contact topology; NOT Link movement proof')
                             for (x,y),n in sorted(observations.items())])
    return dict(format='ALTPOWR1',pin=pin,families=families,support_graph=graph,
                authored_calibration=dict(rise=rise,south_span=32,side_span=16,south_cap_span=16,
                                          source='declared authored scale, not gameplay measurement',
                                          cap_detail='coarse coplanar cap; fine relief unsupported'),
                semantic_components=components,matched_crosssections=len(matches))


def generate(candidate, core, rise=32):
    candidate['pin']['metadata_revision']='castle-curtain-authored-v4'
    candidate['pin']['floor_rise']=rise
    rules=build_rules(candidate,rise)
    apply_rules(candidate,rules,core)
    # Movement permission is separate from geometric support/cap ownership. This
    # is an authored semantic eligibility inventory, NOT legal Link traversal.
    for p in candidate['placements']:
        p['movement_support']='unverified'
        if p['role'] in ('cap','wall','corner','obstacle'):
            p['movement_support']='not-walkable'
        elif p['role']=='floor' and p['status']=='solved':
            p['movement_support']='eligible' if p['behavior']=='normal' else 'not-walkable'
        p['support_eligible']=p['movement_support']=='eligible'
    candidate['castle_calibration']=rules['authored_calibration']
    candidate['castle_calibration']['crest_cap_seam']={
        'example_cells':[[252,219],[252,220]],'support_base':32,
        'local_step':16,'classification':'authored projecting crest cap relief, not support-level discontinuity',
        'evidence':'exact1184/1112/1253 crest assembly;16-unit raised crown above terrace; gameplay/visual calibration pending'}
    candidate['castle_semantic_components']=rules['semantic_components']
    candidate['castle_matched_crosssections']=rules['matched_crosssections']
    candidate['support_graph']['signed_constraints']=rules['support_graph']['constraints']
    candidate['support_graph']['anchors']=rules['support_graph']['anchors']
    candidate['support_graph']['observations']=rules['support_graph']['observations']
    candidate['support_graph']['note']='Authored semantic support topology and calibrated family contacts; actual Link traversal is unverified.'
    candidate['status']='generated'
    candidate['verdict']='reviewable but incomplete'
    candidate['blockers']=[b for b in candidate['blockers'] if b not in
                           ('calibrated reusable assembly semantics','signed ground/terrace endpoint calibration')]
    candidate['blockers'] += ['unsupported corners/crests/towers and cap fine relief',
                              'authored 32-unit scale and semantic topology need gameplay review']
    recount(candidate)
    candidate['castle_supported_scope']={
        'description':'authored semantic floors and exact straight curtain crosssections, not complete architecture',
        'placements':sum(p['role']!='unknown' for p in candidate['placements']),
        'solved':candidate['coverage']['solved'], 'conflicts':candidate['coverage']['conflicts'],
        'unresolved_classified':candidate['coverage']['classified']}
    # Diagnostic viewport denominator is explicitly distinct from complete area.
    # It may be supplied to a parent replay adapter; it never changes solving.
    rect=(1783,1587,2039,1811)
    view=[p for p in candidate['placements'] if p['x']*8<rect[2] and (p['x']+1)*8>rect[0]
          and p['y']*8<rect[3] and (p['y']+1)*8>rect[1]]
    pixels=Counter()
    for p in view:
        n=(min((p['x']+1)*8,rect[2])-max(p['x']*8,rect[0]))*(min((p['y']+1)*8,rect[3])-max(p['y']*8,rect[1]))
        pixels[p['status']]+=n
    candidate['reference_viewport_inventory']={
        'bounds':list(rect),'map8_cells':len(view),'map_pixels_by_status':dict(pixels),
        'note':'map intersection only; not raster-visible pixels or runtime verification',
        'missing_metatiles':dict(Counter(str(p['map16']) for p in view if p['status']!='solved'))}
    # Intrinsic definition-bank inventory, not a solved-cell-derived whitelist.
    # Castle theme definitions 1102..1284 and 1529..1565 contain the architectural
    # system (including threshold paving). Vegetation/terrain definitions below
    # this bank remain independently visible in the complete-area denominator.
    architecture=[p for p in candidate['placements'] if 1102<=p['map16']<=1284 or 1529<=p['map16']<=1565]
    architecture_view=[p for p in view if 1102<=p['map16']<=1284 or 1529<=p['map16']<=1565]
    def scope_counts(cells):
        return dict(placements=len(cells),by_status=dict(Counter(p['status'] for p in cells)),
                    residual_map16=dict(sorted(Counter(str(p['map16']) for p in cells if p['status']!='solved').items())))
    candidate['castle_architecture_inventory']={
        'definition_scope':'theme36 castle metatile banks1102..1284,1529..1565; inventory scope only, not level assignment',
        'complete_area':scope_counts(architecture),'reference_viewport':scope_counts(architecture_view),
        'excluded_environment_viewport':scope_counts([p for p in view if p not in architecture_view])}
    return candidate,rules
