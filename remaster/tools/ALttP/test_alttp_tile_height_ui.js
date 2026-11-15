/* Portable editor integration mock; no browser, ROM, profile or network writes. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {calculateHeights} = require('./alttp_tile_height_worker.js');

async function main() {
    const elements = new Map(), workers = [],heightImages=[];
    const element = id => {
        if (!elements.has(id)) elements.set(id, {value: '', checked: false, textContent: '',
            style: {}, append() {}, remove() {}, addEventListener(event, handler) { this[event] = handler; }});
        return elements.get(id);
    };
    for (const [id, value] of [['worldChoice', 'light'], ['heightReference', '0'], ['heightOpacity', '.85']]) element(id).value = value;
    const drawing = new Proxy({}, {get: (target, key) => target[key] || (() => {})});
    const config = {revision: 'weights-1', semantic_revision: 'semantics-1', weights: {}, path: 'temporary-mock-only'};
    const offsets = {revision:'semantics-1',path:'mock-profile',enabled:true,offsets:{},heights:{},nonuniform:[],terrain:[]};
    const floor = {content_id: 'bg', annotation: {role: 'floor'}, aliases: ['bg']};
    const context = {
        console, setInterval() {}, clearInterval() {}, clearTimeout() {},
        document: {getElementById: element, createElement: () => ({style: {},children:[],
            append(...items) {this.children.push(...items);}, getContext: () => ({...drawing,
                createImageData: (w, h) => ({data: new Uint8ClampedArray(w * h * 4)}), putImageData(pixels) {heightImages.push(pixels.data.slice());}})})},
        worldPane: {insertBefore() {}}, vp: {getBoundingClientRect: () => ({width: 24, height: 16})},
        worldX: -800, worldY: -1600, worldScale: 1, worldName: 'light', worldActive: true,
        worldGrid: {}, worldManifest: {content_ids: ['bg']}, worldCell: {x: 101, y: 201},
        worldRecord: () => ({id: 0, flips: 0}), worldTarget: () => floor,
        flippedFacing: (normal, flips) => { assert.equal(flips, 0); return normal; },
        byId: new Map([['bg', floor]]), selected: null, detailData: null,
        saving: false, pendingId: null, saveTimer: null, meta: {revision: 'semantics-1'},
        complete: () => true, annotation: () => floor.annotation,
        save() {}, renderDetail() {}, renderGrid() {}, changeRole() {}, toggleNormal() {}, update() {}, scheduleSave() {},
        fetch: async (url, options) => {
            if(url.startsWith('/api/annotation/')&&options?.method==='PUT') {
                const id=decodeURIComponent(url.split('/').at(-1)),annotation=JSON.parse(options.body);
                config.semantic_revision+='-semantic';offsets.revision=config.semantic_revision;config.revision+='-semantic';
                return {ok:true,json:async()=>({annotation,content_ids:[id],revision:config.semantic_revision})};
            }
            if(url.startsWith('/api/weights/')&&options?.method==='PUT') {
                config.weights[decodeURIComponent(url.split('/').at(-1))]=JSON.parse(options.body).weight;
                config.revision+='-weight';
            }
            if (url === '/api/height-offsets' && options?.method === 'PUT') {
                const value=JSON.parse(options.body);
                assert.equal(options.headers['If-Match'],context.meta.revision);
                for(const at of value.values||value.locations){const {x,y}=at,key=`${value.world}:${x}:${y}`,offset=value.values?at.offset:value.offset;
                if(value.action==='remove-terrain-heights'){
                    delete offsets.terrain_heights[key];offsets.terrain=offsets.terrain.filter(k=>k!==key);offsets.nonuniform=offsets.nonuniform.filter(k=>k!==key);
                    if(Object.hasOwn(offsets.offsets,key))offsets.heights[key]=offsets.offsets[key];else delete offsets.heights[key];continue;
                }
                if(offset===null){delete offsets.offsets[key];delete offsets.heights[key];}
                else {offsets.offsets[key]=offset;offsets.heights[key]=offset;}
                }
                offsets.revision+='-saved';config.semantic_revision=offsets.revision;config.revision+='-saved';
            }
            return {ok:true,json:async()=>structuredClone(url==='/api/height-offsets'?offsets:config)};
        },
        Worker: class {
            constructor() { workers.push(this); }
            postMessage(message) { this.message = structuredClone(message); }
            terminate() { this.terminated = true; }
            async finish() {
                const data = calculateHeights(this.message.input);
                await this.onmessage({data});
            }
        }
    };
    const workspace={light:{},dark:{}};
    context.window = {tileSemanticWorkspace:{getWorld:name=>workspace[name],updateWorld:(name,patch)=>Object.assign(workspace[name],structuredClone(patch))}};
    context.drawWorld = () => {
        context.window.experimentalHeightUI?.viewChanged();
        context.window.experimentalHeightUI?.showPlacement();
    };
    vm.createContext(context);
    vm.runInContext(fs.readFileSync(require.resolve('./alttp_tile_height_ui.js'), 'utf8'), context);
    const ui = context.window.experimentalHeightUI;
    await Promise.resolve(); await Promise.resolve();
    const run = () => element('heightRun').onclick();
    const setHeight = value => { element('heightReference').value = value; element('heightReference').oninput(); };
    assert.equal(element('heightRunPrevious').disabled,true);
    await run();
    assert.match(element('heightStatus').textContent, /Select a visible tile/);
    assert.equal(workers.length, 0);
    assert.equal(element('heightRunPrevious').disabled,true);

    element('heightAnchor').onclick(); setHeight('7.5');
    await run();
    const first = workers.at(-1), input = first.message.input;
    assert.equal(element('heightRunPrevious').disabled,true);
    assert.deepEqual(input.bounds, {x0: 100, y0: 200, width: 3, height: 2});
    assert.equal(input.grid.byteLength, 3 * 2 * 8);
    assert.deepEqual(input.reference, {x: 101, y: 201, world: 'light', height: 7.5});
    await first.finish();
    assert.match(element('heightStatus').textContent, /light calculated/);
    assert.match(element('heightPlacement').textContent, /raw height 7.5/);
    assert.match(element('heightPlacement').textContent, /processed directions:.*stops here:/);
    assert.equal(element('worldHeight').checked,true);
    assert.match(element('heightActivity').textContent,/Complete — heightmap shown/);
    assert.equal(element('heightCancel').disabled,true);
    assert.equal(element('heightRunPrevious').disabled,false);
    const freshStatus = element('heightStatus').textContent;
    element('worldWeights').checked=true;element('worldWeights').oninput();
    element('worldCalculatedHeights').checked=true;element('worldCalculatedHeights').oninput();
    assert.equal(element('worldWeights').checked,false);
    const labels=[];
    const labelDrawing={save(){},restore(){},strokeText(){},fillText(text){labels.push(text);}};
    context.worldScale=2;ui.drawHeightLabels(labelDrawing);assert.equal(labels.length,0);
    context.worldScale=3;context.worldX=-2400;context.worldY=-4800;
    ui.drawHeightLabels(labelDrawing);assert(labels.includes('7.5'));
    context.worldName='dark';labels.length=0;ui.drawHeightLabels(labelDrawing);
    assert(labels.length>0);assert(labels.every(text=>text==='0')); // no calculation in this world
    context.worldName='light';context.worldScale=1;context.worldX=-800;context.worldY=-1600;
    element('worldWeights').checked=true;element('worldWeights').oninput();
    assert.equal(element('worldCalculatedHeights').checked,false);
    element('worldWeights').checked=false;element('worldWeights').oninput();
    const strokes=[];
    const arrowDrawing=new Proxy({save(){},restore(){},setLineDash(){},beginPath(){},
        moveTo(){},lineTo(){},stroke(){strokes.push(this.strokeStyle);}},
        {get:(target,key)=>target[key]||(()=>{})});
    element('worldHeightArrows').checked=true;
    context.worldScale=2;
    ui.drawTraversal(arrowDrawing);
    assert(strokes.includes('#00ffff'));
    assert(strokes.includes('#ff4040'));
    assert.match(element('heightArrowLegend').textContent,/Showing recorded/);
    context.worldScale=1;
    ui.drawTraversal(arrowDrawing);
    assert.match(element('heightArrowLegend').textContent,/Zoom to/);
    element('worldHeightArrows').checked=false;
    for (let i = 0; i < 10; i++) context.drawWorld();
    assert.equal(element('heightStatus').textContent, freshStatus); // no redraw invalidation loop

    context.worldX -= 8; context.drawWorld();
    assert.match(element('heightStatus').textContent, /STALE.*view/);
    element('worldHeightArrows').checked=true;
    context.worldScale=2; ui.drawTraversal(arrowDrawing);
    assert.match(element('heightArrowLegend').textContent,/STALE/);
    context.worldScale=1; element('worldHeightArrows').checked=false;
    assert.match(element('heightBounds').textContent, /Last calculated light/);
    await run();
    context.worldX -= 8; context.drawWorld();
    await workers.at(-1).finish();
    assert.match(element('heightStatus').textContent, /discarded/); // movement while computing

    context.worldX = -800; context.drawWorld();
    await run();
    const beforeSwitch = workers.at(-1);
    element('worldChoice').value = 'dark'; element('worldChoice').change();
    context.worldName = 'dark'; context.worldCell = {x: 101, y: 201}; context.drawWorld();
    assert.equal(element('heightRunPrevious').disabled,true); // no successful dark calculation yet
    await beforeSwitch.finish();
    assert.match(element('heightStatus').textContent, /discarded/);
    assert.equal(element('heightReference').value, '0');
    assert.match(element('heightAnchorInfo').textContent, /dark: no reference/);
    assert.match(element('heightPlacement').textContent, /unknown/); // never expose light result as dark
    await run();
    assert.match(element('heightStatus').textContent, /Select a visible tile/);
    element('heightAnchor').onclick(); setHeight('-3.25');
    await run(); await workers.at(-1).finish();
    assert.match(element('heightStatus').textContent, /dark calculated/);
    assert.match(element('heightPlacement').textContent, /raw height -3.25/);
    setHeight('-4');
    assert.match(element('heightStatus').textContent, /STALE/);
    await run(); context.update(); await workers.at(-1).finish();
    assert.match(element('heightStatus').textContent, /discarded/); // semantic edits while computing

    element('worldChoice').value = 'light'; element('worldChoice').change();
    context.worldName = 'light'; context.drawWorld();
    assert.equal(element('heightReference').value, '7.5');
    assert.match(element('heightAnchorInfo').textContent, /light reference: cell \(101, 201\)/);
    context.worldX = -1600; context.drawWorld();
    const count = workers.length;
    await run();
    assert.equal(workers.length, count);
    assert.match(element('heightStatus').textContent, /Reference is outside visible bounds/);
    // Display saved scalar and configured raw deltas separately from actual skip.
    context.worldX = -800; context.worldCell = {x:101,y:201};
    floor.annotation = {role:'wall',normals:['south_east']}; config.weights.bg = .5;
    context.update(); context.drawWorld();
    await run(); await workers.at(-1).finish();
    assert.match(element('heightPlacement').textContent, /weight 0.5.*cardinal-axis ΔN 0.5 · ΔE -0.5/);
    assert.match(element('heightPlacement').textContent, /diagonal crossings use one signed weight/);
    floor.annotation = {role:'unknown'};
    context.worldCell = {x:100,y:200}; context.update();
    await run(); await workers.at(-1).finish();
    assert.match(element('heightStatus').textContent, /1 assigned, 5 NO HEIGHT/);
    assert.match(element('heightPlacement').textContent, /UNSET — no path assigned a height/);
    element('worldCalculatedHeights').checked=true;context.worldScale=3;
    context.worldX=-2424;context.worldY=-4800;
    labels.length=0;ui.drawHeightLabels(labelDrawing);assert(labels.includes('?'));
    context.worldScale=1;context.worldX=-800;context.worldY=-1600;element('worldCalculatedHeights').checked=false;
    context.worldX -= 8; context.drawWorld();
    assert.match(element('heightPlacement').textContent, /STALE/);
    await run();
    const cancelled = workers.at(-1);
    element('heightCancel').onclick();
    assert.equal(cancelled.terminated,true);
    assert.equal(element('heightRun').disabled,false);
    assert.match(element('heightActivity').textContent,/Cancelled/);
    await cancelled.finish();
    assert.match(element('heightActivity').textContent,/Cancelled/);
    await run();
    await workers.at(-1).onmessage({data:{heights:new Float64Array(6),stats:{version:'old-worker'}}});
    assert.match(element('heightStatus').textContent,/version mismatch/);
    assert.match(element('heightActivity').textContent,/Failed/);
    assert.equal(element('heightRun').disabled,false);
    await run();
    workers.at(-1).onmessageerror();
    assert.match(element('heightStatus').textContent,/could not be decoded/);
    assert.equal(element('heightRun').disabled,false);
    const createElement = context.document.createElement;
    await run();
    context.document.createElement = () => { throw Error('simulated canvas failure'); };
    await workers.at(-1).finish();
    assert.match(element('heightStatus').textContent,/Calculation\/display failed: simulated canvas failure/);
    assert.equal(element('heightRun').disabled,false);
    context.document.createElement = createElement;
    // Placement save is independent of artwork metadata, including explicit zero.
    floor.annotation={role:'floor'};
    context.worldCell={x:101,y:201};context.drawWorld();
    element('heightOffset').value='12';
    let prevented=false;
    await element('heightOffset').onkeydown({key:'Enter',preventDefault(){prevented=true;}});
    assert(prevented);
    element('worldCalculatedHeights').checked=true;
    context.worldScale=3;context.worldX=-2424;context.worldY=-4824;
    labels.length=0;ui.drawHeightLabels(labelDrawing);assert(labels.includes('12')); // immediate, before recalculation
    assert.equal(labelDrawing.fillStyle,'#66ffff');assert.equal(labelDrawing.globalAlpha,1);
    context.worldScale=1;context.worldX=-800;context.worldY=-1600;
    await run();
    assert.deepEqual(workers.at(-1).message.input.authoredHeights,[{x:101,y:201,height:12}]);
    await workers.at(-1).finish();
    assert.match(element('heightPlacement').textContent,/raw height 12/);
    element('heightOffset').value='0';await element('heightOffsetSave').onclick();
    context.worldScale=3;context.worldX=-2424;context.worldY=-4824;
    labels.length=0;ui.drawHeightLabels(labelDrawing);assert.deepEqual(labels,['0']); // authored zero wins over old 12
    assert.equal(labelDrawing.fillStyle,'#66ffff');
    context.worldScale=1;context.worldX=-800;context.worldY=-1600;
    await run();assert.equal(workers.at(-1).message.input.authoredHeights[0].height,0);
    await workers.at(-1).finish();
    await element('heightOffsetRemove').onclick();await run();
    assert.equal(workers.at(-1).message.input.authoredHeights.length,0);await workers.at(-1).finish();
    // Compact multi-selection: mixed settings, bulk role changes and weights.
    const second={content_id:'second',annotation:{role:'other',normals:[],note:'preserve me'}};
    context.byId.set('second',second);floor.annotation={role:'floor',normals:[],note:'first note',water:true};
    context.worldSelection=new Map([['1:1',{x:1,y:1}],['2:1',{x:2,y:1}]]);
    context.worldSelectedTiles=()=>[floor,second];context.selected='bg';context.detailData={};
    context.roles=['floor','other'];context.labels={floor:'Floor',other:'Other'};
    context.detail={children:[],append(...items){this.children.push(...items);},replaceChildren(){this.children=[];}};
    config.weights.bg=1;config.weights.second=2;
    vm.runInContext('renderDetail()',context);
    const buttons=context.detail.children.find(e=>e.className==='roleButtons');
    assert(buttons.children.every(b=>b.className===''));
    const panel=context.detail.children.find(e=>e.id==='experimentalWeightPanel');
    const inputWeight=panel.children.find(e=>e.type==='number');
    assert.equal(inputWeight.placeholder,'–');assert.equal(inputWeight.disabled,false);
    buttons.children[1].onclick();await ui.selectionChanging();
    assert.equal(floor.annotation.role,'other');assert.equal(second.annotation.role,'other');
    assert.equal(floor.annotation.water,undefined);assert.equal(second.annotation.note,'preserve me');
    vm.runInContext('renderDetail()',context);
    const weightPanel=context.detail.children.find(e=>e.id==='experimentalWeightPanel');
    const bulkWeight=weightPanel.children.find(e=>e.type==='number');
    assert.equal(bulkWeight.step,'1');assert.equal(bulkWeight.max,'32');
    bulkWeight.value='2.5';bulkWeight.onchange();
    assert.equal(config.weights.bg,1);assert.equal(config.weights.second,2);
    bulkWeight.value='3';bulkWeight.onchange();await ui.selectionChanging();
    assert.equal(config.weights.bg,3);assert.equal(config.weights.second,3);
    // Bulk offsets target coordinates, not unique artwork or just the active cell.
    offsets.offsets['light:1:1']=2;offsets.heights['light:1:1']=2;
    await run();await workers.at(-1).finish();
    context.worldCell={x:2,y:1};ui.selectionChanged();context.drawWorld();
    assert.equal(element('heightOffset').placeholder,'–');
    element('heightOffset').value='18';await element('heightOffsetSave').onclick();
    for(const key of ['light:1:1','light:2:1'])assert.equal(offsets.heights[key],18);
    assert.equal(element('heightOffset').value,'18');
    element('heightOffset').value='0';await element('heightOffset').onkeydown({key:'Enter',preventDefault(){}});
    for(const key of ['light:1:1','light:2:1'])assert.equal(offsets.heights[key],0);
    await element('heightOffsetRemove').onclick();
    for(const key of ['light:1:1','light:2:1'])assert(!Object.hasOwn(offsets.heights,key));
    // Bake each coordinate's own calculated value, with no fallback to zero.
    context.worldSelection=new Map([['100:200',{x:100,y:200}],['101:200',{x:101,y:200}]]);
    context.worldCell={x:101,y:200};floor.annotation={role:'floor',normals:[]};context.selected=null;
    context.worldSelectedTiles=()=>[floor,second];second.annotation={role:'floor',normals:[]};
    setHeight('6');await run();await workers.at(-1).finish();
    assert.equal(element('heightOffsetBake').disabled,false);
    // Selection-panel resize/pan stales viewport scope, not calculated cells.
    context.worldX-=8;context.drawWorld();
    assert.match(element('heightStatus').textContent,/STALE.*view/);
    assert.equal(element('heightOffsetBake').disabled,false);
    await element('heightOffsetBake').onclick();
    assert.equal(offsets.heights['light:100:200'],6);assert.equal(offsets.heights['light:101:200'],6);
    context.worldX+=8;context.drawWorld();
    offsets.heights['light:100:200']=20;offsets.offsets['light:100:200']=20;
    await run();await workers.at(-1).finish();
    await element('heightOffsetBake').onclick();
    assert.equal(offsets.heights['light:100:200'],20);assert.equal(offsets.heights['light:101:200'],6);
    const baked=JSON.stringify(offsets.heights);
    await element('heightOffsetBake').onclick(); // result now stale
    assert.equal(JSON.stringify(offsets.heights),baked);
    assert.match(element('heightOffsetState').textContent,/Baked 2 displayed heights/);
    // Conflicting retained numbers are explicitly user-approvable, even stale.
    await element('heightOffsetRemove').onclick();await run();
    const conflictWorker=workers.at(-1),conflicting=calculateHeights(conflictWorker.message.input);
    conflicting.heights[0]=16;conflicting.heights[1]=12;conflicting.conflicts[0]=1;
    await conflictWorker.onmessage({data:conflicting});
    setHeight('9'); // stale reference input, numbers still displayed
    await element('heightOffsetBake').onclick();
    assert.equal(offsets.heights['light:100:200'],16);assert.equal(offsets.heights['light:101:200'],12);
    // Bake another selection without recalculating after the first bake.
    context.worldSelection=new Map([['102:200',{x:102,y:200}]]);context.worldCell={x:102,y:200};
    await element('heightOffsetBake').onclick();assert.equal(offsets.heights['light:102:200'],6);
    // Exact label precedence: authored value, including zero, overrides old calculation.
    context.worldSelection=new Map([['100:200',{x:100,y:200}]]);context.worldCell={x:100,y:200};
    element('heightOffset').value='0';await element('heightOffsetSave').onclick();
    await element('heightOffsetBake').onclick();assert.equal(offsets.heights['light:100:200'],0);
    // Display-default 0 is intentionally bakeable; ? is not.
    context.worldSelection=new Map([['110:210',{x:110,y:210}]]);context.worldCell={x:110,y:210};
    await element('heightOffsetBake').onclick();assert.equal(offsets.heights['light:110:210'],0);
    context.worldSelection=new Map([['100:200',{x:100,y:200}],['101:200',{x:101,y:200}]]);context.worldCell={x:101,y:200};
    await element('heightOffsetRemove').onclick();await run();
    const missingWorker=workers.at(-1),missing=calculateHeights(missingWorker.message.input);
    missing.assigned[1]=0;missing.heights[1]=NaN;await missingWorker.onmessage({data:missing});
    const beforeMissing=JSON.stringify(offsets.heights);
    await element('heightOffsetBake').onclick();
    assert.match(element('heightOffsetState').textContent,/shows \?/);
    assert.equal(JSON.stringify(offsets.heights),beforeMissing);
    // Unreached authored tiles use grayscale shading and magenta numeric labels;
    // saving/removing updates the existing overlay without recalculating.
    context.worldSelection=new Map([['101:200',{x:101,y:200}]]);context.worldCell={x:101,y:200};
    element('heightOffset').value='-16';await element('heightOffsetSave').onclick();
    const shade=heightImages.at(-1).slice(4,8);
    assert.equal(shade[3],255);
    assert.equal(shade[0],shade[1]);assert.equal(shade[1],shade[2]);
    const coverageLabels=[];
    const coverageDrawing={save(){},restore(){},strokeText(){},fillText(text){coverageLabels.push({text,color:this.fillStyle});}};
    element('worldCalculatedHeights').checked=true;
    context.worldScale=3;context.worldX=-2424;context.worldY=-4800;
    ui.drawHeightLabels(coverageDrawing);
    assert(coverageLabels.some(label=>label.text==='-16'&&label.color==='#ff66ee'));
    assert.equal(missing.assigned[1],0); // display treatment never assigns traversal coverage
    const expectedGray=Math.max(0,Math.min(255,Math.round((-16-missing.stats.min)/(missing.stats.max-missing.stats.min||1)*255)));
    assert.deepEqual([...shade],[expectedGray,expectedGray,expectedGray,255]);
    await element('heightOffsetRemove').onclick();
    assert.equal(heightImages.at(-1)[7],255);
    context.worldScale=1;context.worldX=-800;context.worldY=-1600;
    // Previous-area recalculation uses current inputs, not the current viewport.
    const previous = () => element('heightRunPrevious').onclick();
    const savedBounds=structuredClone(workers.at(-1).message.input.bounds);
    context.worldX=-1600;context.worldScale=2;context.drawWorld();setHeight('32');
    assert.equal(element('heightRunPrevious').disabled,false);
    await previous();
    assert.equal(element('heightRunPrevious').disabled,true);
    assert.deepEqual(workers.at(-1).message.input.bounds,savedBounds);
    assert.equal(workers.at(-1).message.input.reference.height,32);
    await workers.at(-1).finish();
    assert.match(element('heightActivity').textContent,/Complete/);
    assert.equal(element('heightRunPrevious').disabled,false);
    // A cancelled, failed, or discarded visible-area attempt cannot replace it.
    context.worldScale=1;context.worldX=-808;context.drawWorld();
    await run();element('heightCancel').onclick();
    await previous();assert.deepEqual(workers.at(-1).message.input.bounds,savedBounds);
    await workers.at(-1).finish();
    await run();workers.at(-1).onerror({message:'test failure'});
    await previous();assert.deepEqual(workers.at(-1).message.input.bounds,savedBounds);
    await workers.at(-1).finish();
    await run();context.worldX-=8;context.drawWorld();await workers.at(-1).finish();
    assert.match(element('heightStatus').textContent,/discarded/);
    await previous();assert.deepEqual(workers.at(-1).message.input.bounds,savedBounds);
    await workers.at(-1).finish();
    // A new successful visible calculation becomes the next previous area.
    context.worldX=-808;context.drawWorld();await run();await workers.at(-1).finish();
    const newBounds=structuredClone(workers.at(-1).message.input.bounds);
    assert.notDeepEqual(newBounds,savedBounds);
    context.worldX=-1600;context.drawWorld();await previous();
    assert.deepEqual(workers.at(-1).message.input.bounds,newBounds);
    await workers.at(-1).finish();
    // Relative edits snapshot each displayed value, not one common scalar.
    context.worldScale=1;context.worldX=-800;context.drawWorld();
    context.worldSelection=new Map([['100:200',{x:100,y:200}],['101:200',{x:101,y:200}]]);context.worldCell={x:101,y:200};
    await element('heightOffsetRemove').onclick();await run();
    const adjustWorker=workers.at(-1),adjusted=calculateHeights(adjustWorker.message.input);
    adjusted.heights[0]=16;adjusted.heights[1]=12;adjusted.conflicts[0]=1;
    await adjustWorker.onmessage({data:adjusted});setHeight('9');
    // A resize/pan arriving during save preparation must not reject captured values.
    const originalFetch=context.fetch;
    let duringVerification=null;
    context.fetch=async(url,options)=>{
        if(url==='/api/weights'&&duringVerification){const callback=duringVerification;duringVerification=null;callback();}
        return originalFetch(url,options);
    };
    duringVerification=()=>{context.worldX-=8;context.drawWorld();};
    element('heightOffsetDelta').value='3';await element('heightOffsetAdjust').onclick();
    assert.equal(offsets.heights['light:100:200'],19);assert.equal(offsets.heights['light:101:200'],15);
    assert.match(element('heightOffsetState').textContent,/Adjusted 2 displayed heights by \+3/);
    // Repeated edits use newly authored values without recalculating.
    element('heightOffsetDelta').value='-5';let adjustedEnter=false;
    await element('heightOffsetDelta').onkeydown({key:'Enter',preventDefault(){adjustedEnter=true;}});
    assert(adjustedEnter);assert.equal(offsets.heights['light:100:200'],14);assert.equal(offsets.heights['light:101:200'],10);
    const beforeInvalid=JSON.stringify(offsets.heights);
    // Genuine concurrent edits are still rejected, with adjustment-specific wording.
    element('heightOffsetDelta').value='1';
    duringVerification=()=>setHeight('10');
    await element('heightOffsetAdjust').onclick();
    assert.match(element('heightOffsetState').textContent,/Editing inputs changed while preparing the adjustment/);
    assert.equal(JSON.stringify(offsets.heights),beforeInvalid);
    setHeight('9');
    duringVerification=()=>{config.revision+='-external';};
    await element('heightOffsetAdjust').onclick();
    assert.match(element('heightOffsetState').textContent,/Saved profile, metadata or weights changed while preparing the adjustment/);
    assert.equal(JSON.stringify(offsets.heights),beforeInvalid);
    context.fetch=originalFetch;
    // Refresh the saved-input revision after the simulated external weight edit.
    context.worldX=-800;context.drawWorld();await run();await workers.at(-1).finish();
    for(const delta of ['', '0.5', 'Infinity', '32767']) {
        element('heightOffsetDelta').value=delta;await element('heightOffsetAdjust').onclick();
        assert.match(element('heightOffsetState').textContent,/Not adjusted/);
        assert.equal(JSON.stringify(offsets.heights),beforeInvalid);
    }
    // Any unknown height rejects all cells atomically.
    await element('heightOffsetRemove').onclick();await run();
    const unknownWorker=workers.at(-1),unknown=calculateHeights(unknownWorker.message.input);
    unknown.assigned[1]=0;unknown.heights[1]=NaN;await unknownWorker.onmessage({data:unknown});
    const beforeUnknown=JSON.stringify(offsets.heights);
    element('heightOffsetDelta').value='2';await element('heightOffsetAdjust').onclick();
    assert.match(element('heightOffsetState').textContent,/Not adjusted:.*shows \?/);
    assert.equal(JSON.stringify(offsets.heights),beforeUnknown);
    // Display-default zero and negative results work; independent terrain is not doubled.
    context.worldSelection=new Map([['110:211',{x:110,y:211}]]);context.worldCell={x:110,y:211};
    element('heightOffsetDelta').value='-4';await element('heightOffsetAdjust').onclick();
    assert.equal(offsets.heights['light:110:211'],-4);
    offsets.terrain.push('light:110:211'); // Pixel detail doesn't change scalar -4.
    await run();await workers.at(-1).finish(); // refresh editor's authored snapshot
    element('heightOffsetDelta').value='2';await element('heightOffsetAdjust').onclick();
    assert.equal(offsets.offsets['light:110:211'],-2);assert.equal(offsets.heights['light:110:211'],-2);
    assert.deepEqual(workspace.light.previousArea,workers.at(-1).message.input.bounds);
    assert.equal(workspace.light.referenceHeight,'9');
    assert.deepEqual(workspace.light.reference,{x:101,y:201,world:'light'});
    assert.equal(workspace.dark.referenceHeight,'-4');
    // A new UI instance restores both worlds without calculation result arrays.
    const restoredElements=new Map();
    const restoredElement=id=>{
        if(!restoredElements.has(id))restoredElements.set(id,{value:'',checked:false,style:{},append(){},remove(){},addEventListener(event,handler){this[event]=handler;}});
        return restoredElements.get(id);
    };
    restoredElement('worldChoice').value='light';
    const restored={...context,window:{tileSemanticWorkspace:context.window.tileSemanticWorkspace},
        document:{...context.document,getElementById:restoredElement},
        update(){},save(){},renderDetail(){},renderGrid(){},changeRole(){},toggleNormal(){},scheduleSave(){}};
    restored.drawWorld=()=>{restored.window.experimentalHeightUI?.viewChanged();};
    vm.createContext(restored);
    vm.runInContext(fs.readFileSync(require.resolve('./alttp_tile_height_ui.js'),'utf8'),restored);
    await Promise.resolve();await Promise.resolve();
    assert.equal(restoredElement('heightReference').value,'9');
    assert.equal(restoredElement('heightRunPrevious').disabled,false);
    assert.match(restoredElement('heightAnchorInfo').textContent,/light reference: cell \(101, 201\)/);
    await restoredElement('heightRunPrevious').onclick();
    assert.deepEqual(workers.at(-1).message.input.bounds,workspace.light.previousArea);
    assert.equal(workers.at(-1).message.input.reference.height,9);
    await workers.at(-1).finish();
    restored.worldName='dark';restoredElement('worldChoice').value='dark';restored.drawWorld();
    assert.equal(restoredElement('heightReference').value,'-4');
    assert.equal(restoredElement('heightRunPrevious').disabled,false);
    await restoredElement('heightRunPrevious').onclick();
    assert.deepEqual(workers.at(-1).message.input.bounds,workspace.dark.previousArea);
    assert.equal(workers.at(-1).message.input.reference.height,-4);
    await workers.at(-1).finish();
    // All-instance saves include aliases and offscreen copies, with explicit consent.
    context.worldName='light';element('worldChoice').value='light';
    context.worldSelection=new Map([['100:200',{x:100,y:200}]]);context.worldCell={x:100,y:200};
    const oldRecord=context.worldRecord,oldManifest=context.worldManifest;
    context.worldManifest={content_ids:['other','bg','bg-alias']};
    context.worldRecord=(x,y)=>({id:x===100&&y===200?1:x===450&&y===450?2:0,flips:0});
    floor.aliases=['bg','bg-alias'];
    context.worldTarget=id=>id==='bg'||id==='bg-alias'?floor:null;
    element('heightOffsetAll').checked=true;element('heightOffset').value='-16';
    const prior=JSON.stringify(offsets),prompts=[];
    context.window.confirm=message=>{prompts.push(message);return false;};
    await element('heightOffsetSave').onclick();
    assert.equal(JSON.stringify(offsets),prior);assert.match(element('heightOffsetState').textContent,/Cancelled/);
    assert.match(prompts[0],/ALL 2 matching tile locations in the Light World/);
    context.window.confirm=message=>{prompts.push(message);return true;};
    await element('heightOffset').onkeydown({key:'Enter',preventDefault(){}});
    assert.equal(offsets.offsets['light:100:200'],-16);assert.equal(offsets.offsets['light:450:450'],-16);
    assert.equal(offsets.offsets['dark:450:450'],undefined);
    element('heightOffsetDelta').value='4';await element('heightOffsetAdjust').onclick();
    assert.equal(offsets.offsets['light:100:200'],-12);assert.equal(offsets.offsets['light:450:450'],-12);
    assert.match(prompts.at(-1),/Adjust each displayed height by \+4/);
    await element('heightOffsetRemove').onclick();
    assert.equal(offsets.offsets['light:100:200'],undefined);assert.equal(offsets.offsets['light:450:450'],undefined);
    assert.match(prompts.at(-1),/Remove authored scalar offsets/);
    element('heightOffsetAll').checked=false;
    context.worldRecord=oldRecord;context.worldManifest=oldManifest;
    // Explicit two-boundary gradient: preview/cancel are read-only; saves
    // replace existing offsets after explicit confirmation, even if all exist.
    const select=points=>{context.worldSelection=new Map(points.map(p=>[`${p.x}:${p.y}`,p]));context.worldCell=points.at(-1);};
    select([{x:10,y:10},{x:12,y:10}]);element('gradientA').onclick();
    select([{x:10,y:12},{x:12,y:12}]);element('gradientB').onclick();
    element('gradientHeightA').value='48';element('gradientHeightB').value='32';
    select([{x:11,y:11}]);element('heightOffset').value='99';await element('heightOffsetSave').onclick();
    const gradientBefore=JSON.stringify(offsets);
    element('gradientPreview').onclick();
    assert.equal(JSON.stringify(offsets),gradientBefore);
    assert.match(element('gradientStatus').textContent,/9 offsets; 1 existing offsets will be overwritten/);
    context.window.confirm=()=>false;await element('gradientSave').onclick();
    assert.equal(JSON.stringify(offsets),gradientBefore);
    context.window.confirm=message=>{assert.match(message,/1 existing scalar offsets will be overwritten/);return true;};await element('gradientSave').onclick();
    assert.equal(offsets.heights['light:10:10'],48);assert.equal(offsets.heights['light:10:12'],32);
    assert.equal(offsets.heights['light:10:11'],40);assert.equal(offsets.heights['light:11:11'],40);
    element('gradientPreview').onclick();
    assert.match(element('gradientStatus').textContent,/9 offsets; 9 existing offsets will be overwritten/);
    context.window.confirm=message=>{assert.match(message,/9 existing scalar offsets will be overwritten/);return true;};
    await element('gradientSave').onclick();assert.equal(offsets.heights['light:11:11'],40);
    element('gradientPreview').onclick();
    context.worldName='dark';await element('gradientSave').onclick();
    assert.match(element('gradientStatus').textContent,/stale/);assert.equal(offsets.heights['dark:11:11'],undefined);
    element('gradientPreview').onclick();assert.match(element('gradientStatus').textContent,/current world/);
    context.worldName='light';
    select([{x:11,y:11}]);element('gradientMask').checked=true;
    element('gradientPreview').onclick();assert.match(element('gradientStatus').textContent,/1 offsets/);
    element('gradientHeightA').oninput();assert.equal(element('gradientSave').disabled,true);
    element('gradientClear').onclick();assert.match(element('gradientStatus').textContent,/cleared/);
    // Per-pixel authoring is labeled as a range and visible without traversal.
    offsets.terrain_heights={'light:10:10':Array.from({length:64},(_,i)=>i)};
    offsets.terrain=['light:10:10'];offsets.nonuniform=['light:10:10'];
    await run();element('heightCancel').onclick();
    context.worldX=-240;context.worldY=-240;context.worldScale=3;
    element('worldCalculatedHeights').checked=true;element('worldHeight').checked=true;
    const pixelLabels=[];ui.drawHeightLabels(new Proxy({fillText:text=>pixelLabels.push(text)},{get:(o,k)=>o[k]||(()=>{})}));
    assert(pixelLabels.includes('48-111'));
    const imagesBefore=heightImages.length;ui.drawHeight(drawing);
    assert(heightImages.length>imagesBefore);assert(heightImages.at(-1)[0]<heightImages.at(-1)[63*4]);
    select([{x:10,y:10}]);
    // Bake/adjust use the base, not the minimum, maximum, or average pixel.
    const detailBefore=JSON.stringify(offsets.terrain_heights);
    await element('heightOffsetBake').onclick();assert.equal(offsets.offsets['light:10:10'],48);
    element('heightOffsetDelta').value='2';await element('heightOffsetAdjust').onclick();
    assert.equal(offsets.offsets['light:10:10'],50);
    assert.equal(JSON.stringify(offsets.terrain_heights),detailBefore);
    // Gradient writes also accept detailed tiles without folding in detail.
    select([{x:10,y:10},{x:12,y:10}]);element('gradientA').onclick();
    select([{x:10,y:12},{x:12,y:12}]);element('gradientB').onclick();
    element('gradientMask').checked=false;
    element('gradientPreview').onclick();assert.match(element('gradientStatus').textContent,/9 offsets/);
    context.window.confirm=()=>true;await element('gradientSave').onclick();
    assert.equal(offsets.offsets['light:10:10'],48);
    assert.equal(JSON.stringify(offsets.terrain_heights),detailBefore);
    select([{x:10,y:10}]);const pixelBefore=JSON.stringify(offsets);
    context.window.confirm=()=>false;await element('heightTerrainRemove').onclick();assert.equal(JSON.stringify(offsets),pixelBefore);
    context.window.confirm=()=>true;await element('heightTerrainRemove').onclick();
    assert.equal(offsets.terrain_heights['light:10:10'],undefined);assert.equal(offsets.offsets['light:10:10'],48);
    console.log('Height UI mock: snapshot, stale rejection, explicit references and world isolation passed.');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
