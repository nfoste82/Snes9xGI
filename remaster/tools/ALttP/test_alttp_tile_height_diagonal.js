'use strict';
const assert = require('node:assert/strict');
const {calculateHeights, crossingFacing, eastwardSteps, westwardSteps, traversalDelta, diagonalWalk} = require('./alttp_tile_height_worker.js');
const {heightPreviewColor,heightArrivalEdges,heightArrivalLabels} = require('./alttp_tile_height_ui.js');
const floor = () => ({resolved:true,facing:null,north:0,east:0,weight:1});
// Authored placement heights reset incoming paths, not wall weights or geometry.
{
    const cells = [floor(), {...floor(), authoredHeight:10}, floor(), floor()];
    let r = diagonalWalk(4,1,cells,[1,0,0,0],0);
    assert.deepEqual([...r.heights],[0,10,10,10]);
    assert.equal(r.conflicts[1],0);assert(r.authoredResets>0);
    cells[1].authoredHeight=0;
    r=diagonalWalk(4,1,cells,[1,0,0,0],7);
    assert.deepEqual([...r.heights],[7,0,0,0]);
    // Authored values override reference only at the same exact coordinate.
    r=diagonalWalk(4,1,cells,[0,1,0,0],7);
    assert.deepEqual([...r.heights],[0,0,0,0]);
    // Offsets beyond an unresolved tile are not independent seeds.
    cells[1].resolved=false;cells[3].authoredHeight=99;
    r=diagonalWalk(4,1,cells,[1,0,0,0],0);
    assert.equal(r.assigned[3],0);
    function calc(world, authoredHeights, nonuniformHeights=[]) {
        return calculateHeights({world,mode:'manual',bounds:{x0:20,y0:30,width:3,height:1},
            grid:new ArrayBuffer(3*8),tiles:[{annotation:{role:'floor'}}],
            reference:{world,x:20,y:30,height:0},authoredHeights,nonuniformHeights});
    }
    assert.deepEqual([...calc('dark',[{x:21,y:30,height:-4}]).heights],[0,-4,-4]);
    assert.deepEqual([...calc('light',[{x:22,y:30,height:0}]).heights],[0,0,0]);
    // Pixel detail never blocks or modifies scalar traversal, even for callers
    // still sending the former nonuniformHeights field.
    assert.deepEqual([...calc('light',[],[{x:21,y:30}]).heights],[0,0,0]);
    assert.equal(calc('light',[],[{x:21,y:30}]).assigned[2],1);
    assert.deepEqual([...calc('light',[{x:21,y:30,height:16}],[{x:21,y:30}]).heights],[0,16,16]);
    // Multiple disagreeing arrivals at the authored tile all adopt its datum.
    const square=Array.from({length:9},floor);square[4].authoredHeight=20;
    r=diagonalWalk(3,3,square,[1,0,0,0,0,0,0,0,0],0);
    assert.equal(r.heights[4],20);assert.equal(r.conflicts[4],0);
    assert(r.acceptedArrivals[4]);assert(r.authoredResets>0);
    // Wall entry is replaced, but wall exit still contributes normally.
    const wall={...floor(),wall:true,north:2,east:0,weight:2,authoredHeight:30};
    r=diagonalWalk(1,3,[floor(),wall,floor()],[0,0,1],0);
    assert.deepEqual([...r.heights],[30,30,0]);
    r=diagonalWalk(1,3,[floor(),wall,floor()],[1,0,0],40);
    assert.deepEqual([...r.heights],[40,30,28]);
}
// Water remains ordinary floor for propagation; the marker is metadata only.
{
    const grid=new ArrayBuffer(3*3*8);
    function calc(water){return calculateHeights({world:'light',mode:'manual',bounds:{x0:0,y0:0,width:3,height:3},grid,
        tiles:[{annotation:{role:'floor',water},weight:1}],reference:{world:'light',x:1,y:1,height:4}});}
    const dry=calc(false),wet=calc(true);
    for(const key of ['heights','assigned','conflicts','processed','stopReasons'])assert.deepEqual(wet[key],dry[key]);
}
// Doors/doorways match cardinal walls exactly, including flips, weights,
// stopping turns on walls and climb-entry/descend-exit timing.
for(const normal of ['north','south','east','west'])for(const flips of [0,1,2,3]) {
    function doorRun(role,normals=[normal]) {
        const grid=new ArrayBuffer(5*5*8),view=new DataView(grid);
        view.setUint32(12*8,1,true);view.setUint8(12*8+7,flips);
        return calculateHeights({world:'light',mode:'manual',bounds:{x0:0,y0:0,width:5,height:5},grid,
            tiles:[{annotation:{role:'floor'},weight:1},{annotation:{role,normals},weight:2}],
            reference:{world:'light',x:0,y:0,height:0}});
    }
    const wall=doorRun('wall');
    for(const role of ['door','doorway']) {
        const door=doorRun(role);
        for(const key of ['heights','assigned','conflicts','processed','stopReasons'])assert.deepEqual(door[key],wall[key]);
        for(const normals of [[],['north_east'],['north','east']]) {
            const unresolved=doorRun(role,normals);
            assert.equal(unresolved.assigned[12],0);
            assert(unresolved.stopReasons[12]&4);
            assert.equal(unresolved.stats.missingFacings,normals.length?0:1);
        }
    }
}
// Every edge is reversible, including floor/wall transitions, changing weights
// and opposing facings. Diagonal movement still uses the logical heading axis.
const edgeCells=[floor()];
for(const weight of [0,.5,1,2]) {
    for(const facing of [[1,1],[1,-1],[-1,1],[-1,-1]])edgeCells.push({...floor(),facing,weight});
    for(const [north,east] of [[1,0],[-1,0],[0,1],[0,-1]])edgeCells.push({...floor(),north:north*weight,east:east*weight,wall:true,weight});
}
for(const axis of ['north','east'])for(const sign of [1,-1]) {
    for(const source of edgeCells)for(const target of edgeCells) {
        assert.equal(traversalDelta(source,target,axis,sign)+traversalDelta(target,source,axis,-sign),0);
    }
}
// A 1/2/1 diagonal sequence has identical placement heights when the same
// crossing is reversed, for every logical axis and diagonal facing.
for(const axis of ['north','east'])for(const facing of [[1,1],[1,-1],[-1,1],[-1,-1]]) {
    const sign=(axis==='east' ? -facing[0] : facing[1]);
    const path=[floor(),...[1,2,1].map(weight=>({...floor(),facing,weight})),floor()];
    const up=[0],down=Array(5);down[4]=4;
    for(let i=1;i<5;i++)up[i]=up[i-1]+traversalDelta(path[i-1],path[i],axis,sign);
    for(let i=3;i>=0;i--)down[i]=down[i+1]+traversalDelta(path[i+1],path[i],axis,-sign);
    assert.deepEqual(up,[0,1,3,4,4]);assert.deepEqual(down,up);
}
// User's 1/2/1 cardinal wall produces identical placement heights from either
// end, in all orientations and after placement flips.
for(const normal of ['north','south','east','west'])for(const flips of [0,1,2,3]) {
    const horizontal=['east','west'].includes(normal),w=horizontal?5:1,h=horizontal?1:5;
    const grid=new ArrayBuffer(w*h*8),view=new DataView(grid);
    const tiles=[{annotation:{role:'floor'},weight:1},...[1,2,1].map(weight=>({annotation:{role:'wall',normals:[normal]},weight}))];
    for(let i=1;i<=3;i++){view.setUint32(i*8,i,true);view.setUint8(i*8+7,flips);}
    const positive=horizontal ? (normal==='west')!==(!!(flips&1)) : (normal==='north')!==(!!(flips&2));
    const low=positive?0:4,high=4-low;
    function calc(index,height){return calculateHeights({world:'light',mode:'manual',bounds:{x0:0,y0:0,width:w,height:h},grid,tiles,
        reference:{world:'light',x:horizontal?index:0,y:horizontal?0:index,height}});}
    const up=calc(low,0),down=calc(high,4);
    assert.deepEqual([...up.heights],positive?[0,1,3,4,4]:[4,4,3,1,0]);
    assert.deepEqual(down.heights,up.heights);
    assert.equal(up.conflicts.some(Boolean),false);assert.equal(down.conflicts.some(Boolean),false);
}
function run(w,h,ref,placements=[],world='light') {
    const grid=new ArrayBuffer(w*h*8), view=new DataView(grid);
    const tiles=[{annotation:{role:'floor'},weight:1}];
    for(const [x,y,normal,weight=1,flips=0] of placements) {
        tiles.push(normal===null ? null : {annotation:{role:'wall',normals:[normal]},weight});
        view.setUint32((y*w+x)*8,tiles.length-1,true);view.setUint8((y*w+x)*8+7,flips);
    }
    const bytes=Buffer.from(grid).toString('hex'), json=JSON.stringify(tiles);
    const result=calculateHeights({world,mode:'manual',bounds:{x0:107,y0:464,width:w,height:h},
        reference:{world,x:107+ref[0],y:464+ref[1],height:ref[2]??0},grid,tiles});
    assert.equal(Buffer.from(grid).toString('hex'),bytes);assert.equal(JSON.stringify(tiles),json);
    assert.equal(result.stats.computed+result.stats.missing,w*h);
    assert.equal([...result.assigned].reduce((a,b)=>a+b),result.stats.computed);
    assert(result.stats.expandedStates<=w*h*4);
    assert.equal(result.traceFormat,'incoming-neighbors-v1');
    for(let i=0;i<w*h;i++) {
        assert.equal(result.firstArrival[i] & result.acceptedArrivals[i],result.firstArrival[i]);
        assert.equal(!!result.conflictArrivals[i],!!result.conflicts[i]);
        for(const edge of heightArrivalEdges(result.firstArrival[i],result.acceptedArrivals[i],result.conflictArrivals[i])) {
            const x=i%w+edge.dx,y=Math.floor(i/w)+edge.dy;
            assert(x>=0&&x<w&&y>=0&&y<h);assert(result.assigned[y*w+x]);
        }
    }
    for(let i=0;i<w*h;i++)assert.equal(Number.isFinite(result.heights[i]),!!result.assigned[i]);
    return result;
}
// Symmetric destination checks: both signs on each crossing orientation.
const probes=Array.from({length:9},floor);
assert.deepEqual(eastwardSteps(3,3,probes,4),[[1,0]]);
for(const ne of [[-1,1],[1,-1]])for(const se of [[-1,-1],[1,1]]) {
    probes[2].facing=ne;probes[8].facing=se;
    assert.deepEqual(eastwardSteps(3,3,probes,4),[[1,-1],[1,1],[1,0]]);
}
probes[2].facing=[1,1];probes[8].facing=[1,-1];
assert.deepEqual(eastwardSteps(3,3,probes,4),[[1,0]]);
probes[4].facing=[-1,1];probes[5].facing=[-1,1];
assert.deepEqual(eastwardSteps(3,3,probes,4),[]); // No horizontal diagonal-to-diagonal.
probes[2].facing=[1,-1];probes[8].facing=[1,1];
assert.deepEqual(eastwardSteps(3,3,probes,4),[[1,-1],[1,1]]);
probes[5].facing=null;
assert.deepEqual(eastwardSteps(3,3,probes,4),[[1,-1],[1,1],[1,0]]);
assert.deepEqual(eastwardSteps(3,3,probes,5),[[1,0]]); // Caller handles boundary.
const westProbes=Array.from({length:9},floor);
assert.deepEqual(westwardSteps(3,3,westProbes,4),[[-1,0]]);
for(const nw of [[1,1],[-1,-1]])for(const sw of [[1,-1],[-1,1]]) {
    westProbes[0].facing=nw;westProbes[6].facing=sw;
    assert.deepEqual(westwardSteps(3,3,westProbes,4),[[-1,-1],[-1,1],[-1,0]]);
}
westProbes[4].facing=[1,1];westProbes[3].facing=[1,1];
assert.deepEqual(westwardSteps(3,3,westProbes,4),[[-1,-1],[-1,1]]);
westProbes[0].facing=[1,-1];westProbes[6].facing=[1,1];
assert.deepEqual(westwardSteps(3,3,westProbes,4),[]);
westProbes[3].facing=null;
assert.deepEqual(westwardSteps(3,3,westProbes,4),[[-1,0]]);
assert.deepEqual(westwardSteps(3,3,westProbes,3),[[-1,0]]); // Left boundary, no wrap.
assert.deepEqual(crossingFacing({role:'wall',normals:['south_east']},3),[-1,-1]);
assert.equal(crossingFacing({role:'wall',normals:['north','east']}),null);

// Isolated corridors test exact crossing sums without unrelated shorter routes
// claiming the same coordinates first under the new breadth-first propagation.
function corridor(w,h,ref,entries) {
    const cells=Array.from({length:w*h},()=>({...floor(),resolved:false}));
    const anchors=new Uint8Array(w*h);anchors[ref[1]*w+ref[0]]=1;
    for(const [x,y,facing,weight=0] of entries)cells[y*w+x]={...floor(),facing,weight};
    return diagonalWalk(w,h,cells,anchors);
}
const crossing=corridor(6,5,[0,1],[[0,1,null],[1,1,[-1,-1],1],
    [2,2,[-1,-1],2],[3,3,[-1,-1],1],[4,3,null],[5,3,null]]);
assert.equal(crossing.heights[1*6+1],1);
assert.equal(crossing.heights[2*6+2],3);
assert.equal(crossing.heights[3*6+3],4);
assert.equal(crossing.heights[3*6+5],4);
assert.equal(crossing.firstArrival[2*6+2],128); // Actual source NW, not logical W.
assert.equal(crossing.firstArrival[1*6+1],64); // Actual source W.
assert.deepEqual(heightArrivalEdges(128,128|64,16),[
    {dx:0,dy:1,kind:'conflict'},{dx:-1,dy:0,kind:'agree'},
    {dx:-1,dy:-1,kind:'first'}]);
assert.equal(heightArrivalLabels(128|64),'W/NW');
assert.deepEqual(heightArrivalEdges(0,0,0),[]);
assert.equal(heightArrivalLabels(0),'none');
for(const [facing,dy,rise] of [[[-1,1],-1,1],[[1,-1],-1,-1],
    [[-1,-1],1,1],[[1,1],1,-1]]) {
    const y=2;
    const r=corridor(5,5,[0,y],[[0,y,null],[1,y,facing,.5],[2,y+dy,facing,1],[3,y+dy,null]]);
    assert.equal(r.heights[y*5+1],rise>0 ? .5 : 0);
    assert.equal(r.heights[(y+dy)*5+2],rise>0 ? 1.5 : -.5);
    assert.equal(r.heights[(y+dy)*5+3],rise*1.5);
}
// Westward climbs on entry and descends on exit, including the final floor step.
for(const facing of [[1,1],[1,-1],[-1,1],[-1,-1]]) {
    const dy=-facing[0]*facing[1],rise=facing[0];
    const r=corridor(5,5,[4,2],[[4,2,null],[3,2,facing,.5],
        [2,2+dy,facing,1],[1,2+dy,null],[0,2+dy,null]]);
    assert.equal(r.heights[2*5+3],rise>0 ? .5 : 0);
    assert.equal(r.heights[(2+dy)*5+2],rise>0 ? 1.5 : -.5);
    assert.equal(r.heights[(2+dy)*5+1],rise*1.5);
    assert.equal(r.heights[(2+dy)*5],rise*1.5);
}
// From floor, branch to NW and SW diagonal candidates AND directly west.
// This includes the reported (15,475)->(14,474) SE-facing destination shape.
for(const [nw,sw] of [[[1,1],[1,-1]],[[-1,-1],[-1,1]]]) {
    const r=corridor(3,3,[2,1],[[2,1,null],[1,0,nw,1],[1,2,sw,.5],[1,1,null]]);
    for(const [x,y,expected] of [[1,0,Math.max(0,nw[0])],[1,2,Math.max(0,sw[0]*.5)],[1,1,0]]) {
        const i=y*3+x;
        assert.equal(r.heights[i],expected);
        assert(r.processed[i]&8);
    }
    assert.equal(r.firstArrival[1],8); // Actual source SE for NW candidate.
    assert.equal(r.firstArrival[7],2); // Actual source NE for SW candidate.
}
// Cardinal E/W walls use the same destination convention; N/S-facing walls
// have no horizontal contribution. Placement flips still transform facing.
for(const [normal,rise] of [['east',1],['west',-1],['north',0],['south',0]]) {
    for(const flips of [0,1]) {
        const expected=rise===0 ? 0 : rise*.5*(flips ? -1 : 1);
        const west=run(3,1,[2,0],[[1,0,normal,.5,flips]]);
        const east=run(3,1,[0,0],[[1,0,normal,.5,flips]]);
        assert.equal(west.heights[1],Math.max(0,expected));
        assert.equal(west.heights[0],expected);
        assert.equal(east.heights[1],Math.max(0,-expected));
        assert.equal(east.heights[2],expected===0 ? 0 : -expected);
    }
}
// Local (17,472) shape: westward and northward arrivals from zero floor now
// agree at the weight-.5 SE wall; no perpendicular turn at that wall.
const corner=run(3,3,[2,2],[[1,1,'south_east',.5]]);
assert.equal(corner.heights[4],.5);
assert.equal(corner.conflicts[4],0);
assert(corner.acceptedArrivals[4]&4); // From E.
// Earlier diagonal assignment can now suppress the optional perpendicular
// launch from S. Verify that entry independently of wavefront arrival order.
const fromSouth=corridor(3,3,[1,2],[[1,2,null],[1,1,[1,1],.5]]);
assert.equal(fromSouth.heights[4],.5);
assert(fromSouth.acceptedArrivals[4]&16);
// Northward source-facing crossing still applies through propagation.
for(const [facing,dx,rise] of [[[-1,1],1,1],[[-1,-1],-1,-1],
    [[1,1],-1,1],[[1,-1],1,-1]]) {
    const r=corridor(7,5,[3,4],[[3,4,null],[3,3,facing,1],
        [3+dx,2,facing,2],[3+2*dx,1,facing,1]]);
    assert.equal(r.heights[3*7+3],rise>0 ? 1 : 0);
    assert.equal(r.heights[2*7+3+dx],rise>0 ? 3 : -1);
    assert.equal(r.heights[7+3+2*dx],rise>0 ? 4 : -3);
}
// N/S climb on entry and descend on exit, including unequal successive diagonal
// walls and exits to floor. Exercise all diagonal facings in both directions.
for(const sign of [1,-1])for(const facing of [[1,1],[1,-1],[-1,1],[-1,-1]]) {
    const dx=-sign*facing[0]*facing[1],dy=-sign,y=sign>0?4:0,rise=sign*facing[1];
    const r=corridor(9,5,[4,y],[[4,y,null],[4,y+dy,facing,.5],
        [4+dx,y+2*dy,facing,2],[4+2*dx,y+3*dy,null],[4+2*dx,y+4*dy,null]]);
    assert.equal(r.heights[(y+dy)*9+4],rise>0 ? .5 : 0);
    assert.equal(r.heights[(y+2*dy)*9+4+dx],rise>0 ? 2.5 : -.5);
    assert.equal(r.heights[(y+3*dy)*9+4+2*dx],rise*2.5);
    assert.equal(r.heights[(y+4*dy)*9+4+2*dx],rise*2.5);
}
// Verify destination entry with real placement flips in both vertical directions.
for(const normal of ['north_east','north_west','south_east','south_west']) {
    for(const flips of [0,1,2,3])for(const sign of [1,-1]) {
        const facing=crossingFacing({role:'wall',normals:[normal]},flips);
        const r=run(3,3,[1,sign>0?2:0],[[1,1,normal,.5,flips]]);
        assert.equal(r.heights[4],Math.max(0,sign*facing[1]*.5));
    }
}
// Cardinal faces apply the authored weight once (including explicitly doubled
// saved weights), in both directions and after vertical placement flips.
// Neither direction charges the wall a second time when leaving it.
for(const [normal,northDelta] of [['north',-1],['south',1],['east',0],['west',0]]) {
    for(const sign of [1,-1])for(const weight of [.5,1,2])for(const flips of [0,2]) {
        const r=run(1,3,[0,sign>0?2:0],[[0,1,normal,weight,flips]]);
        const expected=northDelta===0?0:sign*northDelta*weight*(flips ? -1 : 1);
        assert.equal(r.heights[1],Math.max(0,expected));
        assert.equal(r.heights[sign>0?0:2],expected);
    }
}
// (47,502) local shape: flipped NE (=NW) wall agrees for incoming NW/W/N.
const southCorner=run(3,3,[0,0],[[1,1,'north_east',1,1]]);
assert.equal(southCorner.heights[4],1);
assert.equal(southCorner.conflicts[4],0);
// Adjacent cardinal north wall is still reachable from floor, but direct entry
// from the diagonal corner is now intentionally blocked.
const cardinalCorner=run(4,3,[0,0],[[1,1,'north_east',1,1],[2,1,'north',1]]);
assert.equal(cardinalCorner.heights[6],1);
assert.equal(cardinalCorner.conflicts[6],0);
assert.equal(cardinalCorner.acceptedArrivals[6]&64,0);
assert(cardinalCorner.stopReasons[5]&32);
// Check southward entry separately: optional perpendicular launches may skip
// an already assigned target in the combined scene above.
const cardinalCells=Array.from({length:9},()=>({...floor(),resolved:false}));
cardinalCells[1]=floor();
cardinalCells[4]={...floor(),north:-1};
const cardinalAnchors=new Uint8Array(9);cardinalAnchors[1]=1;
const cardinalEntry=diagonalWalk(3,3,cardinalCells,cardinalAnchors);
assert.equal(cardinalEntry.heights[4],1);
assert(cardinalEntry.acceptedArrivals[4]&1);
const cornerCells=Array.from({length:9},()=>({...floor(),resolved:false}));
for(const i of [0,1,3])cornerCells[i]=floor();
cornerCells[4]={...floor(),facing:[-1,-1],weight:1};
const threeArrivals=diagonalWalk(3,3,cornerCells,Uint8Array.from([1,1,0,1,0,0,0,0,0]));
assert.equal(threeArrivals.heights[4],1);
assert.equal(threeArrivals.conflicts[4],0);
assert.equal(threeArrivals.acceptedArrivals[4]&(128|64|1),128|64|1);
// A diagonal wall continues only the incoming logical heading, never launches
// perpendicular turns. Cover E/W/N/S, including the (17,472)->(16,471) return
// loop shape: westward SE walls must not spawn a southward step back to entry.
for (const {ref, walls, heading, exit, onward, branch} of [
    {ref:[4,4],walls:[[3,4],[2,3]],heading:8,exit:[1,3],onward:[0,3],branch:[1,2]},
    {ref:[0,0],walls:[[1,0],[2,1]],heading:4,exit:[3,1],onward:[4,1],branch:[3,2]},
    {ref:[3,4],walls:[[3,3],[2,2]],heading:1,exit:[1,1],onward:[1,0],branch:[0,1]},
    {ref:[0,0],walls:[[0,1],[1,2]],heading:2,exit:[2,3],onward:[2,4],branch:[3,3]}
]) {
    const entries=[ [...ref,null], ...walls.map(p=>[...p,[1,1],0]) ];
    const locked=corridor(5,5,ref,entries);
    for(const [x,y] of walls) {
        assert.equal(locked.assigned[y*5+x],1);
        assert.equal(locked.processed[y*5+x],heading,'diagonal wall launched a perpendicular traversal');
    }
    assert.equal(locked.conflicts.some(Boolean),false);
    const resumed=corridor(5,5,ref,[...entries,[...exit,null],[...onward,null],[...branch,null]]);
    for(const [x,y] of [exit,onward,branch]) assert.equal(resumed.assigned[y*5+x],1,
        'cardinal continuation and perpendicular propagation must resume after the diagonal');
}
// Stop cardinal/diagonal boundaries before assignment in either direction and
// every logical heading, including zero-weight walls and wall corners.
for(const [dx,dy,heading] of [[1,0,4],[-1,0,8],[0,-1,1],[0,1,2]]) {
    for(const sourceDiagonal of [false,true])for(const role of ['wall','wall_corner'])for(const weight of [0,1]) {
        const w=7,grid=new ArrayBuffer(w*w*8),view=new DataView(grid),tiles=[null];
        const start=[3-2*dx,3-2*dy];
        // NE makes both vertical source detours point right. Horizontal target
        // selection also accepts the reciprocal NE crossing at this offset.
        const sx=start[0]+dx,sy=start[1]+dy;
        const tx=sx+dx+(dy&&sourceDiagonal ? -dy : 0);
        const ty=sy+dy+(dx ? (sourceDiagonal ? 0 : -dx) : 0);
        function put(x,y,annotation){tiles.push({annotation,weight});view.setUint32((y*w+x)*8,tiles.length-1,true);}
        put(...start,{role:'floor'});
        const cardinal=dx ? 'east' : 'north';
        put(sx,sy,{role,normals:[sourceDiagonal?'north_east':cardinal]});
        put(tx,ty,{role,normals:[sourceDiagonal?cardinal:'north_east']});
        const out=calculateHeights({world:'light',mode:'manual',bounds:{x0:0,y0:0,width:w,height:w},grid,tiles,
            reference:{world:'light',x:start[0],y:start[1],height:0}});
        const source=sy*w+sx,target=ty*w+tx;
        assert(out.processed[source]&heading);
        assert(out.stopReasons[source]&32,'wall-type transition must record its stop at source');
        assert.equal(out.assigned[target],0);
        assert.equal(out.acceptedArrivals[target]|out.conflictArrivals[target]|out.firstArrival[target],0);
        assert.equal(out.conflicts[target],0);
        assert(out.stats.stops.wallTypeBoundary>0);
    }
}
// Crossing any wall must not launch a perpendicular heading. Test cardinal
// cross-wall entry, parallel travel, mixed-axis corners and zero weights; after
// exiting onto floor, perpendicular propagation must resume.
for(const [dx,dy,heading] of [[1,0,4],[-1,0,8],[0,-1,1],[0,1,2]]) {
    for(const normals of [['north'],['south'],['east'],['west'],['north','east']]) {
        for(const weight of [0,1]) {
            const w=7,grid=new ArrayBuffer(w*w*8),view=new DataView(grid),tiles=[null];
            const start=[3-2*dx,3-2*dy],px=-dy,py=dx;
            function put(k,side,annotation) {
                const i=(start[1]+k*dy+side*py)*w+start[0]+k*dx+side*px;
                tiles.push({annotation,weight});view.setUint32(i*8,tiles.length-1,true);return i;
            }
            put(0,0,{role:'floor'});
            const wall=put(1,0,{role:normals.length>1?'wall_corner':'wall',normals});
            const side=put(1,1,{role:'floor'});
            const exit=put(2,0,{role:'floor'}),branch=put(2,-1,{role:'floor'});
            const out=calculateHeights({world:'light',mode:'manual',bounds:{x0:0,y0:0,width:w,height:w},
                reference:{world:'light',x:start[0],y:start[1],height:0},grid,tiles});
            assert.equal(out.processed[wall],heading,'wall must not launch perpendicular headings');
            assert.equal(out.assigned[side],0,'side floor must not be reached by turning on a wall');
            assert.equal(out.assigned[exit],1);
            assert.equal(out.assigned[branch],1,'turning must resume on floor');
        }
    }
}
// Parallel cardinal wall travel is straight-only in all four directions, even
// at weight zero. Nearby matching diagonal destinations must not be checked.
for(const [dx,dy,normals] of [[1,0,['north','south']],[-1,0,['north','south']],
    [0,-1,['east','west']],[0,1,['east','west']]]) {
    for(const normal of normals)for(const weight of [0,1])for(const flips of [0,3]) {
        function parallelCase(withExit) {
            const w=7,grid=new ArrayBuffer(w*w*8),view=new DataView(grid);
            const tiles=[null];
            const start=[3-2*dx,3-2*dy],px=-dy,py=dx;
            function put(k,side,annotation) {
                const x=start[0]+k*dx+side*px,y=start[1]+k*dy+side*py;
                tiles.push({annotation,weight});
                view.setUint32((y*w+x)*8,tiles.length-1,true);
                view.setUint8((y*w+x)*8+7,flips);
                return y*w+x;
            }
            put(0,0,{role:'floor'});
            const wall1=put(1,0,{role:'wall',normals:[normal]});
            const wall2=put(2,0,{role:'wall',normals:[normal]});
            // Matching diagonal candidates alongside the second wall; floor
            // neighbors also catch accidental perpendicular launches.
            const side1=put(1,1,{role:'floor'});
            const side2=put(2,-1,dx ? {role:'wall',normals:[dx>0?'north_east':'south_east']} : {role:'floor'});
            let exit,branch;
            if(withExit) {
                exit=put(3,0,{role:'floor'});
                branch=put(3,1,{role:'floor'});
            }
            const result=calculateHeights({world:'light',mode:'manual',bounds:{x0:0,y0:0,width:w,height:w},
                reference:{world:'light',x:start[0],y:start[1],height:0},tiles,grid});
            const heading=dx ? (dx>0?4:8) : (dy<0?1:2);
            assert.equal(result.processed[wall1],heading);
            assert.equal(result.processed[wall2],heading);
            if(!withExit) {
                assert.equal(result.assigned[side1],0);
                assert.equal(result.assigned[side2],0);
            } else {
                assert.equal(result.assigned[exit],1);
                assert.equal(result.assigned[branch],1,'branching must resume after leaving parallel walls');
            }
        }
        parallelCase(false);parallelCase(true);
    }
}
// Unknown obstruction on the initial column must not leave all rows beyond it
// unset: perpendicular paths go around it using real zero-delta floor steps.
const around=run(7,7,[3,6],[[3,3,null]]);
assert.equal(around.stats.computed,48);assert.equal(around.stats.missing,1);
assert.equal(around.assigned[3*7+3],0);assert(around.stopReasons[3*7+3]&4);
assert.equal(around.heights[0],0);assert.equal(around.heights[3],0);
assert.equal(around.stats.conflictCount,0);
assert.equal(around.acceptedArrivals[3*7+3],0); // Blocked targets have no arrival arrows.
// N/S detour around an obstruction in the same row, not only horizontal paths.
const aroundHorizontal=run(7,7,[0,3],[[3,3,null]]);
assert.equal(aroundHorizontal.stats.computed,48);
assert.equal(aroundHorizontal.heights[3*7+6],0);
// An entire unresolved barrier really blocks propagation. No backfilling.
const barrier=run(7,7,[0,3],Array.from({length:7},(_,y)=>[3,y,null]));
assert.equal(barrier.stats.computed,21);assert.equal(barrier.stats.missing,28);
assert(Number.isNaN(barrier.heights[3*7+6]));
const flat=run(6,5,[2,2,7]);assert.equal(flat.stats.missing,0);
assert.equal(flat.firstArrival[2*6+2],0); // Reference has no invented parent edge.
assert.deepEqual([...flat.heights],Array(30).fill(7));assert.equal(flat.stats.conflictCount,0);
// Arrival order changes with the wavefront; competing routes stay explicit,
// never smoothed or overwritten. Both worlds run identical compact inputs.
const scene=[[1,1,'north_west',1],[2,2,'north_west',2],[3,3,'north_west',1]];
const light=run(7,6,[0,5],scene),dark=run(7,6,[0,5],scene,'dark');
assert.deepEqual(light.heights,dark.heights);assert.deepEqual(light.conflicts,dark.conflicts);
assert(light.stats.conflictCount>0);assert(light.stopReasons.some(b=>b&8));
// Independently exercise repeated-heading stopping without depending on turns
// at diagonal walls in the mixed scene above.
const repeated=diagonalWalk(3,1,Array.from({length:3},floor),Uint8Array.from([1,1,1]));
assert(repeated.stops.repeated>0);
assert.deepEqual(light.heights,run(7,6,[0,5],scene).heights); // Deterministic.
assert.deepEqual(heightPreviewColor(NaN,0,0,0,4,0,0),[70,0,90,255]);
assert.deepEqual(heightPreviewColor(NaN,0,0,0,4,1,0),[255,0,220,255]);
assert.deepEqual(heightPreviewColor(0,1,0,0,4,0,0),[0,0,0,255]);
assert.deepEqual(heightPreviewColor(4,1,1,0,4,0,0),[255,145,0,255]);
console.log('Symmetric crossings, perpendicular propagation, barriers, bounds and conflicts passed.');
