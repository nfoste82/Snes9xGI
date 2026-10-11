"""Inspect actual accepted solver equalities; exhibit unresolved free datums."""
import argparse
import collections
import json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('prefix',type=Path);p.add_argument('--audit',type=Path,required=True);p.add_argument('--before-corpus',type=Path);p.add_argument('--after-corpus',type=Path);a=p.parse_args()
tiles=json.loads(Path(str(a.prefix)+'-placements.json').read_text())
records=[json.loads(line) for line in a.audit.read_text().splitlines()]
actual=next(r for r in records if r['bg']==1 and r['active']==[0,1])
adj=collections.defaultdict(list)
for u,v,d in actual['edges']:adj[u].append((v,d));adj[v].append((u,-d))
nodes={v[0]:v for v in actual['nodes']}; bynode=collections.defaultdict(list)
for v in actual['nodes']:bynode[v[1]].append(v[0])
seen=set();groups=[]
for encoded,v in nodes.items():
    if tiles[encoded%4096]['surface']!=2 or tiles[encoded%4096]['base']!=255 or v[1] in seen:continue
    if encoded//4096!=0:continue # unobserved structural ownership only
    q=[v[1]];relative={v[1]:0};seen.add(v[1]);conflicts=[]
    for u in q:
        for n,d in adj[u]:
            if n not in relative:relative[n]=relative[u]+d;q.append(n);seen.add(n)
            elif relative[n]!=relative[u]+d:conflicts.append([u,n])
    cells=sorted(e for n in q for e in bynode[n])
    walls=[e for e in cells if tiles[e%4096]['surface']==2]
    groups.append(dict(wall_cells_plane_xy=[[e//4096,e%64,(e%4096)//64] for e in walls],nodes=len(q),
        support_nodes=sum(any(nodes[e][4] for e in bynode[n]) for n in q),
        flat_nodes=sum(any(nodes[e][5] for e in bynode[n]) for n in q),conflicts=conflicts,
        relative_span=[min(relative.values()),max(relative.values())],
        free_datum=not any(nodes[e][4] or nodes[e][5] for e in cells),
        witness_two_solutions=[dict(datum=d,cell=walls[0] if walls else encoded,base=d+relative[v[1]]) for d in (0,50)]))
counterfactual=next(r for r in records if r['bg']==1 and r['active']==[1,0])
cf={v[0]:v for v in counterfactual['nodes']}
for g in groups:
    cells=[p*4096+y*64+x for p,x,y in g['wall_cells_plane_xy']]
    g['plane0_observation_would_solve']=sum(e in cf and cf[e][2]!=-2147483648 and not cf[e][3] for e in cells)
report=dict(groups=groups,missing_nodes=[dict(cell=t['cell'],template=t['template'],attrs=t['attrs']) for t in tiles if t['surface']==2 and t['base']==255 and t['cell'] not in nodes],
            caveat='Two datums illustrate identical accepted seam constraints, not approved elevations.')
if a.before_corpus and a.after_corpus:
    before=json.loads(a.before_corpus.read_text());after=json.loads(a.after_corpus.read_text())
    deltas=[]
    for x,y in zip(before['records'],after['records']):
        assert x['snapshot']==y['snapshot']
        if x['unknown_walls']!=y['unknown_walls'] or x['conflicts']!=y['conflicts']:deltas.append(dict(room=x['room'],snapshot=x['snapshot'],walls_before=x['unknown_walls'],walls_after=y['unknown_walls'],conflicts_before=x['conflicts'],conflicts_after=y['conflicts']))
    report['corpus']=dict(contexts=after['contexts'],rooms=after['rooms'],changed_contexts=len(deltas),changed_rooms=len({v['room'] for v in deltas}),
        unknown_before=sum(v['unknown_walls'] for v in before['records']),unknown_after=sum(v['unknown_walls'] for v in after['records']),
        conflicts_before=sum(v['conflicts'] for v in before['records']),conflicts_after=sum(v['conflicts'] for v in after['records']),deltas=deltas)
    if all('obstacle_assembly_cells' in v for v in after['records']):
        report['corpus']['obstacles']=dict(assemblies=sum(v['obstacle_assemblies'] for v in after['records']),
            placements=sum(v['obstacle_assembly_cells'] for v in after['records']),
            solved=sum(v['obstacle_assembly_solved'] for v in after['records']),
            missing=sum(v['obstacle_assembly_cells']-v['obstacle_assembly_solved'] for v in after['records']),
            conflicting_part_bases=sum(v['obstacle_part_base_conflicts'] for v in after['records']))
Path(str(a.prefix)+'-component-report.json').write_text(json.dumps(report,indent=2)+'\n')
print('unresolved groups',len(groups),'free',sum(g['free_datum'] for g in groups),'missing nodes',len(report['missing_nodes']))
if 'corpus' in report:
    print({k:v for k,v in report['corpus'].items() if k!='deltas'})
    print('changed contexts',report['corpus']['deltas'])
for g in groups[:8]:print('group',g['wall_cells_plane_xy'][:4],{k:v for k,v in g.items() if k!='wall_cells_plane_xy'})
