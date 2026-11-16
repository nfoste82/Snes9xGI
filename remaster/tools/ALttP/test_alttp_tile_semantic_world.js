/* Exercise real world pointer handlers without a browser or profile writes. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
async function main(){
    const elements=new Map();
    const element=id=>{if(!elements.has(id))elements.set(id,{style:{},parentNode:{append(){}},append(){},insertBefore(){},addEventListener(){},setPointerCapture(){},replaceChildren(){},getBoundingClientRect:()=>({left:0,top:0})});return elements.get(id);};
    const tile={content_id:'floor',aliases:['floor']};
    let reject=false,flushes=0;
    const context={console,document:{createElement:()=>element(Symbol()),getElementById:element,querySelector:element,body:element('body')},
        window:{experimentalHeightUI:{selectionChanging:async()=>{flushes++;if(reject)throw Error('Unsaved metadata');},selectionChanged(){}}},
        ResizeObserver:class{observe(){}},byId:new Map([['floor',tile]]),meta:{},selected:null,detailData:null,
        detail:element('detail'),select:async id=>{context.selected=id;}};
    vm.createContext(context);
    vm.runInContext(fs.readFileSync(require.resolve('./alttp_tile_semantic_world.js'),'utf8'),context);
    const mapHelp=vm.runInContext('worldPane.innerHTML',context).split('<details id="worldMapHelp"')[1].split('</details>')[0];
    assert(mapHelp.includes('id="worldInfo"'));assert(mapHelp.includes('id="worldLegend"'));
    assert(!mapHelp.includes('id="worldViewport"'));
    vm.runInContext(`worldManifest={content_ids:['floor'],counts:{light:{floor:262144}}};worldGrid=new DataView(new ArrayBuffer(512*512*8));worldScale=1;worldActive=true;drawWorld=()=>{};`,context);
    const vp=element('worldViewport');
    const event=(x,y,flags={})=>({button:0,pointerId:1,clientX:x*8+4,clientY:y*8+4,...flags});
    const cells=()=>JSON.parse(vm.runInContext('JSON.stringify([...worldSelection.keys()])',context));
    const click=async(x,y,flags={})=>{vp.onpointerdown(event(x,y,flags));await vp.onpointerup(event(x,y,flags));};
    const drag=async(from,to,flags,back=false)=>{
        vp.onpointerdown(event(...from,flags));vp.onpointermove(event(...to,flags));
        if(back)vp.onpointermove(event(...from,flags));
        await vp.onpointerup(event(...(back?from:to),flags));
    };
    await click(1,1);
    await drag([2,1],[5,1],{altKey:true});
    assert.deepEqual(cells(),['1:1']);assert.equal(vm.runInContext('worldX',context),0);
    await drag([2,1],[5,1],{altKey:true,metaKey:true},true);
    assert.deepEqual(cells(),['1:1','2:1','3:1','4:1','5:1']);
    assert.equal(vm.runInContext('worldLocationMode',context),true);
    assert.equal(vm.runInContext('worldX',context),0);
    // Revisiting within one stroke never toggles twice; a new stroke does.
    await drag([2,1],[5,1],{altKey:true,metaKey:true});assert.deepEqual(cells(),['1:1']);
    await click(1,1,{altKey:true,metaKey:true});assert.deepEqual(cells(),[]);
    await click(2,2);await click(4,3,{shiftKey:true});assert.equal(cells().length,6);
    await click(8,8,{metaKey:true});assert.equal(cells().length,7);
    await click(9,9,{metaKey:true,shiftKey:true});assert.equal(cells().length,10);
    // Normal drag still pans, without changing selection.
    await drag([10,10],[12,11],{});assert.equal(cells().length,10);
    assert.equal(vm.runInContext('worldX',context),16);
    vm.runInContext('worldX=worldY=0',context);
    const before=cells();reject=true;
    await drag([20,20],[23,20],{altKey:true,metaKey:true});
    assert.deepEqual(cells(),before);assert.match(element('worldInfo').textContent,/Unsaved metadata/);
    assert(flushes>0);
    reject=false;
    // Leaving map bounds never manufactures invalid selections.
    await drag([510,510],[514,510],{altKey:true,metaKey:true});
    assert(cells().includes('510:510'));assert(cells().includes('511:510'));assert(!cells().includes('512:510'));
    // Box filtering materializes all highlighted copies, including aliases,
    // without selecting unrelated artwork inside the same rectangle.
    context.byId.set('alias',tile);tile.aliases.push('alias');
    context.byId.set('other',{content_id:'other',aliases:['other']});
    vm.runInContext(`worldManifest.content_ids=['other','floor','alias'];
        worldGrid=new DataView(new ArrayBuffer(512*512*8));
        for(const [x,y,id] of [[10,10,1],[11,10,2],[12,10,1],[20,20,1],[21,20,2]])worldGrid.setUint32((y*512+x)*8,id,true);
        worldBoxMode.value='pan';`,context);
    await click(10,10);
    assert.equal(vm.runInContext('worldLocationMode',context),false);
    assert.deepEqual(cells(),['10:10']); // highlighting by type is not yet a location selection
    vm.runInContext("worldBoxMode.value='keep'",context);
    await drag([9,9],[13,11],{});
    assert.deepEqual(cells(),['10:10','11:10','12:10']);
    assert.equal(vm.runInContext('worldLocationMode',context),true);
    assert.equal(vm.runInContext('worldX',context),0);
    vm.runInContext("worldBoxMode.value='remove'",context);
    await drag([11,10],[12,10],{});assert.deepEqual(cells(),['10:10']);
    // A one-cell retained selection stays exact, rather than expanding by type.
    vm.runInContext("worldBoxMode.value='keep'",context);
    await drag([0,0],[30,30],{});assert.deepEqual(cells(),['10:10']);
    // Remove can also be the first operation on the type-highlighted copies.
    vm.runInContext("worldBoxMode.value='pan'",context);await click(10,10);
    vm.runInContext("worldBoxMode.value='remove'",context);
    await drag([13,11],[9,9],{});assert.deepEqual(cells(),['20:20','21:20']);
    reject=true;await drag([19,19],[22,22],{});assert.deepEqual(cells(),['20:20','21:20']);reject=false;
    vp.onpointerdown(event(19,19));vp.onpointermove(event(22,22));vp.onpointercancel();
    await vp.onpointerup(event(22,22));assert.deepEqual(cells(),['20:20','21:20']);
    await drag([19,19],[22,22],{});assert.deepEqual(cells(),[]);
    assert.equal(context.selected,null);
    // Empty exact selections do not silently re-expand to the last tile type.
    vm.runInContext("worldBoxMode.value='keep'",context);
    await drag([0,0],[511,511],{});assert.deepEqual(cells(),[]);
    // Reload a browser-local workspace and exercise real async map loading.
    const stored={tab:'world',world:'dark',worlds:{
        light:{view:{x:-800,y:-1600,scale:1},reference:{x:101,y:201},referenceHeight:'8',previousArea:{x0:100,y0:200,width:3,height:2}},
        dark:{view:{x:-3200,y:-6400,scale:4},reference:{x:102,y:202},referenceHeight:'-4',previousArea:{x0:100,y0:200,width:8,height:8}}
    }};
    let storage=JSON.stringify(stored);
    const reloaded={...context,grid:element('grid'),
        window:{localStorage:{getItem:()=>storage,setItem:(key,value)=>{storage=value;}}},
        fetch:async url=>({ok:true,json:async()=>({content_ids:['floor'],counts:{light:{floor:1},dark:{floor:1}}}),arrayBuffer:async()=>new ArrayBuffer(512*512*8)}),
        Image:class{set src(value){this.onload();}}};
    vm.createContext(reloaded);
    vm.runInContext(fs.readFileSync(require.resolve('./alttp_tile_semantic_world.js'),'utf8'),reloaded);
    // A canvas stub permits real drawWorld to persist view changes.
    const drawing=new Proxy({},{get:()=>()=>{}});
    element('worldCanvas').getContext=()=>drawing;
    element('worldViewport').getBoundingClientRect=()=>({left:0,top:0,width:800,height:600});
    for(let i=0;i<20;i++)await Promise.resolve();
    assert.equal(vm.runInContext('worldActive',reloaded),true);
    assert.equal(vm.runInContext('worldName',reloaded),'dark');
    assert.equal(vm.runInContext('worldScale',reloaded),4);
    assert.equal(vm.runInContext('worldX',reloaded),-3200);
    assert.equal(vm.runInContext('worldY',reloaded),-6400);
    assert.deepEqual(JSON.parse(storage).worlds.dark.reference,{...stored.worlds.dark.reference,world:'dark'});
    element('worldChoice').value='light';await element('worldChoice').onchange();
    assert.equal(vm.runInContext('worldScale',reloaded),1);
    assert.equal(vm.runInContext('worldX',reloaded),-800);
    assert.equal(JSON.parse(storage).world,'light');
    vm.runInContext('worldX=-900;worldY=-1700;worldScale=2;drawWorld();switcher.onclick();',reloaded);
    assert.equal(JSON.parse(storage).tab,'grid');
    assert.deepEqual(JSON.parse(storage).worlds.light.view,{x:-900,y:-1700,scale:2});
    assert.deepEqual(JSON.parse(storage).worlds.dark.view,stored.worlds.dark.view);
    // Malformed or unavailable storage must not prevent using the editor.
    storage='{bad json';
    assert.equal(vm.runInContext('readWorldWorkspace().tab',reloaded),'grid');
    reloaded.window.localStorage.getItem=()=>JSON.stringify({worlds:{light:{view:{scale:-1,x:0,y:0},reference:{x:999,y:0},previousArea:{x0:500,y0:0,width:20,height:1}}}});
    assert.equal(vm.runInContext('readWorldWorkspace().worlds.light.view',reloaded),undefined);
    assert.equal(vm.runInContext('readWorldWorkspace().worlds.light.reference',reloaded),undefined);
    assert.equal(vm.runInContext('readWorldWorkspace().worlds.light.previousArea',reloaded),undefined);
    reloaded.window.localStorage={getItem(){throw Error('blocked');},setItem(){throw Error('blocked');}};
    vm.runInContext('readWorldWorkspace();saveWorldWorkspace();',reloaded);
    console.log('World selection: Option no-pan, Cmd+Option brush, interpolation, deduplication, save safety and existing modifiers passed.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
