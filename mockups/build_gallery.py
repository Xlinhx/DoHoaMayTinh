"""Offline mockup: the machine and minimal controls, with shared rendered assets."""
import hashlib
import json
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parent
source = ROOT / 'design-source.json'
design = json.loads(source.read_text(encoding='utf-8'))
report = json.loads((ROOT / 'reference-checks.json').read_text(encoding='utf-8'))
assert report['source_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest(), 'Rebuild Blender renders first.'
payload = json.dumps(design, ensure_ascii=False).replace('</', '<\\/')
palette = design['palette']
CSS = '''
:root{--paper:#e5e3dc;--ink:INK;--teal:TEAL;--orange:ORANGE;--cream:CREAM}
*{box-sizing:border-box}html,body{margin:0;height:100%;overflow:hidden;background:var(--paper)}body{font-family:'Segoe UI',sans-serif;color:var(--ink)}button{font:inherit;cursor:pointer;color:inherit}button:focus-visible,a:focus-visible{outline:3px solid var(--orange);outline-offset:4px}
.machine{position:fixed;inset:0;display:grid;place-items:center}.machine img{display:block;width:100%;height:100%;object-fit:contain}.controls{position:fixed;bottom:22px;left:50%;transform:translateX(-50%);display:flex;gap:5px;padding:6px;background:#fffdf6eb;border:1px solid #d4d9d0;border-radius:9px;max-width:calc(100% - 24px)}.controls button,.controls a{display:grid;place-items:center;width:39px;height:39px;flex:none;border:0;border-radius:5px;background:transparent;text-decoration:none}.controls button:hover,.controls a:hover{background:var(--cream)}.controls button.active{background:var(--teal);color:white}.controls svg{width:19px;height:19px;fill:none;stroke:currentColor;stroke-width:1.6;stroke-linecap:round;stroke-linejoin:round}.controls a{color:var(--ink)}.separator{height:24px;align-self:center;border-left:1px solid #d4d9d0;margin:0 3px}.hint{position:fixed;right:18px;top:16px;color:#536963;font-size:11px;opacity:.78;text-decoration:none}.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}
@media(max-width:600px){.controls{bottom:14px;gap:1px;padding:4px}.controls button,.controls a{width:33px;height:35px}.hint{font-size:10px;right:12px;top:10px}}
'''
for key, value in {'INK':palette['dark']['hex'],'TEAL':palette['teal']['hex'],'ORANGE':palette['orange']['hex'],'CREAM':palette['cream']['hex']}.items():
    CSS = CSS.replace(key,value)

ICONS = {
    'previous':'<path d="m14 5-7 7 7 7"/>',
    'next':'<path d="m10 5 7 7-7 7"/>',
    'reset':'<path d="M4 10a8 8 0 1 1 1 8M4 4v6h6"/>',
    'camera':'<path d="m4 7 4 0 2-3h4l2 3h4v13H4Z"/><circle cx="12" cy="13" r="4"/>',
    'machine':'<rect x="5" y="2" width="14" height="20" rx="2"/><path d="M5 16h14M9 6h6m-3 0v3m0 0-3 3m3-3 3 3M9 19h3"/>',
    'claw':'<path d="M12 2v6m0 0-6 5v6l3 2m3-13 6 5v6l-3 2m-3-13v10"/>',
    'play':'<path d="m9 5 10 7-10 7Z"/>',
    'pause':'<path d="M8 5v14M16 5v14"/>',
    'front':'<rect x="6" y="3" width="12" height="18" rx="1"/><path d="M6 16h12"/>',
    'side':'<path d="m7 5 9-2v18l-9-2Zm0 9 9 1"/>',
    'top':'<rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 15h16"/>'
}


def icon(name):
    return '<svg viewBox="0 0 24 24" aria-hidden="true">'+ICONS[name]+'</svg>'


def button(name, label, ident, active=False):
    return f'<button id="{ident}" title="{label}" aria-label="{label}" class="{"active" if active else ""}">{icon(name)}</button>'


def page(title, body, script):
    return f'''<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{title}</title><style>{CSS}</style></head><body>{body}<script id="design-data" type="application/json">{payload}</script><script>const D=JSON.parse(document.querySelector('#design-data').textContent);{script}</script></body></html>'''


gallery_body = '<main class="machine"><img id="stageImage" src="renders/01-hero.png" alt="Máy gắp thú, góc chéo"></main>'
gallery_body += '<p id="poseLabel" class="sr-only" aria-live="polite">Tổng thể</p><nav class="controls" aria-label="Góc nhìn và tư thế">'
gallery_body += button('previous','Góc trước đó','previous')
gallery_body += button('camera','Góc chéo','hero',True)
gallery_body += button('front','Chính diện','front')
gallery_body += button('side','Bên hông','side')
gallery_body += button('top','Nhìn từ trên','top')
gallery_body += button('claw','Cận cảnh càng','claw')
gallery_body += button('next','Góc tiếp theo','next')
gallery_body += '<span class="separator"></span><a href="application-mockup.html" title="Xem chu trình gắp" aria-label="Xem chu trình gắp">'+icon('play')+'</a></nav>'
gallery_script = '''
let current=0;
function selectShot(index){current=(index+D.shots.length)%D.shots.length;const s=D.shots[current];document.querySelector('#stageImage').src='renders/'+s.id+'.png';document.querySelector('#stageImage').alt=s.title;document.querySelector('#poseLabel').textContent=s.title;document.querySelectorAll('.controls button').forEach(b=>b.classList.toggle('active',Number(b.dataset.shot)===current));}
const presets={hero:'01-hero',front:'02-front',side:'03-side',top:'05-top',claw:'08-claw-grip'};Object.entries(presets).forEach(([id,shot])=>{const b=document.getElementById(id);b.dataset.shot=D.shots.findIndex(s=>s.id===shot);b.onclick=()=>selectShot(Number(b.dataset.shot));});document.querySelector('#previous').onclick=()=>selectShot(current-1);document.querySelector('#next').onclick=()=>selectShot(current+1);document.addEventListener('keydown',e=>{if(e.key==='ArrowLeft'){e.preventDefault();selectShot(current-1);}if(e.key==='ArrowRight'){e.preventDefault();selectShot(current+1);}});selectShot(0);
'''

app_body = '<main class="machine"><img id="appImage" src="renders/10-aim.png" alt="Máy gắp thú"></main>'
app_body += '<a class="hint" href="index.html">Bản xem trước · ảnh tĩnh</a><p id="poseLabel" class="sr-only" aria-live="polite"></p>'
app_body += '<nav class="controls" aria-label="Xem trước chu trình gắp">'
app_body += button('previous','Bước trước','previous')
app_body += button('play','Chạy chu trình xem trước','play')
app_body += button('next','Bước tiếp theo','next')
app_body += '<span class="separator"></span>'+button('reset','Về vị trí đầu','reset')
app_body += '<a href="index.html" title="Đổi góc nhìn" aria-label="Đổi góc nhìn">'+icon('camera')+'</a></nav>'
app_script = '''
const sequence=['aim','lower','grip','lift','return','release','result'];let active=0,timer=null;
function setState(index){active=(index+sequence.length)%sequence.length;const state=sequence[active];const shot=D.shots.find(s=>s.group==='states'&&s.state===state);document.querySelector('#appImage').src='renders/'+shot.id+'.png';document.querySelector('#appImage').alt=shot.title;document.querySelector('#poseLabel').textContent=shot.title;}
function stop(){if(timer!==null){clearInterval(timer);timer=null;}document.querySelector('#play').classList.remove('active');document.querySelector('#play').setAttribute('aria-pressed','false');}
document.querySelector('#play').onclick=()=>{if(timer!==null){stop();return;}if(active===sequence.length-1)setState(0);document.querySelector('#play').classList.add('active');document.querySelector('#play').setAttribute('aria-pressed','true');timer=setInterval(()=>{setState(active+1);if(active===sequence.length-1)stop();},1100);};
document.querySelector('#previous').onclick=()=>{stop();setState(active-1);};document.querySelector('#next').onclick=()=>{stop();setState(active+1);};document.querySelector('#reset').onclick=()=>{stop();setState(0);};document.addEventListener('keydown',e=>{if(e.key==='ArrowRight'){e.preventDefault();stop();setState(active+1);}if(e.key==='ArrowLeft'){e.preventDefault();stop();setState(active-1);}if(e.key===' '){e.preventDefault();document.querySelector('#play').click();}});const params=new URLSearchParams(location.search);setState(Math.max(0,sequence.indexOf(params.get('state')||'aim')));
'''
(ROOT/'index.html').write_text(page('CLAW LAB — Máy gắp thú',gallery_body,gallery_script),encoding='utf-8')
(ROOT/'application-mockup.html').write_text(page('CLAW LAB — Chu trình gắp',app_body,app_script),encoding='utf-8')
runpy.run_path(str(ROOT/'build_drawings.py'))
print(json.dumps({'pages':['index.html','application-mockup.html'],'renders':len(design['shots']),'scope':'machine, camera and grab cycle only'},ensure_ascii=False))
