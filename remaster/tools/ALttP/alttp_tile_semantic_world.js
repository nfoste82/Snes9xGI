// Offline world context; shares the existing catalog and semantic authoring panel.
// Browser-local workspace only: never mixed into authored profile/metadata.
const worldWorkspaceKey='alttp-tile-semantic-workspace-v1';
function readWorldWorkspace() {
    let raw;
    try{raw=JSON.parse(window.localStorage?.getItem(worldWorkspaceKey)||'null');}catch{}
    const state={tab:raw?.tab==='world'?'world':'grid',world:raw?.world==='dark'?'dark':'light',worlds:{}};
    for(const name of ['light','dark']) {
        const saved=raw?.worlds?.[name],at={};
        if(saved?.view&&[saved.view.x,saved.view.y,saved.view.scale].every(Number.isFinite)&&saved.view.scale>=.03&&saved.view.scale<=16)
            at.view={x:saved.view.x,y:saved.view.y,scale:saved.view.scale};
        if(saved?.reference&&[saved.reference.x,saved.reference.y].every(v=>Number.isInteger(v)&&v>=0&&v<512))
            at.reference={x:saved.reference.x,y:saved.reference.y,world:name};
        at.referenceHeight=typeof saved?.referenceHeight==='string'&&saved.referenceHeight.trim()&&Number.isFinite(Number(saved.referenceHeight))?saved.referenceHeight:'0';
        const b=saved?.previousArea;
        if(b&&[b.x0,b.y0,b.width,b.height].every(Number.isInteger)&&b.x0>=0&&b.y0>=0&&b.width>0&&b.height>0&&b.x0+b.width<=512&&b.y0+b.height<=512)
            at.previousArea={x0:b.x0,y0:b.y0,width:b.width,height:b.height};
        state.worlds[name]=at;
    }
    return state;
}
const worldWorkspace=readWorldWorkspace();
function saveWorldWorkspace(){try{window.localStorage?.setItem(worldWorkspaceKey,JSON.stringify(worldWorkspace));}catch{}}
window.tileSemanticWorkspace={
    getWorld:name=>worldWorkspace.worlds[name],
    updateWorld:(name,patch)=>{
        const state=worldWorkspace.worlds[name];
        if(Object.entries(patch).every(([key,value])=>JSON.stringify(state[key])===JSON.stringify(value)))return;
        Object.assign(state,patch);saveWorldWorkspace();
    }
};
const worldPane = document.createElement('section');
worldPane.style.cssText='display:none;height:100%;min-height:0;flex-direction:column';
worldPane.innerHTML=`<div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px"><select id="worldChoice" style="width:auto"><option value="light">Light World</option><option value="dark">Dark World</option></select><button id="worldFit">Fit world</button><label><input type="checkbox" id="worldFacing"> Wall facing overlay</label><span id="worldZoom"></span></div><div class="hint">Wheel: zoom around cursor · Drag: pan · Click: author canonical tile (all identical placements share metadata).</div><div id="worldLegend" class="hint"></div><div id="worldInfo" class="hint" style="overflow-wrap:anywhere;min-height:65px"></div><div id="worldViewport" style="flex:1;min-height:240px;position:relative;overflow:hidden;touch-action:none;background:#080a08"><canvas id="worldCanvas" style="width:100%;height:100%;image-rendering:pixelated"></canvas></div><div id="worldLimit" class="hint"></div>`;
const mainPane=document.querySelector('.gridPane');mainPane.append(worldPane);
const switcher=document.createElement('button');switcher.textContent='World map';document.querySelector('header').append(switcher);
const wc=document.getElementById('worldCanvas'),vp=document.getElementById('worldViewport'),wi=document.getElementById('worldInfo');
const worldBoxMode=document.createElement('select');
worldBoxMode.id='worldBoxMode';worldBoxMode.style.width='auto';
worldBoxMode.innerHTML='<option value="pan">Drag: Pan map</option><option value="keep">Drag: Keep selected in box</option><option value="remove">Drag: Remove selected in box</option>';
worldBoxMode.value='pan';
document.getElementById('worldChoice').parentNode.append(worldBoxMode);
let worldManifest=null,worldGrid=null,worldImage=null,worldName='light',worldScale=1,worldX=0,worldY=0,worldCell=null,worldActive=false,worldLoading=0;
worldName=worldWorkspace.world;
document.getElementById('worldChoice').value=worldName;
const worldSelection = new Map();
let worldLocationMode = false;
function worldSelectedTiles() {
    const targets = new Map();
    for (const {x,y} of worldSelection.values()) {
        const tile = worldTarget(worldManifest.content_ids[worldRecord(x,y).id]);
        if (tile) targets.set(tile.content_id, tile);
    }
    return [...targets.values()];
}
const facingColors={south:'red',north:'#4488ff',east:'yellow',west:'#35d050',south_east:'orange',south_west:'#ff80b0',north_east:'cyan',north_west:'#b060ff'};
document.getElementById('worldLegend').textContent='S red · N blue · E yellow · W green · SE orange · SW pink · NE cyan · NW purple (placement flips applied)';
vp.title='Drag mode: pan, keep selected in box, or remove selected in box · Option: no pan · Cmd+Option: toggle tiles along path';
worldBoxMode.title='Click a tile type first. Box filtering converts its highlighted copies in this world into exact selected locations; it never adds other tile types.';
function worldRecord(x,y){let o=(y*512+x)*8;return {id:worldGrid.getUint32(o,true),map16:worldGrid.getUint16(o+4,true),area:worldGrid.getUint8(o+6),flips:worldGrid.getUint8(o+7)}}
function worldTarget(key){let t=byId.get(key);if(!t)return null;return t.aliases.map(a=>byId.get(a)).find(a=>a&&!a.low_variant)||t}
function flippedFacing(normal,flips){return normal.split('_').map(n=>flips&1?(n==='east'?'west':n==='west'?'east':n):n).map(n=>flips&2?(n==='north'?'south':n==='south'?'north':n):n).join('_')}
function drawWorld() {
    if (!worldActive || !worldGrid || !worldImage) return;
    window.tileSemanticWorkspace.updateWorld(worldName,{view:{x:worldX,y:worldY,scale:worldScale}});
    const r = vp.getBoundingClientRect();
    wc.width = Math.max(1, Math.round(r.width)); wc.height = Math.max(1, Math.round(r.height));
    window.experimentalHeightUI?.viewChanged();
    const c = wc.getContext('2d');
    c.imageSmoothingEnabled = false;
    c.drawImage(worldImage, worldX, worldY, 4096 * worldScale, 4096 * worldScale);
    window.experimentalHeightUI?.drawHeight(c);
    const overlay = document.getElementById('worldFacing').checked;
    const x0 = Math.max(0, Math.floor(-worldX / worldScale / 8));
    const y0 = Math.max(0, Math.floor(-worldY / worldScale / 8));
    const x1 = Math.min(512, Math.ceil((wc.width - worldX) / worldScale / 8));
    const y1 = Math.min(512, Math.ceil((wc.height - worldY) / worldScale / 8));
    const group = new Set(worldLocationMode ? [] : byId.get(selected)?.aliases || []);
    const weights = document.getElementById('worldWeights')?.checked;
    if (overlay || group.size || weights) for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
        const cell = worldRecord(x, y), key = worldManifest.content_ids[cell.id];
        const tile = worldTarget(key), a = tile?.annotation;
        const px = worldX + x * 8 * worldScale, py = worldY + y * 8 * worldScale, s = 8 * worldScale;
        if (overlay && ['wall', 'wall_corner', 'door', 'doorway'].includes(a?.role)) {
            const ns = a.normals || [];
            for (let i = 0; i < ns.length; i++) {
                c.globalAlpha = .5; c.fillStyle = facingColors[flippedFacing(ns[i], cell.flips)];
                c.fillRect(px + i * s / ns.length, py, s / ns.length, s);
            }
            c.globalAlpha = 1;
        }
        if (weights && s >= 24 && ['wall', 'wall_corner', 'door', 'doorway'].includes(a?.role)) {
            const weight = window.experimentalHeightUI?.weight(tile);
            if (weight !== undefined) {
                c.font = `${Math.min(16, s * .4)}px sans-serif`;
                c.textAlign = 'center'; c.textBaseline = 'middle';
                c.lineWidth = 3; c.strokeStyle = '#000';
                c.fillStyle = weight === 4 ? '#66ffff' : weight === 8 ? '#fff' : '#ffdc38';
                c.strokeText(String(weight), px + s / 2, py + s / 2);
                c.fillText(String(weight), px + s / 2, py + s / 2);
            }
        }
        if (group.has(key)) { c.strokeStyle = '#fff'; c.lineWidth = 1; c.strokeRect(px, py, s, s); }
    }
    window.experimentalHeightUI?.drawHeightLabels(c);
    window.experimentalHeightUI?.drawTraversal(c);
    if (worldLocationMode) {
        c.strokeStyle = '#fff'; c.lineWidth = 2;
        for (const {x,y} of worldSelection.values())
            c.strokeRect(worldX + x * 8 * worldScale, worldY + y * 8 * worldScale, 8 * worldScale, 8 * worldScale);
    }
    if (worldCell) {
        c.strokeStyle = '#ffdc38'; c.lineWidth = 3;
        c.strokeRect(worldX + worldCell.x * 8 * worldScale, worldY + worldCell.y * 8 * worldScale, 8 * worldScale, 8 * worldScale);
    }
    if(worldDrag?.box) {
        const a=worldDrag.startCell,b=worldDrag.endCell,size=8*worldScale;
        c.strokeStyle=worldDrag.box==='keep'?'#66ffff':'#ff6666';c.lineWidth=2;c.setLineDash([6,4]);
        c.strokeRect(worldX+Math.min(a.x,b.x)*size,worldY+Math.min(a.y,b.y)*size,(Math.abs(a.x-b.x)+1)*size,(Math.abs(a.y-b.y)+1)*size);
        c.setLineDash([]);
    }
    document.getElementById('worldZoom').textContent = worldScale.toFixed(2) + '×';
    window.experimentalHeightUI?.showPlacement();
}
function fitWorld(){worldScale=Math.min(vp.clientWidth,vp.clientHeight)/4096;worldX=(vp.clientWidth-4096*worldScale)/2;worldY=(vp.clientHeight-4096*worldScale)/2;drawWorld()}
function restoreWorldView(){
    const view=worldWorkspace.worlds[worldName].view;
    if(view){worldX=view.x;worldY=view.y;worldScale=view.scale;drawWorld();}
    else fitWorld();
    worldWorkspace.world=worldName;saveWorldWorkspace();
}
async function loadWorld(){let token=++worldLoading;wi.textContent='Loading world…';try{if(!worldManifest){let r=await fetch('/api/world');worldManifest=await r.json();if(!r.ok)throw Error(worldManifest.error)}let name=document.getElementById('worldChoice').value;let [buffer,image]=await Promise.all([fetch('/world/'+name+'.bin').then(r=>{if(!r.ok)throw Error('Placement grid unavailable');return r.arrayBuffer()}),new Promise((resolve,reject)=>{let im=new Image();im.onload=()=>resolve(im);im.onerror=()=>reject(Error('World image unavailable'));im.src='/world/'+name+'.png'})]);if(token!==worldLoading)return;worldName=name;worldGrid=new DataView(buffer);worldImage=image;worldCell=null;wi.textContent='Click any 8×8 cell to inspect its placement and edit shared metadata.';document.getElementById('worldLimit').textContent=worldManifest.limitations;restoreWorldView()}catch(e){wi.textContent='World map unavailable: '+e.message}}
switcher.onclick=()=>{worldActive=!worldActive;worldWorkspace.tab=worldActive?'world':'grid';saveWorldWorkspace();grid.style.display=worldActive?'none':'';worldPane.style.display=worldActive?'flex':'none';switcher.textContent=worldActive?'Tile grid':'World map';if(worldActive){if(worldGrid)drawWorld();else loadWorld()}};
document.getElementById('worldChoice').onchange=async()=>{
    try { await window.experimentalHeightUI?.selectionChanging(); }
    catch (error) { document.getElementById('worldChoice').value=worldName;wi.textContent=error.message;return; }
    worldSelection.clear();worldLocationMode=false;await loadWorld();
};document.getElementById('worldFit').onclick=fitWorld;document.getElementById('worldFacing').onchange=drawWorld;
vp.addEventListener('wheel',e=>{e.preventDefault();let r=vp.getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top,next=Math.max(.03,Math.min(16,worldScale*Math.exp(-e.deltaY*.002)));worldX=x-(x-worldX)*next/worldScale;worldY=y-(y-worldY)*next/worldScale;worldScale=next;drawWorld()},{passive:false});
let worldDrag=null, worldSelectionQueue=Promise.resolve();
function worldPointerCell(e) {
    const r=vp.getBoundingClientRect();
    return {x:Math.floor((e.clientX-r.left-worldX)/worldScale/8),y:Math.floor((e.clientY-r.top-worldY)/worldScale/8)};
}
function brushWorldSelection(e,drag) {
    const to=worldPointerCell(e),from=drag.lastCell||to;
    drag.lastCell=to;
    if(!e.altKey||!e.metaKey)return;
    drag.brushed=true;
    const locations=[],steps=Math.max(Math.abs(to.x-from.x),Math.abs(to.y-from.y));
    // Include cells between pointer events, not just the event endpoints.
    for(let i=0;i<=steps;i++) {
        const x=Math.round(from.x+(to.x-from.x)*(steps?i/steps:0)),y=Math.round(from.y+(to.y-from.y)*(steps?i/steps:0)),key=`${x}:${y}`;
        if(x<0||x>=512||y<0||y>=512||drag.visited.has(key))continue;
        drag.visited.add(key);locations.push({x,y});
    }
    if(!locations.length)return;
    const name=worldName,gridSnapshot=worldGrid;
    worldSelectionQueue=worldSelectionQueue.then(async()=>{
        if(name!==worldName||gridSnapshot!==worldGrid)return;
        if(!await selectWorldLocations(locations,{metaKey:true},true))drag.visited.clear();
    });
}
vp.onpointerdown=e=>{
    if(e.button!==0||worldSelecting)return;
    vp.setPointerCapture(e.pointerId);
    worldDrag={x:e.clientX,y:e.clientY,ox:worldX,oy:worldY,moved:false,noPan:e.altKey,visited:new Set()};
    if(!e.altKey&&['keep','remove'].includes(worldBoxMode.value)) {
        worldDrag.box=worldBoxMode.value;
        worldDrag.startCell=worldDrag.endCell=worldPointerCell(e);
        drawWorld();return;
    }
    if(e.altKey)brushWorldSelection(e,worldDrag);
};
vp.onpointermove=e=>{
    const drag=worldDrag;if(!drag)return;
    const dx=e.clientX-drag.x,dy=e.clientY-drag.y;
    if(Math.hypot(dx,dy)>3)drag.moved=true;
    if(drag.box){drag.endCell=worldPointerCell(e);drawWorld();return;}
    if(e.altKey||drag.noPan){drag.noPan=true;brushWorldSelection(e,drag);return;}
    if(drag.moved){worldX=drag.ox+dx;worldY=drag.oy+dy;drawWorld();}
};
vp.onpointercancel=()=>{worldDrag=null;drawWorld();};
let worldSelecting = false;
vp.onpointerup=async e=>{
    const drag=worldDrag;worldDrag=null;
    if(drag?.box){
        drag.endCell=worldPointerCell(e);
        await worldSelectionQueue;
        await filterWorldSelectionBox(drag.startCell,drag.endCell,drag.box);
        drawWorld();return;
    }
    if(drag&&(drag.noPan||e.altKey))brushWorldSelection(e,drag);
    if(drag?.brushed){await worldSelectionQueue;return;}
    if(!drag||drag.moved||!worldGrid||!meta||worldSelecting)return;
    await worldSelectionQueue;
    await selectWorldLocations([worldPointerCell(e)],e);
};
async function selectWorldLocations(locations,e,brush=false) {
    if(!worldGrid||!meta)return false;
    locations=locations.filter(({x,y})=>x>=0&&x<512&&y>=0&&y<512);
    if(!locations.length)return false;
    worldSelecting=true;
    try {
        await window.experimentalHeightUI?.selectionChanging();
        for(const {x,y} of locations) {
        const at=`${x}:${y}`;
        if(!brush && e.shiftKey && worldCell) {
            const anchor={...worldCell};
            if(!e.metaKey)worldSelection.clear();
            for(let cy=Math.min(anchor.y,y);cy<=Math.max(anchor.y,y);cy++)
                for(let cx=Math.min(anchor.x,x);cx<=Math.max(anchor.x,x);cx++)
                    worldSelection.set(`${cx}:${cy}`,{x:cx,y:cy});
            worldLocationMode=true;
        } else if(e.metaKey) {
            if(!worldSelection.size&&worldCell)worldSelection.set(`${worldCell.x}:${worldCell.y}`,{...worldCell});
            if(worldSelection.has(at))worldSelection.delete(at);else worldSelection.set(at,{x,y});
            if(worldSelection.size>1)worldLocationMode=true;
        } else {worldSelection.clear();worldSelection.set(at,{x,y});worldLocationMode=false;}
        worldCell=worldSelection.has(at)?{x,y}:[...worldSelection.values()].at(-1)||null;
        }
        if(brush)worldLocationMode=true;
        await showWorldSelection();
        return true;
    } catch(error){wi.textContent=error.message;return false;}
    finally{worldSelecting=false;}
}
async function filterWorldSelectionBox(a,b,mode) {
    if(!worldGrid||!meta||worldSelecting)return;
    worldSelecting=true;
    try {
        await window.experimentalHeightUI?.selectionChanging();
        let locations=[...worldSelection.values()];
        if(!worldLocationMode) {
            const aliases=new Set(byId.get(selected)?.aliases||[]);
            if(!aliases.size)throw Error('Click a tile type first, or select exact locations to filter.');
            locations=[];
            for(let y=0;y<512;y++)for(let x=0;x<512;x++)
                if(aliases.has(worldManifest.content_ids[worldRecord(x,y).id]))locations.push({x,y});
        }
        const x0=Math.min(a.x,b.x),x1=Math.max(a.x,b.x),y0=Math.min(a.y,b.y),y1=Math.max(a.y,b.y);
        const kept=locations.filter(({x,y})=>{
            const inside=x>=x0&&x<=x1&&y>=y0&&y<=y1;
            return mode==='keep'?inside:!inside;
        });
        worldSelection.clear();
        for(const at of kept)worldSelection.set(`${at.x}:${at.y}`,at);
        worldLocationMode=true;
        if(!worldCell||!worldSelection.has(`${worldCell.x}:${worldCell.y}`))worldCell=kept.at(-1)||null;
        await showWorldSelection();
    } catch(error){wi.textContent=error.message;}
    finally{worldSelecting=false;}
}
async function showWorldSelection() {
        window.experimentalHeightUI?.selectionChanged();
        if(!worldCell){selected=null;detailData=null;detail.replaceChildren();wi.textContent='No locations selected.';drawWorld();return;}
        const cell=worldRecord(worldCell.x,worldCell.y),key=worldManifest.content_ids[cell.id],target=worldTarget(key);
        const count=target?target.aliases.reduce((sum,a)=>sum+(worldManifest.counts[worldName][a]||0),0):0;
        wi.textContent=worldLocationMode?`${worldSelection.size} exact selected locations · Offsets/baking affect only these locations. Role and weight edits still affect all copies of their artwork. Use box drag modes to keep/remove locations; Cmd+Option-drag to toggle.`:
            `${worldName} cell (${worldCell.x}, ${worldCell.y}) · content_id ${key||'unresolved'} · ${count} placements share these semantics. Cmd-click to add/remove locations.`;
        if(target)await select(target.content_id);
        else {detailData=null;detail.replaceChildren();wi.textContent+=' No editable catalog entry for this cell.';}
        drawWorld();
}
new ResizeObserver(drawWorld).observe(vp);
// Explicit hooks avoid DOM-observer feedback when experimental controls update.
const heightScript = document.createElement('script');
heightScript.src = '/height-ui.js'; document.body.append(heightScript);
if(worldWorkspace.tab==='world')switcher.onclick();
