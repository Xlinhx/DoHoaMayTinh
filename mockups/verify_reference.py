"""Focused offline checks for this review package. Does not certify the game."""
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
source = ROOT / 'design-source.json'
design = json.loads(source.read_text(encoding='utf-8'))
report = json.loads((ROOT / 'reference-checks.json').read_text(encoding='utf-8'))
assert report['source_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest(), 'stale Blender output: rebuild renders'
expected = {shot['id']+'.png' for shot in design['shots']}
actual = {p.name for p in (ROOT/'renders').glob('*.png')}
assert actual == expected, ('missing/extra renders', expected-actual, actual-expected)
for path in (ROOT/'renders').glob('*.png'):
    raw = path.read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II',raw[16:24]) == (1400,1400), path.name
    assert hashlib.sha256(raw).hexdigest() == report['render_sha256'][path.name], ('changed render',path.name)

pages = ['index.html','application-mockup.html']
for name in pages:
    markup = (ROOT/name).read_text(encoding='utf-8')
    embedded = re.search(r'<script id="design-data" type="application/json">(.*?)</script>',markup,re.S).group(1)
    assert json.loads(embedded.replace('<\\/','</')) == design, ('UI source drift',name)
    js = re.findall(r'<script[^>]*>(.*?)</script>',markup,re.S)[-1]
    checked = subprocess.run(['node','--check','-'],input=js,text=True,capture_output=True,encoding='utf-8')
    assert checked.returncode == 0, (name,checked.stderr)
    static_markup = re.sub(r'<script[^>]*>.*?</script>','',markup,flags=re.S)
    for link in re.findall(r'(?:src|href)="([^"]+)"',static_markup):
        if link.startswith(('http:','https:','data:','#')):
            continue
        file = link.split('?',1)[0].split('#',1)[0]
        assert (ROOT/file).is_file(), ('broken local link',name,file)
    assert not re.search(r'Explain|Blinn|M_world|Cohen|Bresenham|explain-panel|<h1|<h2', markup), ('removed presentation UI',name)
for shot in ['02-front','03-side','05-top']:
    svg=(ROOT/f'{shot}-dimensions.svg').read_text(encoding='utf-8')
    assert 'data:image/png;base64,' in svg, ('drawing must embed the existing shared render',shot)
    assert '1600 1600' in svg
assert design['prize_asset']['instance_count'] == len(design['prizes']) == 8
assert len({p['id'] for p in design['prizes']}) == 8
assert design['prize_asset']['target_id'] in {p['id'] for p in design['prizes']}
assert report['measurements']['claw_envelope_poses'] == 32
assert report['measurements']['minimum_sampled_glass_clearance_m'] > 0
print(json.dumps({'result':'PASS for reference package only','renders':len(expected),'size':[1400,1400],
                  'pages_with_identical_manifest':len(pages),'annotated_drawings':3,
                  'blender_checks':report['checks'],'game_runtime':'not checked by this reference-only command; use tools/verify_runtime.py'},ensure_ascii=False,indent=2))
