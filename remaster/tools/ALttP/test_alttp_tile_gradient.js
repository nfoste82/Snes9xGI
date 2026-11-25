'use strict';
const assert=require('node:assert/strict');
const {heightBoundaryGradient:gradient}=require('./alttp_tile_height_ui.js');
const a=[0,1,2,3,4].map(x=>({x,y:0})),b=a.map(p=>({...p,y:4}));
const r=gradient(a,b,48,32);
assert.equal(r.length,25);
for(const p of r)assert.equal(p.height,48-p.y*4);
assert.deepEqual(gradient(b,a,32,48),r);
assert.deepEqual(gradient(a,b,48,32,[{x:2,y:2},{x:2,y:2},{x:10,y:10}]),[{x:2,y:2,height:40}]);
assert.equal(gradient([{x:0,y:0}],[{x:4,y:4}],16,0).length,5);
assert.equal(gradient([{x:0,y:0}],[{x:4,y:0}],-16,0)[2].height,-8);
assert.throws(()=>gradient(a,a,48,32),/overlap/);
assert.throws(()=>gradient([],b,48,32),/both boundaries/);
assert.throws(()=>gradient(a,b,1.5,32),/integers/);
assert.throws(()=>gradient(a,b,48,32,[]),/No target/);
assert.throws(()=>gradient([{x:512,y:0}],b,48,32),/coordinate/);
// Exact sampled-distance weighting for skewed boundaries (not assumed row slope).
const skew=gradient([{x:0,y:0},{x:0,y:4}],[{x:4,y:0},{x:4,y:4}],0,16);
assert.equal(skew.find(p=>p.x===2&&p.y===2).height,8);
// A stepped eastern edge must not be replaced by the chord of its convex hull.
const west=Array.from({length:9},(_,y)=>({x:0,y}));
const east=[2,2,2,3,4,4,4,5,6].map((x,y)=>({x,y}));
const bent=gradient(west,east,16,30);
for(const p of bent)assert(p.x<=east[p.y].x,`Spilled east of boundary at ${p.x},${p.y}`);
for(const p of east)assert.equal(bent.find(q=>q.x===p.x&&q.y===p.y).height,30);
assert(!bent.some(p=>p.x===3&&p.y===2));
assert.deepEqual(gradient(east,west,30,16),bent);
// Transposed top/bottom geometry obeys the same clipping.
const transpose=ps=>ps.map(({x,y})=>({x:y,y:x}));
const turned=gradient(transpose(west),transpose(east),16,30);
for(const p of turned)assert(p.y<=east[p.x].x);
// Inward bends on the west side and masked targets cannot bypass clipping.
const inward=west.map(p=>({...p,x:p.y===4?2:0}));
assert(!gradient(inward,east,16,30).some(p=>p.x===1&&p.y===4));
assert.throws(()=>gradient(west,east,16,30,[{x:3,y:2}]),/No target/);
// Sparse boundary samples connect locally, not by the combined outer hull.
const sparse=gradient([{x:0,y:0},{x:0,y:4}],[{x:2,y:0},{x:2,y:2},{x:6,y:4}],16,30);
assert(!sparse.some(p=>p.x===3&&p.y===2));
// Unequal spans close with straight end caps, filling the wedges all the way
// to each endpoint without extrapolating beyond the connecting segments.
const long=[{x:0,y:0},{x:0,y:8}],short=[{x:4,y:2},{x:4,y:6}];
const capped=gradient(long,short,0,16);
assert(capped.some(p=>p.x===2&&p.y===1));
assert(capped.some(p=>p.x===2&&p.y===7));
assert(!capped.some(p=>p.x===3&&(p.y===1||p.y===7)));
assert(!capped.some(p=>p.x>0&&(p.y===0||p.y===8)));
assert.deepEqual(gradient(short,long,16,0),capped);
const turnedCaps=gradient(transpose(long),transpose(short),0,16);
assert(turnedCaps.some(p=>p.x===1&&p.y===2));
assert(!turnedCaps.some(p=>p.x===1&&p.y===3));
assert.deepEqual(gradient([...long].reverse(),[...short].reverse(),0,16),capped);
assert.deepEqual(gradient(long,short,0,16,[{x:2,y:1},{x:3,y:1}]),[{x:2,y:1,height:8}]);
// Both ends shifted in the same direction: extend different boundaries at
// opposite ends. Preserve a bend on the longer lower-right line (screenshot).
const upper=[{x:0,y:0},{x:4,y:0}],lower=[{x:1,y:4},{x:2,y:4},{x:3,y:5},{x:4,y:6},{x:6,y:6}];
const shifted=gradient(upper,lower,48,32);
assert(shifted.some(p=>p.x===5&&p.y===3));
assert(!shifted.some(p=>p.x===5&&p.y===2));
assert(!shifted.some(p=>p.x===3&&p.y===6));
for(const p of lower)assert.equal(shifted.find(q=>q.x===p.x&&q.y===p.y).height,32);
assert.deepEqual(gradient(lower,upper,32,48),shifted);
console.log('Boundary gradient tests passed');
