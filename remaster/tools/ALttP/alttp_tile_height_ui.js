/* Editor-only controls. All expensive traversal happens in height-worker.js. */
'use strict';
// Pure viewport math is also exercised in Node; bounds are end-exclusive.
function heightVisibleBounds(x, y, scale, viewportWidth, viewportHeight) {
    if (![x, y, scale, viewportWidth, viewportHeight].every(Number.isFinite) || scale <= 0) return null;
    const clamp = n => Math.max(0, Math.min(512, n));
    const x0 = clamp(Math.floor(-x / (scale * 8))), y0 = clamp(Math.floor(-y / (scale * 8)));
    const x1 = clamp(Math.ceil((viewportWidth - x) / (scale * 8)));
    const y1 = clamp(Math.ceil((viewportHeight - y) / (scale * 8)));
    return {x0, y0, width: Math.max(0, x1 - x0), height: Math.max(0, y1 - y0)};
}
function heightSameBounds(a, b) {
    return a === b || !!a && !!b && ['x0', 'y0', 'width', 'height'].every(k => a[k] === b[k]);
}
if (typeof module !== 'undefined') module.exports = {heightVisibleBounds, heightSameBounds};
// Missing is never mapped to grayscale zero. Two-tone cells stay distinct from
// both artwork and valid grayscale at any zoom; conflicts use orange.
function heightPreviewColor(value, assigned, conflict, min, max, x, y) {
    if (!assigned) return (x + y) % 2 ? [255, 0, 220, 255] : [70, 0, 90, 255];
    if (conflict) return [255, 145, 0, 255];
    const gray = Math.round((value - min) / (max - min || 1) * 255);
    return [gray, gray, gray, 255];
}
if (typeof module !== 'undefined') module.exports.heightPreviewColor = heightPreviewColor;
const HEIGHT_ARRIVAL_OFFSETS = [[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1]];
const HEIGHT_ARRIVAL_NAMES = ['N','NE','E','SE','S','SW','W','NW'];
function heightArrivalLabels(mask) {
    return HEIGHT_ARRIVAL_NAMES.filter((_, bit) => mask & (1 << bit)).join('/') || 'none';
}
// Distinct categories can share a neighbor: e.g. the same edge was evaluated
// under different logical headings. Keep those arrows separately colored.
function heightArrivalEdges(first, accepted, conflicts) {
    const edges = [];
    for (let bit = 0; bit < 8; bit++) {
        const mask = 1 << bit, [dx,dy] = HEIGHT_ARRIVAL_OFFSETS[bit];
        if (accepted & mask) edges.push({dx,dy,kind:first & mask ? 'first' : 'agree'});
        if (conflicts & mask) edges.push({dx,dy,kind:'conflict'});
    }
    return edges;
}
if (typeof module !== 'undefined') Object.assign(module.exports, {heightArrivalEdges, heightArrivalLabels});
if (typeof document !== 'undefined') (() => {
    const controls = document.createElement('div');
    controls.innerHTML = `<div style="display:flex;gap:8px;flex-wrap:wrap">
      <label><input id="worldHeight" type="checkbox"> Heightmap (raw)</label>
      <label>Opacity <input id="heightOpacity" type="range" min="0" max="1" step=".05" value=".85"></label>
       <label><input id="worldWeights" type="checkbox"> Wall weights (zoom ≥3×)</label>
       <label><input id="worldCalculatedHeights" type="checkbox"> Heights (zoom ≥3×)</label>
       <label><input id="worldHeightArrows" type="checkbox"> Height traversal arrows (zoom ≥2×)</label>
      <span>Scope: Visible area</span>
        <button id="heightRun" style="width:190px;flex-shrink:0">Recalculate heights</button><button id="heightRunPrevious" disabled>recalc heights (prev area)</button><button id="heightCancel" disabled>Cancel</button></div>
        <div id="heightActivity" role="status" aria-live="polite" style="height:1.4em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex-shrink:0;font-weight:bold">Idle — no calculation yet</div>
      <div style="display:flex;gap:8px;flex-wrap:wrap"><button id="heightAnchor">Use selected tile as reference</button>
       <label>Reference height <input id="heightReference" type="number" step="any" value="0" style="width:90px"></label></div>
       <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap"><span id="heightOffsetCell">Select a map cell</span>
        <label>Authored height offset <input id="heightOffset" type="number" min="-32767" max="32767" step="1" style="width:90px"></label>
        <label><input id="heightOffsetAll" type="checkbox"> Apply to all instances in this world</label>
         <button id="heightOffsetSave">Save offset</button><button id="heightOffsetRemove">Remove offset</button><button id="heightOffsetBake">Bake Current Offset</button>
         <label>Adjust by <input id="heightOffsetDelta" type="number" step="1" value="0" style="width:80px"></label><button id="heightOffsetAdjust">Apply adjustment</button>
       <span id="heightOffsetState" class="hint"></span></div>
       <div id="heightStatus" class="hint" role="status" style="height:1.4em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis">Not calculated.</div>
       <details id="heightHelp"><summary style="cursor:pointer">Help &amp; details</summary>
       <div style="height:180px;overflow:auto;padding:6px 0">
       <div id="heightAnchorInfo" class="hint"></div><div id="heightBounds" class="hint"></div>
        <div class="hint">Experimental diagonal-propagation-v37-1 · both worlds · explicit reference required in view.
        Authored BG2 placement offsets are absolute traversal heights: incoming heights reset to the authored value, then propagation continues. Only reached tiles reset; offsets do not launch independent seeds. A saved zero is an explicit reset. Remove offset restores automatic traversal.
         Saved offsets use native profile units (no scaling). Per-pixel terrain is preserved: uniform fields combine with the scalar; nonuniform fields stop as unresolved rather than being averaged. Saving an offset enables the native authored-overworld-offsets mode.
         Adjust by adds a signed integer to each selected location's own Heights label and saves the result as an authored offset. Authored heights take precedence over calculated values; stale/conflicting numeric values and display-default zero are accepted just like baking. Cells showing ? or unrepresentable results reject the whole adjustment. Independent terrain is preserved without double-counting.
      Outward directional wavefront; non-wall tiles launch perpendicular traversals toward unassigned neighbors.
       All walls and wall corners continue their crossing without launching perpendicular traversals; outward branching resumes on non-wall tiles.
       Travel along cardinal wall faces is straight-only: E/W on N/S-facing walls, N/S on E/W-facing walls. Branching resumes after leaving those faces.
       Climbing adds the destination wall's saved weight on entry; descending subtracts the source wall's saved weight on exit. This applies to cardinal and diagonal walls in all directions.
       Direct cardinal-wall ↔ diagonal-wall transitions stop before transferring height; floor entry/exit remains allowed.
      Magenta cross = reference; cyan dashed box = calculated bounds. Outside calculated bounds is unknown/transparent.
         Magenta/dark-purple checker = NO HEIGHT; orange = conflicting arrival (first height retained).
         Recalc heights (prev area) reuses the last successfully calculated bounds for this world with current saved inputs and reference, even after panning or zooming. Failed or cancelled calculations do not replace that area. The reference must be inside the calculation area.
         Heights and wall weights are mutually exclusive. Saved authored heights take priority and appear immediately in cyan, or magenta when the calculation did not reach that cell; unreached authored cells use authored-height grayscale shading instead of the checkerboard. Otherwise labels show calculated heights, or gray 0 where no calculation exists. Unreached cells without authored heights and nonuniform authored fields show ?. Conflicting calculated heights are orange; stale calculated labels fade. Display-only default zeros are not assigned traversal heights.
       Agreeing arrivals continue in new directions; repeated coordinate/direction states and conflicts stop.
        East branches to NE SW/NE-facing and SE NW/SE-facing walls, plus E except diagonal-to-diagonal (placement flips applied).
        West branches to NW SE/NW-facing and SW NE/SW-facing walls, plus W with the same lateral restriction.
       Boundaries and unresolved metadata stop paths. No automatic filling of missed tiles.
       No smoothing, reconciliation or runtime geometry. Weight 0 disables a wall contribution.</div>
       <div id="heightArrowLegend" class="hint">Traversal arrows: cyan = first arrival; white = agreeing arrival; red = conflicting arrival. Arrowheads point toward the reached tile. Recalculate to record paths.</div>
       <div id="heightStatusDetail" class="hint"></div><div id="heightStorage" class="hint" style="overflow-wrap:anywhere"></div>
       <div id="heightPlacement" class="hint"></div></div></details>`;
    worldPane.insertBefore(controls, document.getElementById('worldViewport'));
    const el = id => document.getElementById(id);
    // Dynamic text must not resize the viewport it describes: that would create
    // a ResizeObserver -> changed bounds -> stale text -> resize feedback loop.
    controls.style.flexShrink = '0';
    // The closed help panel takes only its summary row; open content has a
    // fixed scroll area so changing diagnostics cannot resize the map.
    let config = null, result = null, image = null, generation = 0, resultGeneration = -1;
    // View/layout changes stale overlay scope, not explicit saves of captured numbers.
    let editGeneration = 0;
    let offsetConfig = null, offsetSelection = '', offsetBusy = false;
    const offsetKey = () => worldCell ? `${worldName}:${worldCell.x}:${worldCell.y}` : '';
    const offsetLocations = () => typeof worldSelection !== 'undefined' && worldActive && worldSelection.size ?
        [...worldSelection.values()].map(({x,y})=>({x,y})) : worldCell ? [{...worldCell}] : [];
    const offsetSelectionKey = () => `${worldName}:` + offsetLocations().map(({x,y})=>`${x}:${y}`).sort().join(',');
    function showOffset(force = false) {
        const key = offsetKey(), locations = offsetLocations(), selectionKey = offsetSelectionKey();
        const available = !!locations.length && !!offsetConfig?.path && !offsetBusy;
        el('heightOffsetSave').disabled = !available;
        el('heightOffsetRemove').disabled = !available;
        el('heightOffsetBake').disabled = !available || running;
        el('heightOffsetAdjust').disabled = !available || running;
        el('heightOffsetDelta').disabled = !available || running;
        el('heightOffsetBake').title = 'Save the numbers shown by Heights for selected locations, including conflicting or stale results you accept. Cells showing ? cannot be baked.';
        el('heightOffset').disabled = !available;
        el('heightOffsetAll').disabled = !available;
        if (!force && selectionKey === offsetSelection) return;
        offsetSelection = selectionKey;
        el('heightOffsetCell').textContent = locations.length > 1 ? `${worldName} · ${locations.length} selected locations · BG2` : key ? `${worldName} cell (${worldCell.x}, ${worldCell.y}) · BG2` : 'Select a map cell';
        const values = locations.map(({x,y})=>offsetConfig?.offsets[`${worldName}:${x}:${y}`]);
        const mixed = values.some(value=>value!==values[0]);
        const value = mixed ? undefined : values[0];
        el('heightOffset').placeholder = mixed ? '–' : '';
        el('heightOffset').value = value === undefined ? '' : String(value);
        el('heightOffsetState').textContent = !offsetConfig?.path ? 'Requires a loaded profile' :
            locations.length > 1 ? `${mixed ? 'Mixed scalar offsets' : value === undefined ? 'No authored scalar offsets' : `Authored scalar offset ${value}`} · Save/remove applies to all ${locations.length} locations; per-pixel terrain preserved` :
            offsetConfig.nonuniform.includes(key) ? 'Nonuniform per-pixel heights: traversal stops; terrain preserved' :
            Object.hasOwn(offsetConfig.heights, key) ? `Authored traversal height ${offsetConfig.heights[key]}${offsetConfig.enabled ? '' : ' (native offset mode disabled)'}` : 'Automatic traversal (no authored height)';
    }
    async function readOffsets() {
        const response = await fetch('/api/height-offsets', {cache:'no-store'}), value = await response.json();
        if (!response.ok) throw Error(value.error);
        if (meta && value.revision !== meta.revision) throw Error('Profile/metadata changed elsewhere; reload before editing or calculating.');
        offsetConfig = value; refreshHeightImage(); showOffset(true); drawWorld();
        return value;
    }
    async function saveOffset(remove, bake = false, adjust = false) {
        if (!worldCell || !offsetConfig?.path || offsetBusy) return;
        let locations = offsetLocations();
        const world = worldName, key = offsetSelectionKey(), allInstances=el('heightOffsetAll').checked;
        const text = el('heightOffset').value.trim(), offset = remove ? null : Number(text);
        if (!bake && !remove && (!text || !Number.isInteger(offset) || Math.abs(offset) > 32767)) {
            el('heightOffsetState').textContent = 'Not saved: enter an integer from -32767 to 32767.'; return;
        }
        if(allInstances) {
            const aliases=new Set();
            for(const {x,y} of locations) {
                const id=worldManifest.content_ids[worldRecord(x,y).id],tile=worldTarget(id);
                if(!tile){el('heightOffsetState').textContent='Not saved: a selected location has no editable tile type.';return;}
                for(const alias of tile.aliases||[tile.content_id])aliases.add(alias);
            }
            locations=[];
            for(let y=0;y<512;y++)for(let x=0;x<512;x++)
                if(aliases.has(worldManifest.content_ids[worldRecord(x,y).id]))locations.push({x,y});
        }
        let values;
        let delta=0;
        if(bake) {
            try {
                if(running)throw Error('Wait for the calculation to finish before baking or adjusting.');
                if(adjust) {
                    const text=el('heightOffsetDelta').value.trim();
                    delta=Number(text);
                    if(!text||!Number.isSafeInteger(delta))throw Error('Enter a signed integer adjustment.');
                }
                values=locations.map(({x,y})=>{
                    const height=displayedHeight(x,y).value,at=`${world}:${x}:${y}`;
                    if(!Number.isFinite(height))throw Error(`Cell (${x}, ${y}) shows ?; there is no numeric height to ${adjust?'adjust':'bake'}.`);
                    // Preserve independent uniform terrain without double-adding it.
                    const terrain=offsetConfig.terrain.includes(at)?offsetConfig.heights[at]-(offsetConfig.offsets[at]??0):0;
                    const scalar=height+delta-terrain;
                    if(!Number.isInteger(scalar)||Math.abs(scalar)>32767)throw Error(`Cell (${x}, ${y}) cannot be saved as an integer offset in -32767…32767.`);
                    return {x,y,offset:scalar};
                });
            } catch(error){el('heightOffsetState').textContent=(adjust?'Not adjusted: ':'Not baked: ')+error.message;return;}
        }
        if(allInstances) {
            const action=adjust?`Adjust each displayed height by ${delta>=0?'+':''}${delta}`:bake?'Bake each displayed height':remove?'Remove authored scalar offsets':`Set authored scalar offset to ${offset}`;
            if(!window.confirm(`${action} for ALL ${locations.length} matching tile locations in the ${world==='light'?'Light':'Dark'} World?\n\nThis includes matching artwork aliases outside your selection and visible area, even where the artwork is used in a different context. Existing scalar offsets will be replaced${remove?' or removed':''}. The other world and per-pixel terrain are unchanged.\n\nConfirm to save, or Cancel to leave the profile unchanged.`)) {
                el('heightOffsetState').textContent='Cancelled — no offsets saved.';return;
            }
        }
        const bakeRevision=meta.revision, bakeWeights=config?.revision;
        offsetBusy = true; invalidate(); showOffset();
        const bakeGeneration=editGeneration;
        el('heightOffsetState').textContent = 'Saving placement offset…';
        const job = async () => {
            try {
                await flushSemantics();
                if(bake) {
                    const response=await fetch('/api/weights',{cache:'no-store'}),current=await response.json();
                    const operation=adjust?'adjustment':'bake';
                    if(!response.ok)throw Error(current.error||`Cannot verify saved inputs for the ${operation}.`);
                    if(current.revision!==bakeWeights||meta.revision!==bakeRevision)
                        throw Error(`Saved profile, metadata or weights changed while preparing the ${operation}; review the displayed heights and try again.`);
                    if(editGeneration!==bakeGeneration||worldName!==world)
                        throw Error(`Editing inputs changed while preparing the ${operation}; review the displayed heights and try again.`);
                }
                saving = true;
                const response = await fetch('/api/height-offsets', {method:'PUT',
                    headers:{'Content-Type':'application/json','If-Match':meta.revision},
                    body:JSON.stringify(bake?{world,values}:{world,locations,offset})});
                const value = await response.json();
                if (!response.ok) throw Error(value.error);
                offsetConfig = value; meta.revision = value.revision;
                refreshHeightImage();
                await readConfig();
                drawWorld();
                if (key === offsetSelectionKey()) { showOffset(true); el('heightOffsetState').textContent += adjust ? ` · Adjusted ${locations.length} displayed heights by ${delta>=0?'+':''}${delta}` : bake ? ` · Baked ${locations.length} displayed heights` : remove ? locations.length > 1 ? ' · Offsets removed' : ' · Offset removed' : allInstances ? ` · Saved to profile for all ${locations.length} matching locations in ${world} world` : ' · Saved to profile'; }
            } catch (error) {
                el('heightOffsetState').textContent = 'Not saved: ' + error.message; throw error;
            } finally { saving = false; offsetBusy = false; showOffset(); }
        };
        saveQueue = saveQueue.catch(() => {}).then(job); saveQueue.catch(() => {});
        await saveQueue.catch(() => {});
    }
    el('heightOffsetSave').onclick = () => saveOffset(false);
    el('heightOffset').onkeydown = event => {
        if (event.key === 'Enter' && !event.isComposing) { event.preventDefault(); return saveOffset(false); }
    };
    el('heightOffsetRemove').onclick = () => saveOffset(true);
    el('heightOffsetBake').onclick = () => saveOffset(false, true);
    el('heightOffsetAdjust').onclick = () => saveOffset(false, true, true);
    el('heightOffsetDelta').onkeydown = event => {
        if(event.key==='Enter'&&!event.isComposing){event.preventDefault();return saveOffset(false,true,true);}
    };
    let running = false, worker = null, saveQueue = Promise.resolve(), semanticQueue = Promise.resolve();
    let runID = 0, runTimer = null, started = 0, phase = '';
    const workspace=window.tileSemanticWorkspace;
    const previousAreas = Object.fromEntries(['light','dark'].map(name=>[name,workspace?.getWorld(name)?.previousArea||null]));
    function showPreviousArea() {
        el('heightRunPrevious').disabled = running || !previousAreas[worldName];
    }
    showPreviousArea();
    function setPhase(text) { phase = text; el('heightActivity').textContent = `${text} · ${((Date.now() - started) / 1000).toFixed(1)}s`; }
    function finishRun(text) {
        if (runTimer !== null) clearInterval(runTimer);
        runTimer = null; running = false;
        showOffset();
        el('heightRun').disabled = false; el('heightRun').textContent = 'Recalculate heights';
        showPreviousArea();
        el('heightCancel').disabled = true;
        setPhase(text);
    }
    el('heightCancel').onclick = () => {
        runID++; worker?.terminate(); worker = null;
        finishRun('Cancelled'); status('Calculation cancelled. Previous overlay, if any, is unchanged.');
    };
    let semanticError = null;
    const batchPending = new Set();
    let batchFlush = null;
    const multiSelection = () => typeof worldSelection !== 'undefined' && worldActive && worldSelection.size > 1;
    const selectionTiles = () => multiSelection() ? worldSelectedTiles() : [byId.get(selected)].filter(Boolean);
    function renderBatchPanel() {
        const targets = selectionTiles(), annotations = targets.map(t => t.annotation || {role:'unknown',normals:[]});
        detail.replaceChildren();
        const heading=document.createElement('label');heading.className='cap';heading.textContent='Selected locations';detail.append(heading);
        const hint=document.createElement('div');hint.className='hint';
        hint.textContent=`${worldSelection.size} locations · ${targets.length} tile types. Role/weight edits affect all copies of these tile types. Authored offsets above apply to all selected locations only.`;detail.append(hint);
        const commonRole=annotations.every(a=>a.role===annotations[0]?.role)?annotations[0]?.role:null;
        const cap=document.createElement('label');cap.className='cap';cap.textContent='Tile role';detail.append(cap);
        const buttons=document.createElement('div');buttons.className='roleButtons';
        for(const role of roles){const b=document.createElement('button');b.textContent=labels[role];b.className=role===commonRole?'on':'';b.onclick=()=>batchEdit('role',role);buttons.append(b);}detail.append(buttons);
        if(['wall','wall_corner','door','doorway'].includes(commonRole)) {
            const normals=annotations.every(a=>JSON.stringify(a.normals)===JSON.stringify(annotations[0].normals))?annotations[0].normals:[];
            const corner=annotations.every(a=>a.corner_type===annotations[0].corner_type)?annotations[0].corner_type:'unknown';
            renderNormals({role:commonRole,normals});
            if(commonRole==='wall_corner')renderCornerType({role:commonRole,normals,corner_type:corner});
        }
        const state=document.createElement('div');state.id='saveState';state.className='saveState';
        state.textContent=batchPending.size?'Choose required facing/corner metadata to save all selected tiles.':'';detail.append(state);
    }
    function batchEdit(field,value) {
        const targets=selectionTiles();
        for(const tile of targets) {
            const a={...(tile.annotation||{role:'unknown',normals:[],note:'',context_dependent:false})};
            if(field==='role') {
                if(a.role!==value){a.role=value;a.normals=[];delete a.corner_type;
                    if(value==='wall_corner')a.corner_type='unknown';
                    if(value!=='floor')delete a.water;
                    if(value!=='doorway'){delete a.doorframe_mask;delete a.doorway_support;delete a.doorway_support_direction;}}
            } else if(field==='normal') {
                const count=a.role==='wall_corner'?2:1;
                a.normals=a.normals.includes(value)?a.normals.filter(n=>n!==value):[...a.normals,value].slice(-count);
            } else a.corner_type=value;
            tile.annotation=a;batchPending.add(tile.content_id);
        }
        invalidate();renderDetail();
        if([...batchPending].every(id=>complete(byId.get(id).annotation))) {
            flushBatch().then(()=>{if(multiSelection())renderDetail();}).catch(error=>{el('saveState').textContent=error.message;});
        }
    }
    async function flushBatch() {
        if(batchFlush)return batchFlush;
        if([...batchPending].some(id=>!complete(byId.get(id).annotation)))throw Error('Complete facing/corner metadata for all selected tiles first.');
        batchFlush=(async()=>{
            while(batchPending.size){
                const id=batchPending.values().next().value,snapshot=byId.get(id).annotation;
                if(!complete(snapshot))throw Error('Complete facing/corner metadata for all selected tiles first.');
                await save(id);
                if(byId.get(id).annotation===snapshot || JSON.stringify(byId.get(id).annotation)===JSON.stringify(snapshot))batchPending.delete(id);
            }
        })();
        try{await batchFlush;}finally{batchFlush=null;}
    }
    const references = Object.fromEntries(['light','dark'].map(name=>[name,workspace?.getWorld(name)?.reference||null]));
    const referenceValues = Object.fromEntries(['light','dark'].map(name=>[name,workspace?.getWorld(name)?.referenceHeight||'0']));
    let viewBounds = null, viewWorld = null;
    const status = text => {
        el('heightStatus').textContent = text; el('heightStatus').title = text;
        el('heightStatusDetail').textContent = text;
    };
    const weight = tile => config?.weights[tile?.content_id] ?? 8;
    function visibleBounds() {
        const r = vp.getBoundingClientRect();
        return heightVisibleBounds(worldX, worldY, worldScale, Math.max(1, Math.round(r.width)), Math.max(1, Math.round(r.height)));
    }
    const describeBounds = b => b ? `cells x[${b.x0}, ${b.x0 + b.width}), y[${b.y0}, ${b.y0 + b.height}) · ${b.width}×${b.height}` : 'no visible bounds';
    function showReference() {
        const ref = references[worldName];
        let warning = '';
        if (ref && worldGrid && worldManifest) {
            const cell = worldRecord(ref.x, ref.y), tile = worldTarget(worldManifest.content_ids[cell.id]);
            if (tile?.annotation?.role !== 'floor') warning = ' Warning: not classified as floor (may be wall/mixed/unresolved); reference is still allowed.';
        }
        el('heightAnchorInfo').textContent = ref ? `${worldName} reference: cell (${ref.x}, ${ref.y}), pixel (${ref.x * 8}, ${ref.y * 8}), height ${referenceValues[worldName]}.${warning}` : `${worldName}: no reference. Select a visible tile, then use it as reference (height defaults to 0).`;
    }
    function viewChanged() {
        showPreviousArea();
        const next = visibleBounds();
        if (worldName === viewWorld && heightSameBounds(next, viewBounds)) return;
        if (worldName !== viewWorld) el('heightReference').value = referenceValues[worldName];
        viewWorld = worldName; viewBounds = next;
        generation++;
        el('heightBounds').textContent = `Visible ${worldName}: ${describeBounds(next)} (end-exclusive).${result ? ` Last calculated ${result.world}: ${describeBounds(result.bounds)}.` : ''}`;
        showReference();
        if (result || running) status('STALE — view/world changed; result covers previous bounds. Recalculate for this view.');
    }
    function invalidate() {
        generation++;
        editGeneration++;
        if (result || running) status('STALE — semantics/weights/reference/world changed; recalculate to update raw heights.');
        showReference();
        drawWorld();
    }
    el('heightAnchor').onclick = () => {
        if (!worldCell || !worldGrid || el('worldChoice').value !== worldName) {
            status('Select a visible tile on the loaded world map, then use it as reference.'); return;
        }
        references[worldName] = {...worldCell, world: worldName};
        workspace?.updateWorld(worldName,{reference:{...references[worldName]}});
        invalidate(); showReference();
    };
    el('heightReference').oninput = () => {
        referenceValues[worldName] = el('heightReference').value;
        workspace?.updateWorld(worldName,{referenceHeight:referenceValues[worldName]});
        invalidate();
    };
    async function readConfig() {
        const response = await fetch('/api/weights', {cache: 'no-store'});
        const value = await response.json();
        if (!response.ok) throw Error(value.error);
        if (config && config.revision !== value.revision) invalidate();
        config = value;
        el('heightStorage').textContent = `Experimental weights save separately to: ${value.path} (not runtime profile geometry)`;
        return value;
    }
    // Serialize semantic writes and retain newer local edits if a response arrives late.
    save = id => {
        const job = async () => {
            if (!id) return;
            const tile = byId.get(id), sent = tile.annotation;
            if (!sent || !complete(sent)) { semanticError = 'Complete the pending semantic annotation first.'; return; }
            saving = true;
            try {
                const response = await fetch('/api/annotation/' + encodeURIComponent(id), {
                    method: 'PUT', headers: {'Content-Type': 'application/json', 'If-Match': meta.revision},
                    body: JSON.stringify(sent)
                });
                const value = await response.json();
                if (!response.ok) throw Error(value.error);
                meta.revision = value.revision;
                const newer = tile.annotation !== sent;
                for (const alias of value.content_ids) {
                    if (byId.has(alias) && !(alias === id && newer)) byId.get(alias).annotation = value.annotation;
                }
                if (pendingId === id && !newer) pendingId = null;
                semanticError = null;
                const state = id === selected ? el('saveState') : null;
                if (state) state.textContent = newer ? 'Unsaved newer edits…' : `Saved to ${value.content_ids.length} runtime identities`;
                renderGrid(); drawWorld();
            } catch (error) {
                semanticError = 'Semantic edits not saved: ' + error.message;
                const state = id === selected ? el('saveState') : null;
                if (state) state.textContent = 'Not saved: ' + error.message;
                throw error;
            } finally { saving = false; }
        };
        semanticQueue = semanticQueue.then(job, job);
        semanticQueue.catch(() => {});
        return semanticQueue;
    };
    async function flushSemantics() {
        clearTimeout(saveTimer);
        await semanticQueue.catch(() => {});
        if (pendingId) await save(pendingId);
        await flushBatch();
        if (semanticError || pendingId || selected && !complete(annotation())) {
            throw Error(semanticError || 'Complete and save the pending semantic annotation first.');
        }
    }
    function renderWeightPanel() {
        if (!selected || !detailData) return;
        el('experimentalWeightPanel')?.remove();
        const targets = selectionTiles(), tile = targets[0], box = document.createElement('div');
        box.id = 'experimentalWeightPanel'; box.className = 'normalBox';
        const cap = document.createElement('label'); cap.className = 'cap';
        cap.textContent = 'Experimental wall height weight'; box.append(cap);
        const input = document.createElement('input');
        input.type = 'number'; input.min = '0'; input.max = '32'; input.step = '1';
        const mixed=targets.some(t=>weight(t)!==weight(tile));
        input.value = mixed?'':weight(tile);input.placeholder=mixed?'–':''; input.style.width = '90px';
        input.disabled = !config || !targets.length || !multiSelection() && targets.some(t=>!['wall', 'wall_corner', 'door', 'doorway'].includes(t.annotation?.role));
        const message = document.createElement('div'); message.className = 'hint';
        message.textContent = 'Integer weight (0–32), shared by aliases. Default 8; three confirmed half-slope families default 4.';
        const ids = targets.map(t=>t.content_id);
        function submit(value) {
            if (!Number.isInteger(value) || value < 0 || value > 32) {
                message.textContent = 'Not saved: enter an integer weight from 0 to 32.'; return;
            }
            invalidate();
            message.textContent = 'Saving experimental weight…';
            const job = async () => {
                try {
                    await flushSemantics();
                    // Semantic saves alter the weight revision. Refresh only when the
                    // previous binding/weight file remains unchanged, never adopt external edits.
                    const old = config;
                    const response = await fetch('/api/weights', {cache: 'no-store'});
                    const current = await response.json();
                    if (!response.ok) throw Error(current.error);
                    if (!old || JSON.stringify(old.binding) !== JSON.stringify(current.binding) ||
                        old.file_revision !== current.file_revision || current.semantic_revision !== meta.revision) {
                        throw Error('Weights/profile changed elsewhere; reload before saving.');
                    }
                    config = current;
                    for(const id of ids) {
                    const saved = await fetch('/api/weights/' + encodeURIComponent(id), {
                        method: 'PUT', headers: {'Content-Type': 'application/json', 'If-Match': config.revision},
                        body: JSON.stringify({weight: value})
                    });
                    const updated = await saved.json();
                    if (!saved.ok) throw Error(updated.error);
                    config = updated;
                    }
                    message.textContent = `Saved experimental weight ${value} to ${config.path}`;
                    input.value = value; drawWorld();
                } catch (error) { message.textContent = 'Not saved: ' + error.message; throw error; }
            };
            saveQueue = saveQueue.catch(() => {}).then(job);
            // Keep rejection visible to recalculate, without an unhandled rejection.
            saveQueue.catch(() => {});
        }
        input.oninput = invalidate;
        input.onchange = () => { if(input.value.trim())submit(Number(input.value)); };
        box.append(input);
        for (const [label, value] of [['Half / 4', 4], ['Full / reset to 8', 8]]) {
            const button = document.createElement('button'); button.textContent = label;
            button.disabled = input.disabled; button.onclick = () => submit(value); box.append(button);
        }
        box.append(message); detail.append(box);
    }
    const originalRender = renderDetail;
    renderDetail = function () { if(multiSelection())renderBatchPanel();else originalRender(); renderWeightPanel(); showReference(); drawWorld(); };
    const originalChangeRole=changeRole,originalToggleNormal=toggleNormal,originalUpdate=update;
    changeRole=role=>multiSelection()?batchEdit('role',role):originalChangeRole(role);
    toggleNormal=normal=>multiSelection()?batchEdit('normal',normal):originalToggleNormal(normal);
    update=a=>multiSelection()?batchEdit('corner_type',a.corner_type):originalUpdate(a);
    // These hooks cover incomplete role/normal changes too, not just saved edits.
    for (const name of ['changeRole', 'toggleNormal', 'update', 'scheduleSave']) {
        const original = {changeRole, toggleNormal, update, scheduleSave}[name];
        const wrapped = (...args) => { invalidate(); return original(...args); };
        if (name === 'changeRole') changeRole = wrapped;
        if (name === 'toggleNormal') toggleNormal = wrapped;
        if (name === 'update') update = wrapped;
        if (name === 'scheduleSave') scheduleSave = wrapped;
    }
    function showPlacement() {
        showOffset();
        if (!worldCell || !worldGrid || !worldManifest) { el('heightPlacement').textContent = ''; return; }
        const {x, y} = worldCell, cell = worldRecord(x, y);
        const tile = worldTarget(worldManifest.content_ids[cell.id]);
        let fx = 0, fy = 0;
        for (const n of tile?.annotation?.normals || []) for (const d of flippedFacing(n, cell.flips).split('_')) {
            fx += d === 'east' ? 1 : d === 'west' ? -1 : 0;
            fy += d === 'south' ? 1 : d === 'north' ? -1 : 0;
        }
        const wall = ['wall', 'wall_corner', 'door', 'doorway'].includes(tile?.annotation?.role), w = weight(tile);
        const n = wall ? (fy > 0 ? 1 : fy < 0 ? -1 : 0) * w : 0;
        const e = wall ? (fx > 0 ? -1 : fx < 0 ? 1 : 0) * w : 0;
        const s = result?.stats;
        const inBounds = result?.world === worldName && x >= s.x0 && x < s.x0 + s.width && y >= s.y0 && y < s.y0 + s.height;
        const i = inBounds ? (y - s.y0) * s.width + x - s.x0 : -1;
        const raw = inBounds ? (result.assigned[i] ? result.heights[i] : 'UNSET — no path assigned a height') : 'unknown / outside calculated bounds';
        const current = inBounds && resultGeneration === generation;
        const actual = current && result.conflicts[i] ? ' · CONFLICT: another path disagreed; first arrival retained' : '';
        let diagnostics = '';
        if (inBounds) {
            const directions = [[1,'N'],[2,'S'],[4,'E'],[8,'W']].filter(([bit]) => result.processed[i] & bit).map(([,name]) => name);
            const reasons = [[1,'view boundary'],[2,'direction already processed'],[4,'unresolved metadata'],[8,'conflicting height'],[16,'lateral diagonal-to-diagonal step blocked; no matching diagonal continuation'],[32,'cardinal/diagonal wall boundary; outgoing step stopped before height transfer']].filter(([bit]) => result.stopReasons[i] & bit).map(([,name]) => name);
            diagnostics = ` · processed directions: ${directions.join('/') || 'none'} · stops here: ${reasons.join(', ') || 'none recorded (an unset tile may never have been reached)'}`;
            diagnostics += ` · arrivals FROM: first ${heightArrivalLabels(result.firstArrival[i])}; agreeing ${heightArrivalLabels(result.acceptedArrivals[i] & ~result.firstArrival[i])}; conflicting ${heightArrivalLabels(result.conflictArrivals[i])}`;
        }
        el('heightPlacement').textContent = `Selected placement: weight ${w} · cardinal-axis ΔN ${n} · ΔE ${e} (diagonal crossings use one signed weight) · raw height ${raw}${actual}${diagnostics}${resultGeneration !== generation ? ' (STALE/not calculated)' : ''}`;
    }
    function drawTraversal(c) {
        if (!el('worldHeightArrows').checked) return;
        const s = result?.world === worldName ? result.stats : null;
        const stale = resultGeneration !== generation;
        el('heightArrowLegend').textContent = `Traversal arrows: cyan = first arrival; white = agreeing arrival; red = conflicting arrival. Arrowheads point into the reached tile. ${!s ? 'Recalculate heights for this world first.' : worldScale < 2 ? 'Zoom to ≥2× to see arrows.' : stale ? 'STALE — paths belong to the previous calculation.' : 'Showing recorded arrivals (repeated arrivals from the same neighbor are merged).'}`;
        if (!s || worldScale < 2) return;
        const visible = visibleBounds();
        const x0 = Math.max(s.x0, visible.x0 - 1), y0 = Math.max(s.y0, visible.y0 - 1);
        const x1 = Math.min(s.x0 + s.width, visible.x0 + visible.width + 1);
        const y1 = Math.min(s.y0 + s.height, visible.y0 + visible.height + 1);
        const size = 8 * worldScale, colors = {first:'#00ffff', agree:'#ffffff', conflict:'#ff4040'};
        c.save(); c.globalAlpha = stale ? .45 : 1; c.setLineDash([]);
        // First black outlines, then colored arrows. Conflict layer is last.
        for (const outline of [true,false]) for (const kind of ['agree','first','conflict']) {
            c.beginPath(); c.strokeStyle = outline ? '#000000' : colors[kind]; c.lineWidth = outline ? 3.5 : 1.5;
            for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
                const i = (y - s.y0) * s.width + x - s.x0;
                for (const edge of heightArrivalEdges(result.firstArrival[i], result.acceptedArrivals[i], result.conflictArrivals[i])) {
                    if (edge.kind !== kind) continue;
                    const sourceX = x + edge.dx, sourceY = y + edge.dy;
                    if (sourceX < s.x0 || sourceX >= s.x0+s.width || sourceY < s.y0 || sourceY >= s.y0+s.height) continue;
                    const length = Math.hypot(edge.dx, edge.dy), ux = -edge.dx/length, uy = -edge.dy/length;
                    // Opposite-direction arrows run on opposite sides of their
                    // shared edge, so their heads remain distinguishable.
                    const shift = edge.kind === 'conflict' ? 2.3 : 1;
                    const ox = -uy*shift, oy = ux*shift;
                    const tx = worldX+(x+.5)*size+ox, ty = worldY+(y+.5)*size+oy;
                    const sx = worldX+(sourceX+.5)*size+ox, sy = worldY+(sourceY+.5)*size+oy;
                    const ax=sx+ux*size*.15, ay=sy+uy*size*.15;
                    const bx=tx-ux*size*.18, by=ty-uy*size*.18, head=Math.min(6,size*.23);
                    c.moveTo(ax,ay);c.lineTo(bx,by);
                    c.moveTo(bx-ux*head-uy*head*.55,by-uy*head+ux*head*.55);c.lineTo(bx,by);
                    c.lineTo(bx-ux*head+uy*head*.55,by-uy*head-ux*head*.55);
                }
            }
            c.stroke();
        }
        c.restore();
    }
    // One source of truth for label display and explicit user-approved baking.
    function authoredDisplayHeight(world,x,y,nonuniform) {
        const key=`${world}:${x}:${y}`;
        return nonuniform.has(key)?NaN:offsetConfig?.heights[key];
    }
    function makeHeightImage(data) {
        const s=data.stats,canvas=document.createElement('canvas');
        canvas.width=s.width;canvas.height=s.height;
        const context=canvas.getContext('2d'),pixels=context.createImageData(s.width,s.height);
        const nonuniform=new Set(offsetConfig?.nonuniform||[]);
        for(let i=0;i<data.heights.length;i++) {
            const x=i%s.width,y=Math.floor(i/s.width);
            const authored=authoredDisplayHeight(data.world,x+s.x0,y+s.y0,nonuniform);
             // Shade unreached authored heights on the same scale, without
             // claiming traversal coverage (their numeric label stays magenta).
             pixels.data.set(!data.assigned[i]&&Number.isFinite(authored)?
                 heightPreviewColor(authored,true,false,s.min,s.max,x,y):
                heightPreviewColor(data.heights[i],data.assigned[i],data.conflicts[i],s.min,s.max,x,y),i*4);
        }
        context.putImageData(pixels,0,0);
        return canvas;
    }
    function refreshHeightImage(){if(result)image=makeHeightImage(result);}
    function displayedHeight(x,y,nonuniform = new Set(offsetConfig?.nonuniform || [])) {
        const key=`${worldName}:${x}:${y}`,authored=offsetConfig?.heights[key];
        if(nonuniform.has(key))return {value:NaN,color:'#ff66ee',alpha:1};
        const s=result?.world===worldName?result.stats:null;
        const i=s&&x>=s.x0&&x<s.x0+s.width&&y>=s.y0&&y<s.y0+s.height?(y-s.y0)*s.width+x-s.x0:-1;
        if(Number.isFinite(authored))return {value:authored,color:i>=0&&!result.assigned[i]?'#ff66ee':'#66ffff',alpha:1};
        if(s&&x>=s.x0&&x<s.x0+s.width&&y>=s.y0&&y<s.y0+s.height){
            const i=(y-s.y0)*s.width+x-s.x0;
            return {value:result.assigned[i]?result.heights[i]:NaN,
                color:!result.assigned[i]?'#ff66ee':result.conflicts[i]?'#ff9100':'#fff',
                alpha:resultGeneration===generation?1:.45};
        }
        return {value:0,color:'#aaa',alpha:1};
    }
    function drawHeightLabels(c) {
        if (!el('worldCalculatedHeights').checked || worldScale < 3) return;
        const visible = visibleBounds(), size = 8 * worldScale;
        const nonuniform = new Set(offsetConfig?.nonuniform || []);
        c.save();
        c.font = `${Math.min(16, size * .4)}px sans-serif`;
        c.textAlign = 'center'; c.textBaseline = 'middle';
        c.lineWidth = 3; c.strokeStyle = '#000';
        for (let y = visible.y0; y < visible.y0 + visible.height; y++) for (let x = visible.x0; x < visible.x0 + visible.width; x++) {
            const label=displayedHeight(x,y,nonuniform),text=Number.isFinite(label.value)?String(label.value):'?';
            c.globalAlpha=label.alpha;c.fillStyle=label.color;
            const px = worldX + (x + .5) * size, py = worldY + (y + .5) * size;
            c.strokeText(text, px, py, size - 2); c.fillText(text, px, py, size - 2);
        }
        c.restore();
    }
    function drawHeight(c) {
        const s = result?.world === worldName ? result.stats : null;
        if (image && s && el('worldHeight').checked) {
            c.globalAlpha = Number(el('heightOpacity').value);
            c.drawImage(image, worldX + s.x0 * 8 * worldScale, worldY + s.y0 * 8 * worldScale,
                s.width * 8 * worldScale, s.height * 8 * worldScale);
            c.globalAlpha = 1;
        }
        if (s) {
            c.strokeStyle = '#00ffff'; c.lineWidth = 2; c.setLineDash([6, 4]);
            c.strokeRect(worldX + s.x0 * 8 * worldScale, worldY + s.y0 * 8 * worldScale, s.width * 8 * worldScale, s.height * 8 * worldScale);
            c.setLineDash([]);
        }
        const ref = references[worldName];
        if (ref) {
            const size = 8 * worldScale, x = worldX + (ref.x + .5) * size, y = worldY + (ref.y + .5) * size;
            c.strokeStyle = '#ff66ff'; c.lineWidth = 3;
            c.strokeRect(x - size / 2, y - size / 2, size, size);
            c.beginPath(); c.moveTo(x - 9, y); c.lineTo(x + 9, y); c.moveTo(x, y - 9); c.lineTo(x, y + 9); c.stroke();
        }
    }
    async function recalculate(usePreviousArea = false) {
        if (running) return;
        const previousArea = usePreviousArea ? previousAreas[worldName] : null;
        if (usePreviousArea && !previousArea) return;
        const requestID = ++runID;
        running = true; el('heightRun').disabled = true;
        showPreviousArea();
        started = Date.now(); el('heightCancel').disabled = false;
        el('heightRun').textContent = 'Calculating…';
        setPhase('Preparing — waiting for saved edits');
        status('Preparing calculation; waiting for pending saves…');
        runTimer = setInterval(() => { if (requestID === runID && running) setPhase(phase); }, 250);
        try {
            if (!worldActive || !worldGrid || el('worldChoice').value !== worldName) throw Error('Wait for the selected world map to load.');
            await flushSemantics(); await saveQueue;
            await readConfig();
            const offsets = await readOffsets();
            const authoredHeights = Object.entries(offsets.heights).filter(([key]) => key.startsWith(worldName + ':'))
                .map(([key,height]) => { const [,x,y] = key.split(':'); return {x:Number(x),y:Number(y),height}; });
            const nonuniformHeights = offsets.nonuniform.filter(key => key.startsWith(worldName + ':'))
                .map(key => { const [,x,y] = key.split(':'); return {x:Number(x),y:Number(y)}; });
            if (requestID !== runID) return;
            if (pendingId || saving || selected && !complete(annotation())) throw Error('Semantic edits changed while preparing the snapshot. Finish saving, then recalculate.');
            if (config.semantic_revision !== meta.revision) throw Error('Semantics/profile changed externally; reload the page.');
            if (!worldActive || el('worldChoice').value !== worldName) throw Error('Wait for the selected world map to load.');
            viewChanged();
            const token = generation, name = worldName;
            const snapshotView = visibleBounds();
            const bounds = previousArea ? {...previousArea} : snapshotView, ref = references[name];
            referenceValues[name] = el('heightReference').value;
            const reference = ref && {...ref, height: referenceValues[name].trim() ? Number(referenceValues[name]) : NaN};
            if (!bounds || !bounds.width || !bounds.height) throw Error('No world cells in view. Pan to the world map.');
            if (!reference || !Number.isFinite(reference.height)) throw Error('Select a visible tile, use it as reference, and enter a finite reference height.');
            if (ref.x < bounds.x0 || ref.x >= bounds.x0 + bounds.width || ref.y < bounds.y0 || ref.y >= bounds.y0 + bounds.height) throw Error(previousArea ? 'Reference is outside the previous calculation area. Choose a reference inside that area.' : 'Reference is outside visible bounds. Select a visible tile and use it as reference.');
            const revision = config.revision;
            const tiles = [], ids = new Map();
            const buffer = new ArrayBuffer(bounds.width * bounds.height * 8), compact = new DataView(buffer);
            for (let y = 0; y < bounds.height; y++) for (let x = 0; x < bounds.width; x++) {
                const cell = worldRecord(x + bounds.x0, y + bounds.y0), key = worldManifest.content_ids[cell.id];
                if (!ids.has(key)) {
                    ids.set(key, tiles.length);
                    const t = worldTarget(key);
                    tiles.push(t ? {annotation: t.annotation, weight: weight(t)} : null);
                }
                const offset = (y * bounds.width + x) * 8;
                compact.setUint32(offset, ids.get(key), true); compact.setUint8(offset + 7, cell.flips);
            }
            setPhase('Starting height worker');
            worker = new Worker('/height-worker.js?v=diagonal-propagation-v37-1-trace1');
            worker.onmessage = async event => {
                if (requestID !== runID) return;
                try {
                const data = event.data;
                if (data.status) { setPhase(data.status); if (token === generation) status(data.status); return; }
                worker.terminate(); worker = null;
                if (data.error) throw Error(data.error);
                setPhase('Worker finished — verifying saved inputs');
                if (data.stats?.version !== 'diagonal-propagation-v37-1' || !data.assigned || !data.conflicts ||
                    data.traceFormat !== 'incoming-neighbors-v1' || data.firstArrival?.length !== data.heights?.length ||
                    data.acceptedArrivals?.length !== data.heights?.length || data.conflictArrivals?.length !== data.heights?.length ||
                    data.processed?.length !== data.heights?.length || data.stopReasons?.length !== data.heights?.length ||
                    data.heights?.length !== bounds.width * bounds.height || data.assigned.length !== data.heights.length || data.conflicts.length !== data.heights.length) {
                    throw Error('Editor/worker version mismatch or incomplete result. Reload the page to load matching scripts.');
                }
                try {
                    const check = await fetch('/api/weights', {cache: 'no-store'});
                    const current = await check.json();
                    if (!check.ok || current.revision !== revision) {
                        invalidate(); status('STALE — weights/profile/semantics changed during calculation. Reload/recalculate.'); return;
                    }
                } catch (error) { status('Cannot verify result snapshot: ' + error.message); return; }
                if (requestID !== runID) return;
                const currentBounds = visibleBounds();
                if (token !== generation || name !== worldName || name !== el('worldChoice').value || !heightSameBounds(snapshotView, currentBounds)) {
                    const reason = name !== worldName || name !== el('worldChoice').value ? 'world changed' :
                        !heightSameBounds(snapshotView, currentBounds) ? `visible bounds changed from ${describeBounds(snapshotView)} to ${describeBounds(currentBounds)}` :
                        'edits, reference, or view changed during calculation';
                    status(`STALE — calculation discarded: ${reason}. Recalculate.`); return;
                }
                setPhase('Rendering heightmap');
                const nextImage = makeHeightImage(data), s = data.stats;
                result = data; resultGeneration = token; image = nextImage;
                el('worldHeight').checked = true;
                status(`${name} calculated ${describeBounds(bounds)} · reference (${ref.x}, ${ref.y}) = ${reference.height} · raw grayscale black ${s.min} → white ${s.max} · ${s.computed} assigned, ${s.missing} NO HEIGHT (magenta checker), ${s.conflictCount} conflicts (orange), ${s.authoredResets} authored arrival resets. NOT validated geometry. Warnings: ${s.unknownRoles} unknown roles, ${s.missingSources} missing sources, ${s.missingFacings} missing wall facings, ${s.contextDependent} context-dependent placements.`);
                drawWorld();
                previousAreas[name] = {...bounds};
                workspace?.updateWorld(name,{previousArea:{...bounds},reference:{...ref},referenceHeight:referenceValues[name]});
                finishRun('Complete — heightmap shown');
                } catch (error) {
                    status('Calculation/display failed: ' + error.message);
                    finishRun('Failed — see details below');
                } finally {
                    if (requestID === runID && running && !worker) finishRun('Result not displayed — see details below');
                }
            };
            worker.onerror = event => {
                if (requestID !== runID) return;
                status('Worker failed: ' + event.message); worker.terminate(); worker = null;
                finishRun('Worker failed — see details below');
            };
            worker.onmessageerror = () => {
                if (requestID !== runID) return;
                worker.terminate(); worker = null;
                status('Worker result could not be decoded. Reload the page and recalculate.');
                finishRun('Worker communication failed');
            };
            status('Calculating raw heights in worker…');
            worker.postMessage({id: token, input: {world: name, bounds, mode: 'manual', reference, grid: buffer, tiles, authoredHeights, nonuniformHeights}}, [buffer]);
        } catch (error) {
            if (requestID !== runID) return;
            worker?.terminate(); worker = null;
            status(error.message); finishRun('Not calculated — see details below');
        }
    }
    el('heightRun').onclick = () => recalculate();
    el('heightRunPrevious').onclick = () => recalculate(true);
    for (const id of ['worldHeight', 'worldWeights', 'worldHeightArrows', 'heightOpacity']) el(id).oninput = drawWorld;
    for (const [id, other] of [['worldWeights', 'worldCalculatedHeights'], ['worldCalculatedHeights', 'worldWeights']]) {
        el(id).oninput = () => { if (el(id).checked) el(other).checked = false; drawWorld(); };
    }
    el('worldChoice').addEventListener('change', () => {
        invalidate();
    });
    window.experimentalHeightUI = {drawHeight, drawHeightLabels, drawTraversal, showPlacement, weight, viewChanged,
        selectionChanging:async()=>{
            clearTimeout(saveTimer);await semanticQueue;await flushBatch();
            if(pendingId)await save(pendingId);
            if(pendingId||semanticError)throw Error(semanticError||'Complete pending metadata before changing selection.');
            await saveQueue;
        },
        selectionChanged:()=>{offsetSelection='';}};
    viewChanged();
    readConfig().then(renderWeightPanel).catch(error => status(error.message));
    readOffsets().catch(error => status(error.message));
    // External weight-only edits do not change the semantic revision polled by
    // the original editor. Mark stale but never overwrite local unsaved controls.
    setInterval(async () => {
        if (!config || running || saving || pendingId) return;
        try {
            const response = await fetch('/api/weights', {cache: 'no-store'});
            const value = await response.json();
            if (!response.ok || value.revision !== config.revision) {
                if (resultGeneration === generation) invalidate();
                status('STALE — weights/profile/semantics changed elsewhere. Reload before editing; recalculate to use current data.');
            }
        } catch (error) { /* A transient connection failure must not erase edits. */ }
    }, 2500);
})();
