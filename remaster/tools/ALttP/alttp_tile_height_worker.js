/* diagonal-propagation-v37-1: editor experiment, not validated runtime geometry.
 * CommonJS export is only for portable tests; browser runs this in a Worker. */
'use strict';
const HEIGHT_VERSION = 'diagonal-propagation-v37-1';
const WALL_ROLES = ['wall', 'wall_corner', 'door', 'doorway'];
// Source offsets relative to destination, clockwise from N. These are actual
// neighboring coordinates, NOT logical traversal headings through a detour.
const ARRIVAL_OFFSETS = [[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1]];
const SAND_HASHES = ['07f409cad52651eb', '73b51bc652b27b3b',
    '6273391914a22861', 'df708552688f6d4b'].map(h => 'v1:4bpp:' + h);

function isDiagonal(annotation) {
    // Explicit diagonal facing only: two separate cardinal normals are NOT diagonal.
    // Placement flips change signs but cannot change this classification.
    return ['wall', 'wall_corner'].includes(annotation?.role) &&
        (annotation.normals || []).some(n => /^(north|south)_(east|west)$/.test(n));
}

function heightDeltas(annotation, flips = 0, weight = 1) {
    if (!WALL_ROLES.includes(annotation?.role)) return [0, 0];
    let fx = 0, fy = 0;
    for (const normal of annotation.normals || []) {
        for (const direction of normal.split('_')) {
            fx += direction === 'east' ? 1 : direction === 'west' ? -1 : 0;
            fy += direction === 'south' ? 1 : direction === 'north' ? -1 : 0;
        }
    }
    if (flips & 1) fx = -fx;
    if (flips & 2) fy = -fy;
    return [(fy > 0 ? 1 : fy < 0 ? -1 : 0) * weight,
        (fx > 0 ? -1 : fx < 0 ? 1 : 0) * weight];
}

// Phase starts at each visible line boundary (no hidden out-of-bounds lookups).
// Cardinal walls, including zero-weight/zero-axis/missing-facing walls, count and
// set the next diagonal to skip. Nonwalls reset; facings never split a wall run.
// A terminal diagonal overrides skip ONLY when the next cell is a known nonwall.
function filterContributions(width, height, rawNorth, rawEast, walls, diagonals) {
    const north = Float64Array.from(rawNorth), east = Float64Array.from(rawEast);
    const countNorth = new Uint8Array(width * height), countEast = new Uint8Array(width * height);
    function line(start, step, length, raw, filtered, counted) {
        let skip = false;
        for (let k = 0, i = start; k < length; k++, i += step) {
            if (!walls[i]) { skip = false; continue; }
            if (!diagonals[i]) { counted[i] = 1; skip = true; continue; }
            const terminal = k + 1 < length && !walls[i + step];
            const count = !skip || terminal;
            counted[i] = Number(count);
            filtered[i] = count ? raw[i] : 0;
            skip = count;
        }
    }
    for (let x = 0; x < width; x++) line((height - 1) * width + x, -width, height, rawNorth, north, countNorth);
    for (let y = 0; y < height; y++) line(y * width, 1, width, rawEast, east, countEast);
    return {north, east, countNorth, countEast};
}

// Pure walk also accepts small arrays for exact synthetic regression tests.
function anchoredWalk(width, height, north, east, anchor, referenceHeight = 0) {
    const heights = new Float64Array(width * height);
    const seeded = [];
    for (let x = 0; x < width; x++) {
        let start = -1;
        for (let y = 0; y < height; y++) if (anchor[y * width + x]) start = y;
        if (start < 0) continue;
        seeded.push(x);
        heights[start * width + x] = referenceHeight;
        for (let y = start - 1; y >= 0; y--) {
            const i = y * width + x;
            heights[i] = anchor[i] ? referenceHeight : heights[i + width] + north[i];
        }
        for (let y = start + 1; y < height; y++) {
            const i = y * width + x;
            heights[i] = heights[i - width] - north[i - width];
        }
    }
    if (!seeded.length) throw Error('No original sand anchors in scope. Select Light World and check ROM/aliases.');
    const seedSet = new Set(seeded);
    for (let y = 0; y < height; y++) {
        // Row prefix preserves the enter-cell / leave-cell convention exactly.
        const prefix = new Float64Array(width + 1);
        for (let x = 0; x < width; x++) prefix[x + 1] = prefix[x] + east[y * width + x];
        for (let x = 0; x < width; x++) {
            if (seedSet.has(x)) continue;
            let source = seeded[0];
            for (const candidate of seeded) {
                if (Math.abs(candidate - x) < Math.abs(source - x)) source = candidate;
            }
            heights[y * width + x] = heights[y * width + source] + prefix[x + 1] - prefix[source + 1];
        }
    }
    return {heights, seededColumns: seeded.length};
}

// A single explicit diagonal after placement flips; ambiguous faces are not guessed.
function crossingFacing(annotation, flips = 0) {
    if (!['wall', 'wall_corner'].includes(annotation?.role)) return null;
    const normals = annotation.normals || [];
    if (normals.length !== 1) return null;
    const match = /^(north|south)_(east|west)$/.exec(normals[0]);
    if (!match) return null;
    return [(match[2] === 'east' ? 1 : -1) * (flips & 1 ? -1 : 1),
        (match[1] === 'south' ? 1 : -1) * (flips & 2 ? -1 : 1)];
}

// Eastward branches: NE SW/NE-facing, SE NW/SE-facing, plus E
// except diagonal-to-diagonal: those crossings must be diagonal, never lateral.
// Do not use assignment state to choose a different path or bypass a conflict.
// Resolved/ambiguous faces are handled by the caller, not guessed here.
function eastwardSteps(width, height, cells, i) {
    return horizontalSteps(width, height, cells, i, 1);
}
function westwardSteps(width, height, cells, i) {
    return horizontalSteps(width, height, cells, i, -1);
}
// Reciprocal westward branches: NW SE/NW-facing, SW NE/SW-facing, plus W
// under the same diagonal-to-diagonal lateral restriction as eastward travel.
function horizontalSteps(width, height, cells, i, sign) {
    const x = i % width, y = Math.floor(i / width);
    const steps = [];
    if (cells[i].straightAxes & 1) return [[sign, 0]];
    if (x + sign < 0 || x + sign >= width) return [[sign, 0]];
    for (const dy of [-1, 1]) {
        if (y + dy < 0 || y + dy >= height) continue;
        const target = cells[(y + dy) * width + x + sign];
        if (target.facing && sign * target.facing[0] * target.facing[1] === dy) steps.push([sign, dy]);
    }
    if (!(cells[i].facing && cells[y * width + x + sign].facing)) steps.push([sign, 0]);
    return steps;
}

// Climbing belongs to the wall entered; descending belongs to the wall left.
// Evaluate both endpoints independently so floor entry/exit and facing changes
// use the same reversible edge rule, without delayed mutable traversal state.
function traversalDelta(source, target, axis, sign) {
    function signed(cell) {
        const delta = cell.facing ? (axis === 'east' ? -cell.facing[0] : cell.facing[1]) * cell.weight : cell[axis];
        return sign * delta;
    }
    return Math.max(0, signed(target)) + Math.min(0, signed(source));
}

// Breadth-first directional wavefront. Each accepted arrival continues its
// heading and offers perpendicular traversals toward unassigned neighbors only
// on non-wall tiles. Wall crossings keep their originating heading until
// they exit onto a non-wall tile; intermediate walls cannot launch turns.
// Every step still uses the wall crossing rules; no interpolation/gap filling.
function diagonalWalk(width, height, cells, anchors, referenceHeight = 0) {
    const size = width * height;
    const heights = new Float64Array(size).fill(NaN);
    const assigned = new Uint8Array(size), conflicts = new Uint8Array(size);
    const processed = new Uint8Array(size), stopReasons = new Uint8Array(size);
    const firstArrival = new Uint8Array(size), acceptedArrivals = new Uint8Array(size), conflictArrivals = new Uint8Array(size);
    const seedColumns = new Set();
    const queue = [], scheduled = new Uint8Array(size);
    // Stable reference launch order, then continuation before perpendiculars.
    const directions = [{axis:'north',sign:1,bit:1,dx:0,dy:-1},
        {axis:'north',sign:-1,bit:2,dx:0,dy:1},
        {axis:'east',sign:1,bit:4,dx:1,dy:0},
        {axis:'east',sign:-1,bit:8,dx:-1,dy:0}];
    let anchorsCount = 0;
    const stops = {boundary: 0, repeated: 0, conflict: 0, unresolved: 0, diagonalOnly: 0, wallTypeBoundary: 0};
    let agreeingArrivals = 0, authoredArrivals = 0, authoredResets = 0;
    // Per-cell diagnostic bits: boundary=1, repeated heading=2,
    // unresolved metadata=4, conflicting arrival=8, no legal diagonal continuation=16,
    // cardinal/diagonal wall transition=32 (recorded at the tile being left).
    // A cell can have several.
    function stop(i, reason, bit) { stops[reason]++; stopReasons[i] |= bit; }
    function schedule(i, direction) {
        if (scheduled[i] & direction.bit) return;
        scheduled[i] |= direction.bit;
        queue.push({i, direction});
    }
    for (let i = 0; i < size; i++) if (anchors[i]) {
        heights[i] = Number.isFinite(cells[i].authoredHeight) ? cells[i].authoredHeight : referenceHeight; assigned[i] = 1;
        anchorsCount++; seedColumns.add(i % width);
        for (const direction of directions) schedule(i, direction);
    }
    if (!anchorsCount) throw Error('No original sand anchors in scope. Select a reference in view.');
    function perpendiculars(i, direction) {
        if (cells[i].wall || cells[i].facing || cells[i].straightAxes) return;
        const x = i % width, y = Math.floor(i / width);
        for (const other of directions) {
            if (other.axis === direction.axis) continue;
            const nx = x + other.dx, ny = y + other.dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height && !assigned[ny * width + nx]) schedule(i, other);
        }
    }
    for (let cursor = 0; cursor < queue.length; cursor++) {
            const {i, direction} = queue[cursor];
            const {axis, sign, bit: heading} = direction;
            if (processed[i] & heading) { stop(i, 'repeated', 2); continue; }
            processed[i] |= heading;
            const source = cells[i];
            if (!source.resolved) { stop(i, 'unresolved', 4); continue; }
            perpendiculars(i, direction);
            let dx = axis === 'east' ? sign : 0, dy = axis === 'north' ? -sign : 0;
            if (source.facing) {
                const [fx, fy] = source.facing;
                if (axis === 'east') dy = sign * fx * fy;
                else dx = -sign * fx * fy;
            }
            const steps = axis === 'east' ? horizontalSteps(width, height, cells, i, sign) : [[dx, dy]];
            if (!steps.length) stop(i, 'diagonalOnly', 16);
            for (let k = 0; k < steps.length; k++) {
                const [stepX, stepY] = steps[k];
                const x = i % width + stepX, y = Math.floor(i / width) + stepY;
                if (x < 0 || x >= width || y < 0 || y >= height) { stop(i, 'boundary', 1); continue; }
                const next = y * width + x, target = cells[next];
                if (!target.resolved) { stop(next, 'unresolved', 4); continue; }
                // Stop before transferring any height or arrival across a wall
                // type boundary. Sibling candidates and independent arrivals
                // remain valid; floor transitions are deliberately unaffected.
                if ((source.wall || source.facing) && (target.wall || target.facing) &&
                    !!source.facing !== !!target.facing) {
                    stop(i, 'wallTypeBoundary', 32); continue;
                }
                const delta = traversalDelta(source, target, axis, sign);
                 const incoming = heights[i] + delta;
                 const authored = Number.isFinite(target.authoredHeight);
                 const value = authored ? target.authoredHeight : incoming;
                 if (authored) {
                     authoredArrivals++;
                     if (Math.abs(incoming - value) > 1e-9) authoredResets++;
                 }
                const arrivalBit = 1 << ARRIVAL_OFFSETS.findIndex(([sx, sy]) => sx === -stepX && sy === -stepY);
                if (assigned[next]) {
                    if (Math.abs(value - heights[next]) > 1e-9) {
                        conflictArrivals[next] |= arrivalBit;
                        conflicts[next] = 1; stop(next, 'conflict', 8); continue;
                    }
                    agreeingArrivals++;
                } else {
                    firstArrival[next] = arrivalBit;
                    heights[next] = value; assigned[next] = 1;
                }
                acceptedArrivals[next] |= arrivalBit;
                if (processed[next] & heading) stop(next, 'repeated', 2);
                else schedule(next, direction);
                perpendiculars(next, direction);
            }
    }
    return {heights, assigned, conflicts, processed, stopReasons, firstArrival, acceptedArrivals, conflictArrivals, stops, agreeingArrivals, authoredArrivals, authoredResets,
        seededColumns: seedColumns.size, expandedStates: queue.length};
}

function calculateHeights(input, progress = () => {}) {
    if (!['light', 'dark'].includes(input.world)) throw Error('Invalid world');
    const {x0, y0, width, height} = input.bounds || {};
    if (![x0, y0, width, height].every(Number.isInteger) || x0 < 0 || y0 < 0 ||
        width < 1 || height < 1 || x0 + width > 512 || y0 + height > 512) {
        throw Error('Invalid visible bounds: expected a nonempty rectangle within 512×512 cells');
    }
    const grid = new DataView(input.grid);
    if (grid.byteLength !== width * height * 8) throw Error('Expected compact placement grid matching visible bounds');
    const preset = input.mode === 'legacy-sand';
    if (preset && input.world !== 'light') throw Error('Desert sand anchors are Light World only');
    if (!preset && input.mode !== 'manual') throw Error('Choose manual reference or explicit legacy-sand mode');
    const ref = input.reference;
    if (!preset && (!ref || ref.world !== input.world || !Number.isInteger(ref.x) ||
        !Number.isInteger(ref.y) || !Number.isFinite(ref.height))) throw Error('Select a visible tile as reference and enter a finite height for this world');
    if (!preset && (ref.x < x0 || ref.x >= x0 + width || ref.y < y0 || ref.y >= y0 + height)) {
        throw Error('Reference is outside visible bounds. Select a visible tile and use it as reference.');
    }
    const north = new Float64Array(width * height), east = new Float64Array(width * height);
    const anchor = new Uint8Array(width * height);
    const cells = [];
    const authoredHeights = new Map();
    for (const entry of input.authoredHeights || []) {
        if (!Number.isInteger(entry.x) || !Number.isInteger(entry.y) || !Number.isFinite(entry.height))
            throw Error('Invalid authored placement height');
        authoredHeights.set(`${entry.x}:${entry.y}`, entry.height);
    }
    const nonuniform = new Set((input.nonuniformHeights || []).map(entry => `${entry.x}:${entry.y}`));
    let anchors = 0, unknownRoles = 0, missingSources = 0, missingFacings = 0, contextDependent = 0;
    progress('Resolving visible placement facings and explicit reference…');
    for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
        const i = y * width + x, offset = i * 8;
        const tile = input.tiles[grid.getUint32(offset, true)];
        if (!tile) missingSources++;
        const a = tile?.annotation;
        if (!a || a.role === 'unknown') unknownRoles++;
        if (a?.context_dependent) contextDependent++;
        if (WALL_ROLES.includes(a?.role) && !(a.normals || []).length) missingFacings++;
        [north[i], east[i]] = heightDeltas(a, grid.getUint8(offset + 7), tile?.weight ?? 8);
        const wall = WALL_ROLES.includes(a?.role);
        const facing = crossingFacing(a, grid.getUint8(offset + 7));
        const knownRole = ['floor', ...WALL_ROLES, 'obstacle', 'other'].includes(a?.role);
        const door = ['door', 'doorway'].includes(a?.role);
        const knownNormals = a?.normals?.length && a.normals.every(n => /^(north|south|east|west|(?:north|south)_(?:east|west))$/.test(n)) &&
            (!door || a.normals.length === 1 && /^(north|south|east|west)$/.test(a.normals[0]));
        // Axis-parallel cardinal faces allow straight continuation only. Use
        // semantic normals, not height deltas: zero weights/opposing normals
        // must not disable this rule, and floors must not acquire it.
        const straightAxes = wall && knownNormals ?
            (a.normals.every(n => n === 'north' || n === 'south') ? 1 :
                a.normals.every(n => n === 'east' || n === 'west') ? 2 : 0) : 0;
        cells.push({north: north[i], east: east[i], facing, weight: tile?.weight ?? 8,
            straightAxes, wall, authoredHeight: authoredHeights.get(`${x+x0}:${y+y0}`),
            resolved: !nonuniform.has(`${x+x0}:${y+y0}`) && !!tile && knownRole && (!wall || !!knownNormals && (!isDiagonal(a) || !!facing))});
        if (preset ? x + x0 < 256 && y + y0 >= 492 && tile?.sand : x + x0 === ref.x && y + y0 === ref.y) {
            anchor[i] = 1; anchors++;
        }
    }
    progress('Propagating directional wavefront with diagonal wall crossings (no interpolation)…');
    const result = diagonalWalk(width, height, cells, anchor, preset ? 0 : ref.height);
    let min = Infinity, max = -Infinity;
    let computed = 0, conflictCount = 0;
    for (let i = 0; i < result.heights.length; i++) {
        if (result.assigned[i]) { computed++; min = Math.min(min, result.heights[i]); max = Math.max(max, result.heights[i]); }
        conflictCount += result.conflicts[i];
    }
    return {world: input.world, bounds: {x0, y0, width, height}, reference: preset ? null : {...ref},
        heights: result.heights, assigned: result.assigned, conflicts: result.conflicts,
        processed: result.processed, stopReasons: result.stopReasons,
        traceFormat: 'incoming-neighbors-v1', firstArrival: result.firstArrival,
        acceptedArrivals: result.acceptedArrivals, conflictArrivals: result.conflictArrivals,
        stats: {version: HEIGHT_VERSION, world: input.world,
        mode: input.mode, x0, y0, width, height,
        min, max, anchors, seededColumns: result.seededColumns, expandedStates: result.expandedStates,
        computed, missing: width * height - computed, conflictCount, stops: result.stops, agreeingArrivals: result.agreeingArrivals,
        authoredArrivals: result.authoredArrivals, authoredResets: result.authoredResets,
        unknownRoles, missingSources, missingFacings, contextDependent, validatedGeometry: false}};
}

if (typeof module !== 'undefined') module.exports = {heightDeltas, isDiagonal, crossingFacing, eastwardSteps, westwardSteps, traversalDelta, diagonalWalk, filterContributions, anchoredWalk, calculateHeights, SAND_HASHES, ARRIVAL_OFFSETS};
if (typeof self !== 'undefined' && typeof self.postMessage === 'function') {
    self.onmessage = event => {
        const {id, input} = event.data;
        try {
            const result = calculateHeights(input, status => self.postMessage({id, status}));
            self.postMessage({id, ...result}, [result.heights.buffer, result.assigned.buffer, result.conflicts.buffer,
                result.processed.buffer, result.stopReasons.buffer, result.firstArrival.buffer,
                result.acceptedArrivals.buffer, result.conflictArrivals.buffer]);
        } catch (error) { self.postMessage({id, error: error.message}); }
    };
}
