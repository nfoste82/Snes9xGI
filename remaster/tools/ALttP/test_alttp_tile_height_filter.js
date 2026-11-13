'use strict';
const assert = require('node:assert/strict');
const {filterContributions: filter, isDiagonal, heightDeltas, anchoredWalk} = require('./alttp_tile_height_worker.js');
// Strings are in traversal order, northward or eastward, not screen row order.
function example(run, expected) {
    const walls = [...run].map(c => c !== 'G' ? 1 : 0);
    const diagonals = [...run].map(c => c === 'D' ? 1 : 0);
    const raw = walls.map((v, i) => v * (i + .25)); // preserve signed/fractional magnitudes
    const before = JSON.stringify({walls, diagonals, raw});
    const east = filter(run.length, 1, raw, raw, walls, diagonals);
    assert.deepEqual([...east.countEast], [...expected].map(Number), run);
    assert.deepEqual([...east.east], raw.map((v, i) => expected[i] === '1' ? v : 0));
    const reverse = a => [...a].reverse();
    const north = filter(1, run.length, reverse(raw), reverse(raw), reverse(walls), reverse(diagonals));
    assert.deepEqual(reverse(north.countNorth), [...expected].map(Number), 'N ' + run);
    assert.deepEqual(reverse(north.north), [...east.east]);
    assert.equal(JSON.stringify({walls, diagonals, raw}), before);
    return east;
}
example('GDDDG', '01010');
example('GDDDDG', '010110');
example('CDDDG', '10110'); // cardinal, skip, count, terminal override
example('CDG', '110');
example('GDDGDDDG', '01101010'); // floor gaps reset, do not merge runs
example('GDD', '010'); // outside is unknown: NO terminal override
example('CDD', '101');
example('DDDD', '1010'); // cropped run starts count at visible boundary
example('GCCDDG', '011010');
const three = filter(5,1,[0,1,1,1,0],[0,1,1,1,0],[0,1,1,1,0],[0,1,1,1,0]);
assert.equal([...three.east].reduce((a,b)=>a+b),2); // user: 3 unit diagonals count twice
const northFacing = {role:'wall',normals:['north_east']};
const [dn,de] = heightDeltas(northFacing,0,.4);
assert.equal(dn,-.4); assert.equal(de,-.4);
const signed = filter(5,1,[0,dn,dn,dn,0],[0,de,de,de,0],[0,1,1,1,0],[0,1,1,1,0]);
assert.deepEqual([...signed.east],[0,-.4,0,-.4,0]);
assert.equal(isDiagonal({role:'wall_corner', normals:['north','east']}), false);
assert.equal(isDiagonal({role:'wall_corner', normals:['north_east']}), true);
assert.equal(isDiagonal({role:'floor', normals:['north_east']}), false);
// Opposite diagonal facings still alternate. Cardinal E has zero N but sets phase.
const annotations = [
    {role:'floor'}, {role:'wall',normals:['east']},
    {role:'wall',normals:['north_east']}, {role:'wall',normals:['south_west']},
    {role:'wall',normals:['north_west']}, {role:'floor'}];
for (const flips of [0,1,2,3]) {
    const ds = annotations.map((a,i) => heightDeltas(a, flips, i === 1 ? 0 : .25));
    const wall = annotations.map(a => a.role === 'wall' ? 1 : 0);
    const diag = annotations.map(isDiagonal);
    const rawN = ds.map(d => d[0]), rawE = ds.map(d => d[1]);
    const out = filter(6,1,rawN,rawE,wall,diag);
    assert.deepEqual([...out.countEast],[0,1,0,1,1,0]);
    assert.deepEqual([...out.east],rawE.map((v,i) => i === 2 ? 0 : v));
    const rev = a => [...a].reverse();
    const n = filter(1,6,rev(rawN),rev(rawE),rev(wall),rev(diag));
    assert.deepEqual(rev(n.north),rawN.map((v,i) => i === 2 ? 0 : v));
    // S and W subtract the SAME filtered contribution of the cell left.
    for (const anchorIndex of [0,2,4,5]) {
        const anchor = Array(6).fill(0); anchor[anchorIndex] = 1;
        const eHeights = anchoredWalk(6,1,rawN,out.east,anchor,3.25).heights;
        const nHeights = anchoredWalk(1,6,n.north,rawE,anchor,3.25).heights;
        for (let i=1;i<6;i++) {
            assert.equal(eHeights[i]-eHeights[i-1],out.east[i] || 0);
            assert.equal(nHeights[i-1]-nHeights[i],n.north[i-1] || 0);
        }
    }
}
// Missing-facing walls remain walls with zero raw contribution, not ground.
const missing = filter(4,1,[0,0,1,0],[0,0,1,0],[0,1,1,0],[0,0,1,0]);
assert.deepEqual([...missing.countEast],[0,1,1,0]);
const missingInterior = filter(5,1,[0,0,1,1,0],[0,0,1,1,0],[0,1,1,1,0],[0,0,1,1,0]);
assert.deepEqual([...missingInterior.countEast],[0,1,0,1,0]);
// Browser Worker response includes/ transfers actual fields, not just raw heights.
const fs = require('node:fs'), vm = require('node:vm');
const messages = [], self = {postMessage: (message, transfer) => messages.push({message, transfer})};
vm.runInNewContext(fs.readFileSync(require.resolve('./alttp_tile_height_worker.js'),'utf8'), {self});
self.onmessage({data:{id:17,input:{world:'dark',mode:'manual',
    bounds:{x0:91,y0:300,width:1,height:1},grid:new ArrayBuffer(8),
    reference:{world:'dark',x:91,y:300,height:4.5},tiles:[{annotation:{role:'floor'}}]}}});
const response = messages.at(-1);
assert.equal(response.message.id,17);
assert.equal(response.transfer.length,8);
assert.equal(response.transfer[0],response.message.heights.buffer);
assert.equal(response.transfer[1],response.message.assigned.buffer);
assert.equal(response.transfer[2],response.message.conflicts.buffer);
assert.equal(response.transfer[3],response.message.processed.buffer);
assert.equal(response.transfer[4],response.message.stopReasons.buffer);
assert.equal(response.transfer[5],response.message.firstArrival.buffer);
assert.equal(response.transfer[6],response.message.acceptedArrivals.buffer);
assert.equal(response.transfer[7],response.message.conflictArrivals.buffer);
assert.equal(response.message.heights[0],4.5);
console.log('Explicit count/skip examples, flips, fractions, readonly inputs and inverse travel pass.');
