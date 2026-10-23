"""Self-contained catalog, truthful readiness, and browser issue-contract checks."""
import json
import hashlib
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

from alttp_overworld_review import build_data, generate, local_link, script_json


def fixture():
    pin = dict(rom_sha256='abc', area=27, world='light', state_key='base', metadata_revision='v1')
    p = dict(x=192, y=192, layer=0, surface=0, hash='</script><script>alert(1)</script>',
             map8_slot=369, map16=1135, map32=1509, raw_collision=1, collision=1,
             role='floor', status='solved', graphics_status='resolved',
             base=32, local=[0]*64, normals=[[0, 0, 1]]*64,
             assembly=1, geometry_source='authored', support=2)
    c = dict(pin=pin, placements=[p], revision='generator1', blockers=[],
             scope={'complete_gameplay_state': True})
    r = dict(pin=pin.copy(), coverage={'placements': 1}, ready_for_review=True,
             candidate_sha256=hashlib.sha256(json.dumps(c,sort_keys=True,separators=(',',':')).encode()).hexdigest(), complete_area_pixel_bounds=[1536,1536,1544,1544],
             required_viewports=[dict(x=1536,y=1536,width=8,height=8,capture=None)])
    return c, r


def png(width, height):
    def chunk(kind, value):
        return struct.pack('>I', len(value))+kind+value+struct.pack('>I', zlib.crc32(kind+value))
    return (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', width,height,8,2,0,0,0))
            +chunk(b'IDAT', zlib.compress((b'\0'+bytes(width*3))*height))+chunk(b'IEND', b''))


class CatalogTest(unittest.TestCase):
    def test_readiness_requires_full_geometry_and_checked_links(self):
        c, r = fixture()
        self.assertEqual(build_data(c,r)['status'], 'blocked')
        with tempfile.TemporaryDirectory() as tmp:
            packet = Path(tmp)/'linked.s9xrmf'
            packet.touch()
            regs = [dict(x=1536,y=1536,capture=str(packet),matching_checks_passed=True)]
            self.assertEqual(build_data(c,r,registrations=regs)['status'], 'ready for review')
            c['placements'][0]['status'] = 'classified'
            data = build_data(c,r,registrations=regs)
            self.assertEqual(data['status'], 'blocked')
            self.assertEqual(data['missing'], 1)
            c['pin']['area'] = 28
            with self.assertRaisesRegex(ValueError, 'pins differ'):
                build_data(c,r)

    def test_registration_paths_and_no_invented_links(self):
        c, r = fixture()
        self.assertIsNone(build_data(c,r)['viewports'][0]['capture'])
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'packet.s9xrmf').touch()
            (root/'open.command').touch()
            regs = [dict(x=1536,y=1536,capture='packet.s9xrmf',_base=root),
                    dict(x=1536,y=1536,launcher=str(root/'open.command'))]
            data = build_data(c,r,registrations=regs)
            self.assertTrue(data['viewports'][0]['linked'])
            self.assertEqual(data['viewports'][0]['launcher']['href'], (root/'open.command').resolve().as_uri())
            self.assertEqual(data['status'], 'blocked')  # existence is not validation
        with self.assertRaises(ValueError):
            local_link('javascript:alert(1)', Path.cwd())
        with self.assertRaises(ValueError):
            build_data(c,r,registrations=[dict(x=0,y=0,capture='no.s9xrmf')])

    def test_embedded_artifact_and_escaping(self):
        c, r = fixture()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cp,rp,art,out = [root/name for name in ('candidate.json','review.json','overview.png','index.html')]
            cp.write_text(json.dumps(c));rp.write_text(json.dumps(r));art.write_bytes(png(8,8))
            generate(cp,rp,out,art)
            html = out.read_text()
            payload = re.search(r'<script id="review-data" type="application/json">(.*?)</script>', html, re.S)[1]
            self.assertNotIn('</script>',payload)
            self.assertIn('data:image/png;base64,',payload)
            self.assertNotIn('/*__REVIEW_DATA__*/',html)
            self.assertEqual(len(json.loads(payload)['rows']),1)
            self.assertIn(c['placements'][0]['hash'], str(json.loads(payload)))
            self.assertNotRegex(html,r'<(?:script|link)[^>]+(?:src|href)=')
            art.write_bytes(png(16,8))
            with self.assertRaisesRegex(ValueError, 'bounds'):
                generate(cp,rp,out,art)

    @unittest.skipUnless(shutil.which('node'), 'Node needed for browser contract')
    def test_browser_rasters_and_revision_aware_issue_roundtrip(self):
        c,r=fixture()
        data=build_data(c,r)
        data['revision']='revision1'
        template=Path(__file__).with_name('alttp_overworld_review.html').read_text()
        script=re.findall(r'<script>(.*?)</script>',template,re.S)[0]
        # Execute the real browser script, with a tiny DOM/canvas and persistent storage.
        harness=r'''
const vm=require('node:vm'),assert=require('node:assert/strict');
const stored=new Map();
function boot(data){
 const els=new Map(),context={createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),
  putImageData(i){this.last=i},drawImage(){},clearRect(){},strokeRect(){},fillText(){},beginPath(){},arc(){},fill(){}};
 function element(id){return {id,value:'',checked:id==='grid',textContent:'',children:[],style:{},clientWidth:800,clientHeight:600,
  add(o){this.children.push(o);if(this.children.length===1)this.value=String(o.value)},
  append(...o){this.children.push(...o)},replaceChildren(){this.children=[]},
  getContext(){return context},scrollTo(){},getBoundingClientRect(){return {left:0,top:0}},setPointerCapture(){}}}
 const document={getElementById(id){if(!els.has(id))els.set(id,element(id));return els.get(id)},createElement:element};
 document.getElementById('review-data').textContent=JSON.stringify(data);
 document.getElementById('mode').value='height';
 const sandbox={document,Option:function(text,value){return {text,value}},Image:function(){},
  localStorage:{getItem:k=>stored.get(k),setItem:(k,v)=>stored.set(k,v)},console,Uint8ClampedArray};
 vm.createContext(sandbox);vm.runInContext(SCRIPT,sandbox);
 return {els,context,run:s=>vm.runInContext(s,sandbox)};
}
let app=boot(DATA);
assert.equal(app.context.last.data[0],0); // exact supplied base+local, min=max
app.els.get('mode').value='normal';app.els.get('mode').onchange();
assert.deepEqual(Array.from(app.context.last.data.slice(0,4)),[128,128,255,255]);
app.els.get('mode').value='occlusion';app.els.get('mode').onchange();
assert.deepEqual(Array.from(app.context.last.data.slice(0,4)),[238,48,160,255]); // missing, NOT opaque default
app.run('inspect(1537,1538,0,0)');
app.run("$('note').value='wall <script> note'");app.els.get('addIssue').onclick();
let saved=JSON.parse([...stored.values()][0]);
assert.equal(saved[0].scope.rom_sha256,'abc');assert.equal(saved[0].scope.state_key,'base');
assert.equal(saved[0].revision,'revision1');assert.equal(saved[0].mode,'occlusion');
assert.deepEqual(saved[0].map_position,{x:192,y:192,layer:0,surface:0});
assert.deepEqual(saved[0].pixel,{x:1537,y:1538});
DATA.revision='revision2';app=boot(DATA);
assert.match(app.els.get('issues').children[0].textContent,/STALE revision/);
app.els.get('issues').children[0].onclick();
assert.match(app.els.get('inspector').textContent,/1537/);
app.els.get('issues').children[1].onclick();
assert.equal(JSON.parse([...stored.values()][0])[0].status,'resolved');
'''
        harness='const DATA='+script_json(data)+';const SCRIPT='+json.dumps(script)+';\n'+harness
        result=subprocess.run(['node'],input=harness,text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)


if __name__=='__main__':
    unittest.main()
