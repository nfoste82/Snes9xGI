#!/usr/bin/env python3
"""Build a self-contained, browsable ALTTP dungeon room gallery.

The ROM-derived images stay in the output directory. The HTML opens directly
from disk, without a server or an external service.
"""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib

from alttp_floor_rooms import inventory


def ppm_to_png(source: Path, target: Path) -> None:
    data = source.read_bytes()
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    if not match:
        raise ValueError(f"renderer did not write a P6 PPM: {source}")
    width, height = (int(group) for group in match.groups())
    pixels = data[match.end():]
    if (width, height) != (512, 512) or len(pixels) != width * height * 3:
        raise ValueError(f"unexpected room image size: {source}")

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(rows, 7)) + chunk(b"IEND", b""))
    target.write_bytes(png)


def render_rooms(report: dict, renderer: Path, output: Path) -> None:
    images = output / "images"
    images.mkdir(parents=True, exist_ok=True)
    for index, room in enumerate(report["rooms"], start=1):
        room_id = room["room_id"]
        name = f"room-{room_id:03x}"
        ppm = images / f"{name}.ppm"
        png = images / f"{name}.png"
        command = [str(renderer), hex(room_id), str(ppm)]
        result = subprocess.run(command, cwd=renderer.parent, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode:
            room["preview_error"] = result.stderr.strip()[-500:]
        else:
            try:
                ppm_to_png(ppm, png)
                room["image"] = f"images/{png.name}"
                info = dict(part.split("=", 1) for part in result.stdout.split())
                room["entrance_id"] = int(info["entrance"])
                room["theme_source_room"] = int(info["source_room"], 16)
                room["theme_id"] = int(info["theme"])
                room["theme_is_inferred"] = room["theme_source_room"] != room_id
                room["capture_theme_override"] = False
                room["scripted_preview"] = (int(info["main"]) != 7 or
                                            int(info["sub"]) != 0)
                room["preview_forced_lit"] = bool(int(info["forced_lit"]))
            except (ValueError, KeyError) as exc:
                room["preview_error"] = str(exc)
        ppm.unlink(missing_ok=True)
        if index % 40 == 0 or index == len(report["rooms"]):
            print(f"Rendered {index}/{len(report['rooms'])} rooms", flush=True)


def page(report: dict) -> str:
    data = json.dumps(report, separators=(",", ":")).replace("<", "\\u003c")
    rom_short = html.escape(report["rom_sha256"][:12])
    return r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ALTTP Room Atlas</title>
<style>
:root{color-scheme:dark;--bg:#11151b;--panel:#1b222b;--panel2:#222c37;--border:#364553;--text:#e8edf3;--muted:#aab7c5;--gold:#ead29b;--blue:#88cbe2;--green:#a5d5b3;--red:#e6a39e}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}button,input,textarea{font:inherit}button{cursor:pointer}header{padding:24px 28px 18px;background:#171d25;border-bottom:1px solid var(--border)}h1{font-size:26px;letter-spacing:.02em;margin:0 0 4px}h2{font-size:18px;margin:0 0 12px}.subtitle{color:var(--muted);margin:0}.app{display:grid;grid-template-columns:260px minmax(360px,1fr) 392px;min-height:calc(100vh - 97px)}aside{border-right:1px solid var(--border);padding:22px 18px;background:#171d25}aside h2{margin-bottom:17px}.field{margin-bottom:20px}.field label{display:block;margin:0 0 7px;color:var(--muted);font-size:13px;font-weight:650;text-transform:uppercase;letter-spacing:.06em}input[type=search],textarea{width:100%;background:#10161d;color:var(--text);border:1px solid var(--border);border-radius:8px;padding:10px 11px;outline:none}input:focus,textarea:focus{border-color:var(--blue)}.checks{display:grid;gap:10px}.check{display:flex;gap:9px;align-items:flex-start;line-height:1.3;cursor:pointer}.check input{accent-color:#a4c4dc;margin-top:2px}.hint{font-size:12px;line-height:1.45;color:var(--muted);margin:17px 0}.main{min-width:0;padding:19px 20px 30px}.toolbar{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:15px}.toolbar strong{color:var(--gold)}.smallbutton{background:var(--panel2);color:var(--text);border:1px solid var(--border);border-radius:7px;padding:6px 10px}.smallbutton:hover,.card:hover{border-color:#7797aa}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(190px,1fr));gap:15px}.card{border:1px solid var(--border);background:var(--panel);border-radius:11px;overflow:hidden;text-align:left;color:var(--text);padding:0;min-width:0}.card.selected{outline:2px solid var(--gold);border-color:var(--gold)}.thumb{display:block;width:100%;aspect-ratio:1;image-rendering:pixelated;object-fit:contain;background:#080a0d}.missing{aspect-ratio:1;display:grid;place-items:center;text-align:center;color:var(--muted);padding:15px}.cardbody{padding:10px 11px 12px}.cardtitle{display:flex;justify-content:space-between;align-items:center;font-weight:750;margin-bottom:5px}.state{font-size:11px;color:var(--green)}.state.review{color:var(--gold)}.chips{display:flex;gap:4px;flex-wrap:wrap;min-height:21px}.chip{border:1px solid #4d6371;border-radius:999px;color:#bcd0da;padding:2px 6px;font-size:10px;white-space:nowrap}.chip.dark{color:var(--gold);border-color:#8f7a4c}.chip.water{color:var(--blue);border-color:#528297}.chip.floor{color:var(--green);border-color:#5c8b6f}.detail{background:#171d25;border-left:1px solid var(--border);padding:20px 21px 30px;min-width:0}.detail img{width:100%;image-rendering:pixelated;border:1px solid var(--border);border-radius:7px;background:#080a0d}.detail h2{font-size:23px;margin:0 0 9px}.meta{display:grid;grid-template-columns:1fr auto;gap:6px 12px;margin:16px 0;color:var(--muted);font-size:13px}.meta b{color:var(--text);font-weight:650}.detail p{font-size:13px;color:var(--muted)}.reviewrow{display:flex;gap:6px;margin:15px 0}.reviewrow button{flex:1;background:var(--panel2);color:var(--text);border:1px solid var(--border);border-radius:7px;padding:8px 3px;font-size:12px}.reviewrow button.active{background:#384a41;border-color:var(--green);color:#d1f4db}.reviewrow button.problem.active{background:#513434;border-color:var(--red);color:#ffdfd9}.reviewrow button.skip.active{background:#47423b;border-color:var(--gold);color:#fff0cd}textarea{resize:vertical;min-height:84px}.note{font-size:12px;color:var(--muted);margin-top:6px}.empty{padding:35px;text-align:center;border:1px dashed var(--border);border-radius:10px;color:var(--muted)}@media(max-width:900px){.app{grid-template-columns:235px 1fr}.detail{grid-column:1/-1;border-left:0;border-top:1px solid var(--border)}.detail img{max-width:512px}}@media(max-width:690px){.app{display:block}aside{border-right:0;border-bottom:1px solid var(--border)}.checks{grid-template-columns:repeat(2,1fr)}.detail{border-top:1px solid var(--border)}header{padding:17px}}
body{height:100vh;display:flex;flex-direction:column;overflow:hidden}header{flex:none}.app{flex:1;min-height:0;overflow:hidden;grid-template-columns:260px minmax(320px,1fr) 10px minmax(280px,var(--detail-width,392px))}aside,.main,.detail{height:100%;overflow-y:auto;overscroll-behavior:contain}.detail-resizer{position:relative;height:100%;background:#171d25;border-left:1px solid var(--border);border-right:1px solid var(--border);cursor:col-resize;touch-action:none}.detail-resizer::after{content:"";position:absolute;left:2px;top:40%;width:4px;height:54px;border-radius:3px;background:#688091}.detail-resizer:hover,.detail-resizer:focus-visible{background:#314250;outline:0}.detail-resizer:hover::after,.detail-resizer:focus-visible::after{background:var(--gold)}body.resizing{cursor:col-resize;user-select:none}@media(max-width:900px){body{height:auto;display:block;overflow:auto}.app{height:auto;overflow:visible;grid-template-columns:235px minmax(0,1fr)}aside,.main,.detail{height:auto;overflow:visible}.detail-resizer{display:none}.detail{grid-column:1/-1;border-left:0;border-top:1px solid var(--border)}.detail img{max-width:512px}}@media(max-width:690px){.app{display:block}}
</style>
</head>
<body>
<header><h1>ALTTP Room Atlas</h1><p class="subtitle">Browse ROM room previews, lighting and level clues · USA ROM ''' + rom_short + r'''…</p></header>
<div class="app">
<aside><h2>Find rooms</h2><div class="field"><label for="search">Room ID or note</label><input id="search" type="search" placeholder="Try 0x061 or stairs"></div>
<div class="checks">
<label class="check"><input type="checkbox" data-filter="candidate">Height review candidates</label>
<label class="check"><input type="checkbox" data-filter="layer">BG2 layer mode</label>
<label class="check"><input type="checkbox" data-filter="mixed">Sprites on both levels</label>
<label class="check"><input type="checkbox" data-filter="stairs">Stairs</label>
<label class="check"><input type="checkbox" data-filter="water">Water related</label>
<label class="check"><input type="checkbox" data-filter="dark">Lights out</label>
<label class="check"><input type="checkbox" data-filter="validated">Height mask exists</label>
<label class="check"><input type="checkbox" data-filter="needs">Marked needs work</label>
</div><p class="hint">Filters combine. A water or height flag is a review clue, not proof that every tile in the room has that property.</p><button id="clear" class="smallbutton">Clear filters</button></aside>
<main class="main"><div class="toolbar"><div><strong id="count"></strong> rooms shown</div><button id="export" class="smallbutton">Export review notes</button></div><div id="grid" class="grid"></div></main>
<div id="detail-resizer" class="detail-resizer" role="separator" aria-orientation="vertical" aria-label="Resize room details" tabindex="0" title="Drag to resize room details; use arrow keys for smaller adjustments"></div>
<section id="detail" class="detail" aria-live="polite"></section>
</div>
<script id="rooms-data" type="application/json">''' + data + r'''</script>
<script>
const atlas=JSON.parse(document.getElementById('rooms-data').textContent);
const rooms=atlas.rooms;
const roomsById=new Map(rooms.map(r=>[r.room_id,r]));
const key='alttp-room-atlas-review-'+atlas.rom_sha256;
let reviews={};try{reviews=JSON.parse(localStorage.getItem(key)||'{}')}catch{}
const grid=document.getElementById('grid'),detail=document.getElementById('detail');
const search=document.getElementById('search');
const app=document.querySelector('.app'),resizer=document.getElementById('detail-resizer');
const widthKey='alttp-room-atlas-detail-width';
function widthLimit(){return Math.max(280,app.clientWidth-260-10-320)}
function setDetailWidth(width,persist=true){let size=Math.round(Math.min(widthLimit(),Math.max(280,width)));app.style.setProperty('--detail-width',size+'px');resizer.setAttribute('aria-valuemin','280');resizer.setAttribute('aria-valuemax',String(widthLimit()));resizer.setAttribute('aria-valuenow',String(size));if(persist){savedWidth=size;try{localStorage.setItem(widthKey,String(size))}catch{}}}
let savedWidth=392;try{savedWidth=Number(localStorage.getItem(widthKey))||392}catch{}
setDetailWidth(savedWidth,false);
resizer.addEventListener('pointerdown',event=>{if(event.button!==0)return;resizer.setPointerCapture(event.pointerId);document.body.classList.add('resizing');event.preventDefault()});
resizer.addEventListener('pointermove',event=>{if(resizer.hasPointerCapture(event.pointerId))setDetailWidth(app.getBoundingClientRect().right-event.clientX-5)});
for(let eventName of ['pointerup','pointercancel'])resizer.addEventListener(eventName,()=>document.body.classList.remove('resizing'));
resizer.addEventListener('keydown',event=>{if(event.key==='ArrowLeft'||event.key==='ArrowRight'){setDetailWidth(Number(resizer.getAttribute('aria-valuenow'))+(event.key==='ArrowLeft'?24:-24));event.preventDefault()}});
resizer.addEventListener('dblclick',()=>setDetailWidth(392));
window.addEventListener('resize',()=>setDetailWidth(savedWidth,false));
let selected=Number.parseInt(new URLSearchParams(location.search).get('room')||'61',16);
if(!roomsById.has(selected))selected=0x61;
function review(id){return reviews[id]||{status:'',note:''}}
function save(){try{localStorage.setItem(key,JSON.stringify(reviews))}catch{}}
function flags(){return new Set([...document.querySelectorAll('[data-filter]:checked')].map(x=>x.dataset.filter))}
function matches(r,f,q){let v=review(r.room_id);if(f.has('candidate')&&!r.evidence.length)return false;if(f.has('layer')&&!r.bg2_mode)return false;if(f.has('mixed')&&r.sprite_floors.length<2)return false;if(f.has('stairs')&&!r.in_room_stairs&&!r.between_room_stairs)return false;if(f.has('water')&&!r.water_related)return false;if(f.has('dark')&&!r.lights_out)return false;if(f.has('validated')&&!r.validated_height_mask)return false;if(f.has('needs')&&v.status!=='needs work')return false;if(!q)return true;let text=[r.room_hex,String(r.room_id),r.evidence.join(' '),v.note,v.status].join(' ').toLowerCase();return text.includes(q)}
function chips(r){let values=[];if(r.validated_height_mask)values.push(['Height mask','floor']);else if(r.evidence.length)values.push(['Height clue','floor']);if(r.lights_out)values.push(['Lights out','dark']);if(r.water_related)values.push(['Water','water']);if(r.bg2_mode)values.push(['BG2 mode '+r.bg2_mode,'']);if(r.sprite_floors.length===2)values.push(['2 combat levels','']);if(r.preview_forced_lit)values.push(['Lit preview','dark']);if(!r.object_count)values.push(['No room objects','']);if(r.scripted_preview)values.push(['Scripted state','']);return values.slice(0,5).map(([t,c])=>`<span class="chip ${c}">${t}</span>`).join('')}
function renderGrid(){let f=flags(),q=search.value.trim().toLowerCase();let visible=rooms.filter(r=>matches(r,f,q));document.getElementById('count').textContent=visible.length+' / '+rooms.length;grid.replaceChildren();if(!visible.length){let d=document.createElement('div');d.className='empty';d.textContent='No rooms match these filters.';grid.append(d);return}let fragment=document.createDocumentFragment();for(let r of visible){let card=document.createElement('button');card.className='card'+(r.room_id===selected?' selected':'');card.type='button';card.dataset.id=r.room_id;let status=review(r.room_id).status;card.innerHTML=(r.image?`<img class="thumb" loading="lazy" src="${r.image}" alt="Room ${r.room_hex} preview">`:`<div class="missing">Preview unavailable</div>`)+`<div class="cardbody"><div class="cardtitle"><span>${r.room_hex}</span><span class="state ${status==='needs work'?'review':''}">${status||''}</span></div><div class="chips">${chips(r)}</div></div>`;card.onclick=()=>select(r.room_id);fragment.append(card)}grid.append(fragment)}
function addMeta(container,label,value){let a=document.createElement('span');a.textContent=label;let b=document.createElement('b');b.textContent=value;container.append(a,b)}
function renderDetail(){let r=roomsById.get(selected);let v=review(selected);detail.replaceChildren();detail.scrollTop=0;let title=document.createElement('h2');title.textContent='Room '+r.room_hex;detail.append(title);if(r.image){let img=document.createElement('img');img.src=r.image;img.alt='Full 512 by 512 pixel room preview';detail.append(img)}else{let d=document.createElement('div');d.className='missing';d.textContent='Preview unavailable: '+(r.preview_error||'unknown error');detail.append(d)}let meta=document.createElement('div');meta.className='meta';addMeta(meta,'Height review',r.evidence.length?r.evidence.join(', ').replaceAll('_',' '):'No flagged clue');addMeta(meta,'BG2 mode / collision',r.bg2_mode+' / '+r.collision_mode);addMeta(meta,'ROM lights out',r.lights_out?'Yes':'No');addMeta(meta,'Preview lighting',r.preview_forced_lit?'Lights out disabled':'Normal');addMeta(meta,'Water clue',r.water_related?'Yes':'No');addMeta(meta,'Sprites on levels',r.sprite_floors.length?r.sprite_floors.join(', '):'None');addMeta(meta,'Room objects',String(r.object_count));addMeta(meta,'Object layers',Object.entries(r.layer_object_counts).map(([k,n])=>k+':'+n).join('  '));addMeta(meta,'Stairs',r.in_room_stairs+' in room, '+r.between_room_stairs+' between rooms');addMeta(meta,'Graphics entrance',r.entrance_id===undefined?'Unavailable':String(r.entrance_id));addMeta(meta,'Graphics theme',r.theme_id===undefined?'Unavailable':String(r.theme_id)+(r.theme_is_inferred?' (inferred)':''));addMeta(meta,'ROM room address',r.source_address);detail.append(meta);let p=document.createElement('p');p.textContent=r.capture_theme_override?'Preview uses the graphics context that resembles the supplied captures. It shows the reference engine’s default room state.':r.theme_is_inferred?'The graphics theme comes from a nearby entrance; compare colors and sprites with care. The room layout comes from the game engine.':'This preview uses a ROM entrance for the same room and the reference engine’s default state.';if(r.preview_forced_lit)p.textContent+=' The ROM lights-out effect is disabled in this preview so the room artwork is visible.';if(!r.object_count)p.textContent+=' This room record has no placed objects; its plain floor and walls come from the default layout.';if(r.scripted_preview)p.textContent+=' A scripted event or dialogue is active in this preview.';detail.append(p);let h=document.createElement('h2');h.textContent='Your review';detail.append(h);let buttons=document.createElement('div');buttons.className='reviewrow';for(let [value,label,kind] of [['verified','Looks right',''],['needs work','Needs work','problem'],['skip','Skip','skip']]){let b=document.createElement('button');b.textContent=label;b.className=kind+(v.status===value?' active':'');b.onclick=()=>{reviews[selected]={...review(selected),status:review(selected).status===value?'':value};save();renderDetail();renderGrid()};buttons.append(b)}detail.append(buttons);let note=document.createElement('textarea');note.placeholder='Add a note about floors, walls, water, lighting…';note.value=v.note||'';note.oninput=()=>{reviews[selected]={...review(selected),note:note.value};save()};detail.append(note);let n=document.createElement('div');n.className='note';n.textContent='Reviews stay in this browser on this computer when local storage is available. Export a copy to share or back up.';detail.append(n)}
function select(id){selected=id;history.replaceState(null,'','?room='+id.toString(16));renderGrid();renderDetail()}
search.oninput=renderGrid;document.querySelectorAll('[data-filter]').forEach(x=>x.onchange=renderGrid);document.getElementById('clear').onclick=()=>{search.value='';document.querySelectorAll('[data-filter]').forEach(x=>x.checked=false);renderGrid()};document.getElementById('export').onclick=()=>{let rows=Object.entries(reviews).filter(([,v])=>v.status||v.note);let data={rom_sha256:atlas.rom_sha256,exported_at:new Date().toISOString(),reviews:Object.fromEntries(rows)};let a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));a.download='alttp-room-reviews.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)};renderGrid();renderDetail();
</script></body></html>'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--atlas", type=Path, required=True)
    parser.add_argument("--renderer", type=Path, required=True,
                        help="headless alttp-room-preview built from Zelda3")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = inventory(args.atlas)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    render_rooms(report, args.renderer.resolve(), output)
    (output / "index.html").write_text(page(report))
    (output / "room-data.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Open {output / 'index.html'}")


if __name__ == "__main__":
    main()
